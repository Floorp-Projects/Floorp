/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JS bytecode generation.
 */
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <new>
#include <string.h>

#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsfun.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsautooplen.h"        // generated headers last

#include "ds/LifoAlloc.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/BytecodeEmitter.h"
#include "frontend/Parser.h"
#include "frontend/TokenStream.h"
#include "vm/RegExpObject.h"

#include "jsatominlines.h"
#include "jsscopeinlines.h"
#include "jsscriptinlines.h"

#include "frontend/BytecodeEmitter-inl.h"
#include "frontend/ParseMaps-inl.h"

/* Allocation chunk counts, must be powers of two in general. */
#define BYTECODE_CHUNK_LENGTH  1024    /* initial bytecode chunk length */
#define SRCNOTE_CHUNK_LENGTH   1024    /* initial srcnote chunk length */

/* Macros to compute byte sizes from typed element counts. */
#define BYTECODE_SIZE(n)        ((n) * sizeof(jsbytecode))
#define SRCNOTE_SIZE(n)         ((n) * sizeof(jssrcnote))

using namespace js;
using namespace js::gc;
using namespace js::frontend;

static JSBool
NewTryNote(JSContext *cx, BytecodeEmitter *bce, JSTryNoteKind kind, uintN stackDepth,
           size_t start, size_t end);

static bool
EmitIndexOp(JSContext *cx, JSOp op, uintN index, BytecodeEmitter *bce, JSOp *psuffix = NULL);

static JSBool
EmitLeaveBlock(JSContext *cx, BytecodeEmitter *bce, JSOp op, ObjectBox *box);

static JSBool
SetSrcNoteOffset(JSContext *cx, BytecodeEmitter *bce, uintN index, uintN which, ptrdiff_t offset);

void
TreeContext::trace(JSTracer *trc)
{
    bindings.trace(trc);
}

BytecodeEmitter::BytecodeEmitter(Parser *parser, uintN lineno)
  : TreeContext(parser),
    atomIndices(parser->context),
    stackDepth(0), maxStackDepth(0),
    ntrynotes(0), lastTryNode(NULL),
    spanDeps(NULL), jumpTargets(NULL), jtFreeList(NULL),
    numSpanDeps(0), numJumpTargets(0), spanDepTodo(0),
    arrayCompDepth(0),
    emitLevel(0),
    constMap(parser->context),
    constList(parser->context),
    upvarIndices(parser->context),
    upvarMap(parser->context),
    globalScope(NULL),
    globalUses(parser->context),
    globalMap(parser->context),
    closedArgs(parser->context),
    closedVars(parser->context),
    traceIndex(0),
    typesetCount(0)
{
    flags = TCF_COMPILING;
    memset(&prolog, 0, sizeof prolog);
    memset(&main, 0, sizeof main);
    current = &main;
    firstLine = prolog.currentLine = main.currentLine = lineno;
}

bool
BytecodeEmitter::init(JSContext *cx, TreeContext::InitBehavior ib)
{
    roLexdeps.init();
    return TreeContext::init(cx, ib) && constMap.init() && atomIndices.ensureMap(cx);
}

BytecodeEmitter::~BytecodeEmitter()
{
    JSContext *cx = parser->context;

    cx->free_(prolog.base);
    cx->free_(prolog.notes);
    cx->free_(main.base);
    cx->free_(main.notes);

    /* NB: non-null only after OOM. */
    if (spanDeps)
        cx->free_(spanDeps);
}

static ptrdiff_t
EmitCheck(JSContext *cx, BytecodeEmitter *bce, ptrdiff_t delta)
{
    jsbytecode *base = bce->base();
    jsbytecode *newbase;
    jsbytecode *next = bce->next();
    jsbytecode *limit = bce->limit();
    ptrdiff_t offset = next - base;
    size_t minlength = offset + delta;

    if (next + delta > limit) {
        size_t newlength;
        if (!base) {
            JS_ASSERT(!next && !limit);
            newlength = BYTECODE_CHUNK_LENGTH;
            if (newlength < minlength)     /* make it bigger if necessary */
                newlength = RoundUpPow2(minlength);
            newbase = (jsbytecode *) cx->malloc_(BYTECODE_SIZE(newlength));
        } else {
            JS_ASSERT(base <= next && next <= limit);
            newlength = (limit - base) * 2;
            if (newlength < minlength)     /* make it bigger if necessary */
                newlength = RoundUpPow2(minlength);
            newbase = (jsbytecode *) cx->realloc_(base, BYTECODE_SIZE(newlength));
        }
        if (!newbase) {
            js_ReportOutOfMemory(cx);
            return -1;
        }
        JS_ASSERT(newlength >= size_t(offset + delta));
        bce->current->base = newbase;
        bce->current->limit = newbase + newlength;
        bce->current->next = newbase + offset;
    }
    return offset;
}

static void
UpdateDepth(JSContext *cx, BytecodeEmitter *bce, ptrdiff_t target)
{
    jsbytecode *pc;
    JSOp op;
    const JSCodeSpec *cs;
    uintN nuses;
    intN ndefs;

    pc = bce->code(target);
    op = (JSOp) *pc;
    cs = &js_CodeSpec[op];
    if ((cs->format & JOF_TMPSLOT_MASK)) {
        uintN depth = (uintN) bce->stackDepth +
                      ((cs->format & JOF_TMPSLOT_MASK) >> JOF_TMPSLOT_SHIFT);
        if (depth > bce->maxStackDepth)
            bce->maxStackDepth = depth;
    }

    nuses = js_GetStackUses(cs, op, pc);
    bce->stackDepth -= nuses;
    JS_ASSERT(bce->stackDepth >= 0);
    if (bce->stackDepth < 0) {
        char numBuf[12];
        TokenStream *ts;

        JS_snprintf(numBuf, sizeof numBuf, "%d", target);
        ts = &bce->parser->tokenStream;
        JS_ReportErrorFlagsAndNumber(cx, JSREPORT_WARNING,
                                     js_GetErrorMessage, NULL,
                                     JSMSG_STACK_UNDERFLOW,
                                     ts->getFilename() ? ts->getFilename() : "stdin",
                                     numBuf);
    }
    ndefs = cs->ndefs;
    if (ndefs < 0) {
        JSObject *blockObj;

        /* We just executed IndexParsedObject */
        JS_ASSERT(op == JSOP_ENTERBLOCK);
        JS_ASSERT(nuses == 0);
        blockObj = bce->objectList.lastbox->object;
        JS_ASSERT(blockObj->isStaticBlock());
        JS_ASSERT(blockObj->getSlot(JSSLOT_BLOCK_DEPTH).isUndefined());

        OBJ_SET_BLOCK_DEPTH(cx, blockObj, bce->stackDepth);
        ndefs = OBJ_BLOCK_COUNT(cx, blockObj);
    }
    bce->stackDepth += ndefs;
    if ((uintN)bce->stackDepth > bce->maxStackDepth)
        bce->maxStackDepth = bce->stackDepth;
}

static inline void
UpdateDecomposeLength(BytecodeEmitter *bce, uintN start)
{
    uintN end = bce->offset();
    JS_ASSERT(uintN(end - start) < 256);
    bce->code(start)[-1] = end - start;
}

ptrdiff_t
frontend::Emit1(JSContext *cx, BytecodeEmitter *bce, JSOp op)
{
    ptrdiff_t offset = EmitCheck(cx, bce, 1);

    if (offset >= 0) {
        *bce->current->next++ = (jsbytecode)op;
        UpdateDepth(cx, bce, offset);
    }
    return offset;
}

ptrdiff_t
frontend::Emit2(JSContext *cx, BytecodeEmitter *bce, JSOp op, jsbytecode op1)
{
    ptrdiff_t offset = EmitCheck(cx, bce, 2);

    if (offset >= 0) {
        jsbytecode *next = bce->next();
        next[0] = (jsbytecode)op;
        next[1] = op1;
        bce->current->next = next + 2;
        UpdateDepth(cx, bce, offset);
    }
    return offset;
}

ptrdiff_t
frontend::Emit3(JSContext *cx, BytecodeEmitter *bce, JSOp op, jsbytecode op1,
                    jsbytecode op2)
{
    ptrdiff_t offset = EmitCheck(cx, bce, 3);

    if (offset >= 0) {
        jsbytecode *next = bce->next();
        next[0] = (jsbytecode)op;
        next[1] = op1;
        next[2] = op2;
        bce->current->next = next + 3;
        UpdateDepth(cx, bce, offset);
    }
    return offset;
}

ptrdiff_t
frontend::Emit5(JSContext *cx, BytecodeEmitter *bce, JSOp op, uint16 op1, uint16 op2)
{
    ptrdiff_t offset = EmitCheck(cx, bce, 5);

    if (offset >= 0) {
        jsbytecode *next = bce->next();
        next[0] = (jsbytecode)op;
        next[1] = UINT16_HI(op1);
        next[2] = UINT16_LO(op1);
        next[3] = UINT16_HI(op2);
        next[4] = UINT16_LO(op2);
        bce->current->next = next + 5;
        UpdateDepth(cx, bce, offset);
    }
    return offset;
}

ptrdiff_t
frontend::EmitN(JSContext *cx, BytecodeEmitter *bce, JSOp op, size_t extra)
{
    ptrdiff_t length = 1 + (ptrdiff_t)extra;
    ptrdiff_t offset = EmitCheck(cx, bce, length);

    if (offset >= 0) {
        jsbytecode *next = bce->next();
        *next = (jsbytecode)op;
        memset(next + 1, 0, BYTECODE_SIZE(extra));
        bce->current->next = next + length;

        /*
         * Don't UpdateDepth if op's use-count comes from the immediate
         * operand yet to be stored in the extra bytes after op.
         */
        if (js_CodeSpec[op].nuses >= 0)
            UpdateDepth(cx, bce, offset);
    }
    return offset;
}

/* XXX too many "... statement" L10N gaffes below -- fix via js.msg! */
const char js_with_statement_str[] = "with statement";
const char js_finally_block_str[]  = "finally block";
const char js_script_str[]         = "script";

static const char *statementName[] = {
    "label statement",       /* LABEL */
    "if statement",          /* IF */
    "else statement",        /* ELSE */
    "destructuring body",    /* BODY */
    "switch statement",      /* SWITCH */
    "block",                 /* BLOCK */
    js_with_statement_str,   /* WITH */
    "catch block",           /* CATCH */
    "try block",             /* TRY */
    js_finally_block_str,    /* FINALLY */
    js_finally_block_str,    /* SUBROUTINE */
    "do loop",               /* DO_LOOP */
    "for loop",              /* FOR_LOOP */
    "for/in loop",           /* FOR_IN_LOOP */
    "while loop",            /* WHILE_LOOP */
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(statementName) == STMT_LIMIT);

static const char *
StatementName(BytecodeEmitter *bce)
{
    if (!bce->topStmt)
        return js_script_str;
    return statementName[bce->topStmt->type];
}

static void
ReportStatementTooLarge(JSContext *cx, BytecodeEmitter *bce)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NEED_DIET,
                         StatementName(bce));
}

/**
  Span-dependent instructions in JS bytecode consist of the jump (JOF_JUMP)
  and switch (JOF_LOOKUPSWITCH, JOF_TABLESWITCH) format opcodes, subdivided
  into unconditional (gotos and gosubs), and conditional jumps or branches
  (which pop a value, test it, and jump depending on its value).  Most jumps
  have just one immediate operand, a signed offset from the jump opcode's pc
  to the target bytecode.  The lookup and table switch opcodes may contain
  many jump offsets.

  Mozilla bug #80981 (http://bugzilla.mozilla.org/show_bug.cgi?id=80981) was
  fixed by adding extended "X" counterparts to the opcodes/formats (NB: X is
  suffixed to prefer JSOP_ORX thereby avoiding a JSOP_XOR name collision for
  the extended form of the JSOP_OR branch opcode).  The unextended or short
  formats have 16-bit signed immediate offset operands, the extended or long
  formats have 32-bit signed immediates.  The span-dependency problem consists
  of selecting as few long instructions as possible, or about as few -- since
  jumps can span other jumps, extending one jump may cause another to need to
  be extended.

  Most JS scripts are short, so need no extended jumps.  We optimize for this
  case by generating short jumps until we know a long jump is needed.  After
  that point, we keep generating short jumps, but each jump's 16-bit immediate
  offset operand is actually an unsigned index into bce->spanDeps, an array of
  js::SpanDep structs.  Each struct tells the top offset in the script of the
  opcode, the "before" offset of the jump (which will be the same as top for
  simplex jumps, but which will index further into the bytecode array for a
  non-initial jump offset in a lookup or table switch), the after "offset"
  adjusted during span-dependent instruction selection (initially the same
  value as the "before" offset), and the jump target (more below).

  Since we generate bce->spanDeps lazily, from within SetJumpOffset, we must
  ensure that all bytecode generated so far can be inspected to discover where
  the jump offset immediate operands lie within bce->main. But the bonus is
  that we generate span-dependency records sorted by their offsets, so we can
  binary-search when trying to find a SpanDep for a given bytecode offset, or
  the nearest SpanDep at or above a given pc.

  To avoid limiting scripts to 64K jumps, if the bce->spanDeps index overflows
  65534, we store SPANDEP_INDEX_HUGE in the jump's immediate operand.  This
  tells us that we need to binary-search for the bce->spanDeps entry by the
  jump opcode's bytecode offset (sd->before).

  Jump targets need to be maintained in a data structure that lets us look
  up an already-known target by its address (jumps may have a common target),
  and that also lets us update the addresses (script-relative, a.k.a. absolute
  offsets) of targets that come after a jump target (for when a jump below
  that target needs to be extended).  We use an AVL tree, implemented using
  recursion, but with some tricky optimizations to its height-balancing code
  (see http://www.cmcrossroads.com/bradapp/ftp/src/libs/C++/AvlTrees.html).

  A final wrinkle: backpatch chains are linked by jump-to-jump offsets with
  positive sign, even though they link "backward" (i.e., toward lower bytecode
  address).  We don't want to waste space and search time in the AVL tree for
  such temporary backpatch deltas, so we use a single-bit wildcard scheme to
  tag true JumpTarget pointers and encode untagged, signed (positive) deltas in
  SpanDep::target pointers, depending on whether the SpanDep has a known
  target, or is still awaiting backpatching.

  Note that backpatch chains would present a problem for BuildSpanDepTable,
  which inspects bytecode to build bce->spanDeps on demand, when the first
  short jump offset overflows.  To solve this temporary problem, we emit a
  proxy bytecode (JSOP_BACKPATCH; JSOP_BACKPATCH_POP for branch ops) whose
  nuses/ndefs counts help keep the stack balanced, but whose opcode format
  distinguishes its backpatch delta immediate operand from a normal jump
  offset.
 */
static int
BalanceJumpTargets(JumpTarget **jtp)
{
    JumpTarget *jt = *jtp;
    JS_ASSERT(jt->balance != 0);

    int dir;
    JSBool doubleRotate;
    if (jt->balance < -1) {
        dir = JT_RIGHT;
        doubleRotate = (jt->kids[JT_LEFT]->balance > 0);
    } else if (jt->balance > 1) {
        dir = JT_LEFT;
        doubleRotate = (jt->kids[JT_RIGHT]->balance < 0);
    } else {
        return 0;
    }

    int otherDir = JT_OTHER_DIR(dir);
    JumpTarget *root;
    int heightChanged;
    if (doubleRotate) {
        JumpTarget *jt2 = jt->kids[otherDir];
        *jtp = root = jt2->kids[dir];

        jt->kids[otherDir] = root->kids[dir];
        root->kids[dir] = jt;

        jt2->kids[dir] = root->kids[otherDir];
        root->kids[otherDir] = jt2;

        heightChanged = 1;
        root->kids[JT_LEFT]->balance = -JS_MAX(root->balance, 0);
        root->kids[JT_RIGHT]->balance = -JS_MIN(root->balance, 0);
        root->balance = 0;
    } else {
        *jtp = root = jt->kids[otherDir];
        jt->kids[otherDir] = root->kids[dir];
        root->kids[dir] = jt;

        heightChanged = (root->balance != 0);
        jt->balance = -((dir == JT_LEFT) ? --root->balance : ++root->balance);
    }

    return heightChanged;
}

struct AddJumpTargetArgs {
    JSContext           *cx;
    BytecodeEmitter     *bce;
    ptrdiff_t           offset;
    JumpTarget          *node;
};

static int
AddJumpTarget(AddJumpTargetArgs *args, JumpTarget **jtp)
{
    JumpTarget *jt = *jtp;
    if (!jt) {
        BytecodeEmitter *bce = args->bce;

        jt = bce->jtFreeList;
        if (jt) {
            bce->jtFreeList = jt->kids[JT_LEFT];
        } else {
            jt = args->cx->tempLifoAlloc().new_<JumpTarget>();
            if (!jt) {
                js_ReportOutOfMemory(args->cx);
                return 0;
            }
        }
        jt->offset = args->offset;
        jt->balance = 0;
        jt->kids[JT_LEFT] = jt->kids[JT_RIGHT] = NULL;
        bce->numJumpTargets++;
        args->node = jt;
        *jtp = jt;
        return 1;
    }

    if (jt->offset == args->offset) {
        args->node = jt;
        return 0;
    }

    int balanceDelta;
    if (args->offset < jt->offset)
        balanceDelta = -AddJumpTarget(args, &jt->kids[JT_LEFT]);
    else
        balanceDelta = AddJumpTarget(args, &jt->kids[JT_RIGHT]);
    if (!args->node)
        return 0;

    jt->balance += balanceDelta;
    return (balanceDelta && jt->balance)
           ? 1 - BalanceJumpTargets(jtp)
           : 0;
}

#ifdef DEBUG_brendan
static int
AVLCheck(JumpTarget *jt)
{
    int lh, rh;

    if (!jt) return 0;
    JS_ASSERT(-1 <= jt->balance && jt->balance <= 1);
    lh = AVLCheck(jt->kids[JT_LEFT]);
    rh = AVLCheck(jt->kids[JT_RIGHT]);
    JS_ASSERT(jt->balance == rh - lh);
    return 1 + JS_MAX(lh, rh);
}
#endif

static JSBool
SetSpanDepTarget(JSContext *cx, BytecodeEmitter *bce, SpanDep *sd, ptrdiff_t off)
{
    AddJumpTargetArgs args;

    if (off < JUMPX_OFFSET_MIN || JUMPX_OFFSET_MAX < off) {
        ReportStatementTooLarge(cx, bce);
        return JS_FALSE;
    }

    args.cx = cx;
    args.bce = bce;
    args.offset = sd->top + off;
    args.node = NULL;
    AddJumpTarget(&args, &bce->jumpTargets);
    if (!args.node)
        return JS_FALSE;

#ifdef DEBUG_brendan
    AVLCheck(bce->jumpTargets);
#endif

    SD_SET_TARGET(sd, args.node);
    return JS_TRUE;
}

#define SPANDEPS_MIN            256
#define SPANDEPS_SIZE(n)        ((n) * sizeof(js::SpanDep))
#define SPANDEPS_SIZE_MIN       SPANDEPS_SIZE(SPANDEPS_MIN)

static JSBool
AddSpanDep(JSContext *cx, BytecodeEmitter *bce, jsbytecode *pc, jsbytecode *pc2, ptrdiff_t off)
{
    uintN index = bce->numSpanDeps;
    if (index + 1 == 0) {
        ReportStatementTooLarge(cx, bce);
        return JS_FALSE;
    }

    SpanDep *sdbase;
    if ((index & (index - 1)) == 0 &&
        (!(sdbase = bce->spanDeps) || index >= SPANDEPS_MIN))
    {
        size_t size = sdbase ? SPANDEPS_SIZE(index) : SPANDEPS_SIZE_MIN / 2;
        sdbase = (SpanDep *) cx->realloc_(sdbase, size + size);
        if (!sdbase)
            return JS_FALSE;
        bce->spanDeps = sdbase;
    }

    bce->numSpanDeps = index + 1;
    SpanDep *sd = bce->spanDeps + index;
    sd->top = pc - bce->base();
    sd->offset = sd->before = pc2 - bce->base();

    if (js_CodeSpec[*pc].format & JOF_BACKPATCH) {
        /* Jump offset will be backpatched if off is a non-zero "bpdelta". */
        if (off != 0) {
            JS_ASSERT(off >= 1 + JUMP_OFFSET_LEN);
            if (off > BPDELTA_MAX) {
                ReportStatementTooLarge(cx, bce);
                return JS_FALSE;
            }
        }
        SD_SET_BPDELTA(sd, off);
    } else if (off == 0) {
        /* Jump offset will be patched directly, without backpatch chaining. */
        SD_SET_TARGET(sd, 0);
    } else {
        /* The jump offset in off is non-zero, therefore it's already known. */
        if (!SetSpanDepTarget(cx, bce, sd, off))
            return JS_FALSE;
    }

    if (index > SPANDEP_INDEX_MAX)
        index = SPANDEP_INDEX_HUGE;
    SET_SPANDEP_INDEX(pc2, index);
    return JS_TRUE;
}

static jsbytecode *
AddSwitchSpanDeps(JSContext *cx, BytecodeEmitter *bce, jsbytecode *pc)
{
    JSOp op;
    jsbytecode *pc2;
    ptrdiff_t off;
    jsint low, high;
    uintN njumps, indexlen;

    op = (JSOp) *pc;
    JS_ASSERT(op == JSOP_TABLESWITCH || op == JSOP_LOOKUPSWITCH);
    pc2 = pc;
    off = GET_JUMP_OFFSET(pc2);
    if (!AddSpanDep(cx, bce, pc, pc2, off))
        return NULL;
    pc2 += JUMP_OFFSET_LEN;
    if (op == JSOP_TABLESWITCH) {
        low = GET_JUMP_OFFSET(pc2);
        pc2 += JUMP_OFFSET_LEN;
        high = GET_JUMP_OFFSET(pc2);
        pc2 += JUMP_OFFSET_LEN;
        njumps = (uintN) (high - low + 1);
        indexlen = 0;
    } else {
        njumps = GET_UINT16(pc2);
        pc2 += UINT16_LEN;
        indexlen = INDEX_LEN;
    }
    while (njumps) {
        --njumps;
        pc2 += indexlen;
        off = GET_JUMP_OFFSET(pc2);
        if (!AddSpanDep(cx, bce, pc, pc2, off))
            return NULL;
        pc2 += JUMP_OFFSET_LEN;
    }
    return 1 + pc2;
}

static JSBool
BuildSpanDepTable(JSContext *cx, BytecodeEmitter *bce)
{
    jsbytecode *pc, *end;
    JSOp op;
    const JSCodeSpec *cs;
    ptrdiff_t off;

    pc = bce->base() + bce->spanDepTodo;
    end = bce->next();
    while (pc != end) {
        JS_ASSERT(pc < end);
        op = (JSOp)*pc;
        cs = &js_CodeSpec[op];

        switch (JOF_TYPE(cs->format)) {
          case JOF_TABLESWITCH:
          case JOF_LOOKUPSWITCH:
            pc = AddSwitchSpanDeps(cx, bce, pc);
            if (!pc)
                return JS_FALSE;
            break;

          case JOF_JUMP:
            off = GET_JUMP_OFFSET(pc);
            if (!AddSpanDep(cx, bce, pc, pc, off))
                return JS_FALSE;
            /* FALL THROUGH */
          default:
            pc += cs->length;
            break;
        }
    }

    return JS_TRUE;
}

static SpanDep *
GetSpanDep(BytecodeEmitter *bce, jsbytecode *pc)
{
    uintN index;
    ptrdiff_t offset;
    int lo, hi, mid;
    SpanDep *sd;

    index = GET_SPANDEP_INDEX(pc);
    if (index != SPANDEP_INDEX_HUGE)
        return bce->spanDeps + index;

    offset = pc - bce->base();
    lo = 0;
    hi = bce->numSpanDeps - 1;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        sd = bce->spanDeps + mid;
        if (sd->before == offset)
            return sd;
        if (sd->before < offset)
            lo = mid + 1;
        else
            hi = mid - 1;
    }

    JS_ASSERT(0);
    return NULL;
}

static JSBool
SetBackPatchDelta(JSContext *cx, BytecodeEmitter *bce, jsbytecode *pc, ptrdiff_t delta)
{
    SpanDep *sd;

    JS_ASSERT(delta >= 1 + JUMP_OFFSET_LEN);
    if (!bce->spanDeps && delta < JUMP_OFFSET_MAX) {
        SET_JUMP_OFFSET(pc, delta);
        return JS_TRUE;
    }

    if (delta > BPDELTA_MAX) {
        ReportStatementTooLarge(cx, bce);
        return JS_FALSE;
    }

    if (!bce->spanDeps && !BuildSpanDepTable(cx, bce))
        return JS_FALSE;

    sd = GetSpanDep(bce, pc);
    JS_ASSERT(SD_GET_BPDELTA(sd) == 0);
    SD_SET_BPDELTA(sd, delta);
    return JS_TRUE;
}

static void
UpdateJumpTargets(JumpTarget *jt, ptrdiff_t pivot, ptrdiff_t delta)
{
    if (jt->offset > pivot) {
        jt->offset += delta;
        if (jt->kids[JT_LEFT])
            UpdateJumpTargets(jt->kids[JT_LEFT], pivot, delta);
    }
    if (jt->kids[JT_RIGHT])
        UpdateJumpTargets(jt->kids[JT_RIGHT], pivot, delta);
}

static SpanDep *
FindNearestSpanDep(BytecodeEmitter *bce, ptrdiff_t offset, int lo, SpanDep *guard)
{
    int num = bce->numSpanDeps;
    JS_ASSERT(num > 0);
    int hi = num - 1;
    SpanDep *sdbase = bce->spanDeps;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        SpanDep *sd = sdbase + mid;
        if (sd->before == offset)
            return sd;
        if (sd->before < offset)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    if (lo == num)
        return guard;
    SpanDep *sd = sdbase + lo;
    JS_ASSERT(sd->before >= offset && (lo == 0 || sd[-1].before < offset));
    return sd;
}

static void
FreeJumpTargets(BytecodeEmitter *bce, JumpTarget *jt)
{
    if (jt->kids[JT_LEFT])
        FreeJumpTargets(bce, jt->kids[JT_LEFT]);
    if (jt->kids[JT_RIGHT])
        FreeJumpTargets(bce, jt->kids[JT_RIGHT]);
    jt->kids[JT_LEFT] = bce->jtFreeList;
    bce->jtFreeList = jt;
}

static JSBool
OptimizeSpanDeps(JSContext *cx, BytecodeEmitter *bce)
{
    jsbytecode *pc, *oldpc, *base, *limit, *next;
    SpanDep *sd, *sd2, *sdbase, *sdlimit, *sdtop, guard;
    ptrdiff_t offset, growth, delta, top, pivot, span, length, target;
    JSBool done;
    JSOp op;
    uint32 type;
    jssrcnote *sn, *snlimit;
    JSSrcNoteSpec *spec;
    uintN i, n, noteIndex;
    TryNode *tryNode;
    DebugOnly<int> passes = 0;

    base = bce->base();
    sdbase = bce->spanDeps;
    sdlimit = sdbase + bce->numSpanDeps;
    offset = bce->offset();
    growth = 0;

    do {
        done = JS_TRUE;
        delta = 0;
        top = pivot = -1;
        sdtop = NULL;
        pc = NULL;
        op = JSOP_NOP;
        type = 0;
#ifdef DEBUG_brendan
        passes++;
#endif

        for (sd = sdbase; sd < sdlimit; sd++) {
            JS_ASSERT(JT_HAS_TAG(sd->target));
            sd->offset += delta;

            if (sd->top != top) {
                sdtop = sd;
                top = sd->top;
                JS_ASSERT(top == sd->before);
                pivot = sd->offset;
                pc = base + top;
                op = (JSOp) *pc;
                type = JOF_OPTYPE(op);
                if (JOF_TYPE_IS_EXTENDED_JUMP(type)) {
                    /*
                     * We already extended all the jump offset operands for
                     * the opcode at sd->top.  Jumps and branches have only
                     * one jump offset operand, but switches have many, all
                     * of which are adjacent in bce->spanDeps.
                     */
                    continue;
                }

                JS_ASSERT(type == JOF_JUMP ||
                          type == JOF_TABLESWITCH ||
                          type == JOF_LOOKUPSWITCH);
            }

            if (!JOF_TYPE_IS_EXTENDED_JUMP(type)) {
                span = SD_SPAN(sd, pivot);
                if (span < JUMP_OFFSET_MIN || JUMP_OFFSET_MAX < span) {
                    ptrdiff_t deltaFromTop = 0;

                    done = JS_FALSE;

                    switch (op) {
                      case JSOP_GOTO:         op = JSOP_GOTOX; break;
                      case JSOP_IFEQ:         op = JSOP_IFEQX; break;
                      case JSOP_IFNE:         op = JSOP_IFNEX; break;
                      case JSOP_OR:           op = JSOP_ORX; break;
                      case JSOP_AND:          op = JSOP_ANDX; break;
                      case JSOP_GOSUB:        op = JSOP_GOSUBX; break;
                      case JSOP_CASE:         op = JSOP_CASEX; break;
                      case JSOP_DEFAULT:      op = JSOP_DEFAULTX; break;
                      case JSOP_TABLESWITCH:  op = JSOP_TABLESWITCHX; break;
                      case JSOP_LOOKUPSWITCH: op = JSOP_LOOKUPSWITCHX; break;
                      case JSOP_LABEL:        op = JSOP_LABELX; break;
                      default:
                        ReportStatementTooLarge(cx, bce);
                        return JS_FALSE;
                    }
                    *pc = (jsbytecode) op;

                    for (sd2 = sdtop; sd2 < sdlimit && sd2->top == top; sd2++) {
                        if (sd2 <= sd) {
                            /*
                             * sd2->offset already includes delta as it stood
                             * before we entered this loop, but it must also
                             * include the delta relative to top due to all the
                             * extended jump offset immediates for the opcode
                             * starting at top, which we extend in this loop.
                             *
                             * If there is only one extended jump offset, then
                             * sd2->offset won't change and this for loop will
                             * iterate once only.
                             */
                            sd2->offset += deltaFromTop;
                            deltaFromTop += JUMPX_OFFSET_LEN - JUMP_OFFSET_LEN;
                        } else {
                            /*
                             * sd2 comes after sd, and won't be revisited by
                             * the outer for loop, so we have to increase its
                             * offset by delta, not merely by deltaFromTop.
                             */
                            sd2->offset += delta;
                        }

                        delta += JUMPX_OFFSET_LEN - JUMP_OFFSET_LEN;
                        UpdateJumpTargets(bce->jumpTargets, sd2->offset,
                                          JUMPX_OFFSET_LEN - JUMP_OFFSET_LEN);
                    }
                    sd = sd2 - 1;
                }
            }
        }

        growth += delta;
    } while (!done);

    if (growth) {
#ifdef DEBUG_brendan
        TokenStream *ts = &bce->parser->tokenStream;

        printf("%s:%u: %u/%u jumps extended in %d passes (%d=%d+%d)\n",
               ts->filename ? ts->filename : "stdin", bce->firstLine,
               growth / (JUMPX_OFFSET_LEN - JUMP_OFFSET_LEN), bce->numSpanDeps,
               passes, offset + growth, offset, growth);
#endif

        /*
         * Ensure that we have room for the extended jumps, but don't round up
         * to a power of two -- we're done generating code, so we cut to fit.
         */
        limit = bce->limit();
        length = offset + growth;
        next = base + length;
        if (next > limit) {
            base = (jsbytecode *) cx->realloc_(base, BYTECODE_SIZE(length));
            if (!base) {
                js_ReportOutOfMemory(cx);
                return JS_FALSE;
            }
            bce->current->base = base;
            bce->current->limit = next = base + length;
        }
        bce->current->next = next;

        /*
         * Set up a fake span dependency record to guard the end of the code
         * being generated.  This guard record is returned as a fencepost by
         * FindNearestSpanDep if there is no real spandep at or above a given
         * unextended code offset.
         */
        guard.top = -1;
        guard.offset = offset + growth;
        guard.before = offset;
        guard.target = NULL;
    }

    /*
     * Now work backwards through the span dependencies, copying chunks of
     * bytecode between each extended jump toward the end of the grown code
     * space, and restoring immediate offset operands for all jump bytecodes.
     * The first chunk of bytecodes, starting at base and ending at the first
     * extended jump offset (NB: this chunk includes the operation bytecode
     * just before that immediate jump offset), doesn't need to be copied.
     */
    JS_ASSERT(sd == sdlimit);
    top = -1;
    while (--sd >= sdbase) {
        if (sd->top != top) {
            top = sd->top;
            op = (JSOp) base[top];
            type = JOF_OPTYPE(op);

            for (sd2 = sd - 1; sd2 >= sdbase && sd2->top == top; sd2--)
                continue;
            sd2++;
            pivot = sd2->offset;
            JS_ASSERT(top == sd2->before);
        }

        oldpc = base + sd->before;
        span = SD_SPAN(sd, pivot);

        /*
         * If this jump didn't need to be extended, restore its span immediate
         * offset operand now, overwriting the index of sd within bce->spanDeps
         * that was stored temporarily after *pc when BuildSpanDepTable ran.
         *
         * Note that span might fit in 16 bits even for an extended jump op,
         * if the op has multiple span operands, not all of which overflowed
         * (e.g. JSOP_LOOKUPSWITCH or JSOP_TABLESWITCH where some cases are in
         * range for a short jump, but others are not).
         */
        if (!JOF_TYPE_IS_EXTENDED_JUMP(type)) {
            JS_ASSERT(JUMP_OFFSET_MIN <= span && span <= JUMP_OFFSET_MAX);
            SET_JUMP_OFFSET(oldpc, span);
            continue;
        }

        /*
         * Set up parameters needed to copy the next run of bytecode starting
         * at offset (which is a cursor into the unextended, original bytecode
         * vector), down to sd->before (a cursor of the same scale as offset,
         * it's the index of the original jump pc).  Reuse delta to count the
         * nominal number of bytes to copy.
         */
        pc = base + sd->offset;
        delta = offset - sd->before;
        JS_ASSERT(delta >= 1 + JUMP_OFFSET_LEN);

        /*
         * Don't bother copying the jump offset we're about to reset, but do
         * copy the bytecode at oldpc (which comes just before its immediate
         * jump offset operand), on the next iteration through the loop, by
         * including it in offset's new value.
         */
        offset = sd->before + 1;
        size_t size = BYTECODE_SIZE(delta - (1 + JUMP_OFFSET_LEN));
        if (size) {
            memmove(pc + 1 + JUMPX_OFFSET_LEN,
                    oldpc + 1 + JUMP_OFFSET_LEN,
                    size);
        }

        SET_JUMPX_OFFSET(pc, span);
    }

    if (growth) {
        /*
         * Fix source note deltas.  Don't hardwire the delta fixup adjustment,
         * even though currently it must be JUMPX_OFFSET_LEN - JUMP_OFFSET_LEN
         * at each sd that moved.  The future may bring different offset sizes
         * for span-dependent instruction operands.  However, we fix only main
         * notes here, not prolog notes -- we know that prolog opcodes are not
         * span-dependent, and aren't likely ever to be.
         */
        offset = growth = 0;
        sd = sdbase;
        for (sn = bce->main.notes, snlimit = sn + bce->main.noteCount;
             sn < snlimit;
             sn = SN_NEXT(sn)) {
            /*
             * Recall that the offset of a given note includes its delta, and
             * tells the offset of the annotated bytecode from the main entry
             * point of the script.
             */
            offset += SN_DELTA(sn);
            while (sd < sdlimit && sd->before < offset) {
                /*
                 * To compute the delta to add to sn, we need to look at the
                 * spandep after sd, whose offset - (before + growth) tells by
                 * how many bytes sd's instruction grew.
                 */
                sd2 = sd + 1;
                if (sd2 == sdlimit)
                    sd2 = &guard;
                delta = sd2->offset - (sd2->before + growth);
                if (delta > 0) {
                    JS_ASSERT(delta == JUMPX_OFFSET_LEN - JUMP_OFFSET_LEN);
                    sn = AddToSrcNoteDelta(cx, bce, sn, delta);
                    if (!sn)
                        return JS_FALSE;
                    snlimit = bce->main.notes + bce->main.noteCount;
                    growth += delta;
                }
                sd++;
            }

            /*
             * If sn has span-dependent offset operands, check whether each
             * covers further span-dependencies, and increase those operands
             * accordingly.  Some source notes measure offset not from the
             * annotated pc, but from that pc plus some small bias.  NB: we
             * assume that spec->offsetBias can't itself span span-dependent
             * instructions!
             */
            spec = &js_SrcNoteSpec[SN_TYPE(sn)];
            if (spec->isSpanDep) {
                pivot = offset + spec->offsetBias;
                n = spec->arity;
                for (i = 0; i < n; i++) {
                    span = js_GetSrcNoteOffset(sn, i);
                    if (span == 0)
                        continue;
                    target = pivot + span * spec->isSpanDep;
                    sd2 = FindNearestSpanDep(bce, target,
                                             (target >= pivot)
                                             ? sd - sdbase
                                             : 0,
                                             &guard);

                    /*
                     * Increase target by sd2's before-vs-after offset delta,
                     * which is absolute (i.e., relative to start of script,
                     * as is target).  Recompute the span by subtracting its
                     * adjusted pivot from target.
                     */
                    target += sd2->offset - sd2->before;
                    span = target - (pivot + growth);
                    span *= spec->isSpanDep;
                    noteIndex = sn - bce->main.notes;
                    if (!SetSrcNoteOffset(cx, bce, noteIndex, i, span))
                        return JS_FALSE;
                    sn = bce->main.notes + noteIndex;
                    snlimit = bce->main.notes + bce->main.noteCount;
                }
            }
        }
        bce->main.lastNoteOffset += growth;

        /*
         * Fix try/catch notes (O(numTryNotes * log2(numSpanDeps)), but it's
         * not clear how we can beat that).
         */
        for (tryNode = bce->lastTryNode; tryNode; tryNode = tryNode->prev) {
            /*
             * First, look for the nearest span dependency at/above tn->start.
             * There may not be any such spandep, in which case the guard will
             * be returned.
             */
            offset = tryNode->note.start;
            sd = FindNearestSpanDep(bce, offset, 0, &guard);
            delta = sd->offset - sd->before;
            tryNode->note.start = offset + delta;

            /*
             * Next, find the nearest spandep at/above tn->start + tn->length.
             * Use its delta minus tn->start's delta to increase tn->length.
             */
            length = tryNode->note.length;
            sd2 = FindNearestSpanDep(bce, offset + length, sd - sdbase, &guard);
            if (sd2 != sd) {
                tryNode->note.length =
                    length + sd2->offset - sd2->before - delta;
            }
        }
    }

#ifdef DEBUG_brendan
  {
    uintN bigspans = 0;
    top = -1;
    for (sd = sdbase; sd < sdlimit; sd++) {
        offset = sd->offset;

        /* NB: sd->top cursors into the original, unextended bytecode vector. */
        if (sd->top != top) {
            JS_ASSERT(top == -1 ||
                      !JOF_TYPE_IS_EXTENDED_JUMP(type) ||
                      bigspans != 0);
            bigspans = 0;
            top = sd->top;
            JS_ASSERT(top == sd->before);
            op = (JSOp) base[offset];
            type = JOF_OPTYPE(op);
            JS_ASSERT(type == JOF_JUMP ||
                      type == JOF_JUMPX ||
                      type == JOF_TABLESWITCH ||
                      type == JOF_TABLESWITCHX ||
                      type == JOF_LOOKUPSWITCH ||
                      type == JOF_LOOKUPSWITCHX);
            pivot = offset;
        }

        pc = base + offset;
        if (JOF_TYPE_IS_EXTENDED_JUMP(type)) {
            span = GET_JUMPX_OFFSET(pc);
            if (span < JUMP_OFFSET_MIN || JUMP_OFFSET_MAX < span) {
                bigspans++;
            } else {
                JS_ASSERT(type == JOF_TABLESWITCHX ||
                          type == JOF_LOOKUPSWITCHX);
            }
        } else {
            span = GET_JUMP_OFFSET(pc);
        }
        JS_ASSERT(SD_SPAN(sd, pivot) == span);
    }
    JS_ASSERT(!JOF_TYPE_IS_EXTENDED_JUMP(type) || bigspans != 0);
  }
#endif

    /*
     * Reset so we optimize at most once -- bce may be used for further code
     * generation of successive, independent, top-level statements.  No jump
     * can span top-level statements, because JS lacks goto.
     */
    cx->free_(bce->spanDeps);
    bce->spanDeps = NULL;
    FreeJumpTargets(bce, bce->jumpTargets);
    bce->jumpTargets = NULL;
    bce->numSpanDeps = bce->numJumpTargets = 0;
    bce->spanDepTodo = bce->offset();
    return JS_TRUE;
}

static ptrdiff_t
EmitJump(JSContext *cx, BytecodeEmitter *bce, JSOp op, ptrdiff_t off)
{
    JSBool extend;
    ptrdiff_t jmp;
    jsbytecode *pc;

    extend = off < JUMP_OFFSET_MIN || JUMP_OFFSET_MAX < off;
    if (extend && !bce->spanDeps && !BuildSpanDepTable(cx, bce))
        return -1;

    jmp = Emit3(cx, bce, op, JUMP_OFFSET_HI(off), JUMP_OFFSET_LO(off));
    if (jmp >= 0 && (extend || bce->spanDeps)) {
        pc = bce->code(jmp);
        if (!AddSpanDep(cx, bce, pc, pc, off))
            return -1;
    }
    return jmp;
}

static ptrdiff_t
GetJumpOffset(BytecodeEmitter *bce, jsbytecode *pc)
{
    if (!bce->spanDeps)
        return GET_JUMP_OFFSET(pc);

    SpanDep *sd = GetSpanDep(bce, pc);
    JumpTarget *jt = sd->target;
    if (!JT_HAS_TAG(jt))
        return JT_TO_BPDELTA(jt);

    ptrdiff_t top = sd->top;
    while (--sd >= bce->spanDeps && sd->top == top)
        continue;
    sd++;
    return JT_CLR_TAG(jt)->offset - sd->offset;
}

JSBool
frontend::SetJumpOffset(JSContext *cx, BytecodeEmitter *bce, jsbytecode *pc, ptrdiff_t off)
{
    if (!bce->spanDeps) {
        if (JUMP_OFFSET_MIN <= off && off <= JUMP_OFFSET_MAX) {
            SET_JUMP_OFFSET(pc, off);
            return JS_TRUE;
        }

        if (!BuildSpanDepTable(cx, bce))
            return JS_FALSE;
    }

    return SetSpanDepTarget(cx, bce, GetSpanDep(bce, pc), off);
}

bool
TreeContext::inStatement(StmtType type)
{
    for (StmtInfo *stmt = topStmt; stmt; stmt = stmt->down) {
        if (stmt->type == type)
            return true;
    }
    return false;
}

bool
TreeContext::ensureSharpSlots()
{
#if JS_HAS_SHARP_VARS
    JS_STATIC_ASSERT(SHARP_NSLOTS == 2);

    if (sharpSlotBase >= 0) {
        JS_ASSERT(flags & TCF_HAS_SHARPS);
        return true;
    }

    JS_ASSERT(!(flags & TCF_HAS_SHARPS));
    if (inFunction()) {
        JSContext *cx = parser->context;
        JSAtom *sharpArrayAtom = js_Atomize(cx, "#array", 6);
        JSAtom *sharpDepthAtom = js_Atomize(cx, "#depth", 6);
        if (!sharpArrayAtom || !sharpDepthAtom)
            return false;

        sharpSlotBase = bindings.countVars();
        if (!bindings.addVariable(cx, sharpArrayAtom))
            return false;
        if (!bindings.addVariable(cx, sharpDepthAtom))
            return false;
    } else {
        /*
         * js::frontend::CompileScript will rebase immediate operands
         * indexing the sharp slots to come at the end of the global script's
         * |nfixed| slots storage, after gvars and regexps.
         */
        sharpSlotBase = 0;
    }
    flags |= TCF_HAS_SHARPS;
#endif
    return true;
}

bool
TreeContext::skipSpansGenerator(unsigned skip)
{
    TreeContext *tc = this;
    for (unsigned i = 0; i < skip; ++i, tc = tc->parent) {
        if (!tc)
            return false;
        if (tc->flags & TCF_FUN_IS_GENERATOR)
            return true;
    }
    return false;
}

bool
frontend::SetStaticLevel(TreeContext *tc, uintN staticLevel)
{
    /*
     * This is a lot simpler than error-checking every UpvarCookie::set, and
     * practically speaking it leaves more than enough room for upvars.
     */
    if (UpvarCookie::isLevelReserved(staticLevel)) {
        JS_ReportErrorNumber(tc->parser->context, js_GetErrorMessage, NULL,
                             JSMSG_TOO_DEEP, js_function_str);
        return false;
    }
    tc->staticLevel = staticLevel;
    return true;
}

bool
frontend::GenerateBlockId(TreeContext *tc, uint32& blockid)
{
    if (tc->blockidGen == JS_BIT(20)) {
        JS_ReportErrorNumber(tc->parser->context, js_GetErrorMessage, NULL,
                             JSMSG_NEED_DIET, "program");
        return false;
    }
    blockid = tc->blockidGen++;
    return true;
}

void
frontend::PushStatement(TreeContext *tc, StmtInfo *stmt, StmtType type, ptrdiff_t top)
{
    stmt->type = type;
    stmt->flags = 0;
    stmt->blockid = tc->blockid();
    SET_STATEMENT_TOP(stmt, top);
    stmt->label = NULL;
    JS_ASSERT(!stmt->blockBox);
    stmt->down = tc->topStmt;
    tc->topStmt = stmt;
    if (STMT_LINKS_SCOPE(stmt)) {
        stmt->downScope = tc->topScopeStmt;
        tc->topScopeStmt = stmt;
    } else {
        stmt->downScope = NULL;
    }
}

void
frontend::PushBlockScope(TreeContext *tc, StmtInfo *stmt, ObjectBox *blockBox, ptrdiff_t top)
{
    PushStatement(tc, stmt, STMT_BLOCK, top);
    stmt->flags |= SIF_SCOPE;
    blockBox->parent = tc->blockChainBox;
    blockBox->object->setParent(tc->blockChain());
    stmt->downScope = tc->topScopeStmt;
    tc->topScopeStmt = stmt;
    tc->blockChainBox = blockBox;
    stmt->blockBox = blockBox;
}

/*
 * Emit a backpatch op with offset pointing to the previous jump of this type,
 * so that we can walk back up the chain fixing up the op and jump offset.
 */
static ptrdiff_t
EmitBackPatchOp(JSContext *cx, BytecodeEmitter *bce, JSOp op, ptrdiff_t *lastp)
{
    ptrdiff_t offset, delta;

    offset = bce->offset();
    delta = offset - *lastp;
    *lastp = offset;
    JS_ASSERT(delta > 0);
    return EmitJump(cx, bce, op, delta);
}

/* A macro for inlining at the top of EmitTree (whence it came). */
#define UPDATE_LINE_NUMBER_NOTES(cx, bce, line)                               \
    JS_BEGIN_MACRO                                                            \
        uintN line_ = (line);                                                 \
        uintN delta_ = line_ - bce->currentLine();                            \
        if (delta_ != 0) {                                                    \
            /*                                                                \
             * Encode any change in the current source line number by using   \
             * either several SRC_NEWLINE notes or just one SRC_SETLINE note, \
             * whichever consumes less space.                                 \
             *                                                                \
             * NB: We handle backward line number deltas (possible with for   \
             * loops where the update part is emitted after the body, but its \
             * line number is <= any line number in the body) here by letting \
             * unsigned delta_ wrap to a very large number, which triggers a  \
             * SRC_SETLINE.                                                   \
             */                                                               \
            bce->current->currentLine = line_;                                \
            if (delta_ >= (uintN)(2 + ((line_ > SN_3BYTE_OFFSET_MASK)<<1))) { \
                if (NewSrcNote2(cx, bce, SRC_SETLINE, (ptrdiff_t)line_) < 0)  \
                    return JS_FALSE;                                          \
            } else {                                                          \
                do {                                                          \
                    if (NewSrcNote(cx, bce, SRC_NEWLINE) < 0)                 \
                        return JS_FALSE;                                      \
                } while (--delta_ != 0);                                      \
            }                                                                 \
        }                                                                     \
    JS_END_MACRO

/* A function, so that we avoid macro-bloating all the other callsites. */
static JSBool
UpdateLineNumberNotes(JSContext *cx, BytecodeEmitter *bce, uintN line)
{
    UPDATE_LINE_NUMBER_NOTES(cx, bce, line);
    return JS_TRUE;
}

static ptrdiff_t
EmitTraceOp(JSContext *cx, BytecodeEmitter *bce, ParseNode *nextpn)
{
    if (nextpn) {
        /*
         * Try to give the JSOP_TRACE the same line number as the next
         * instruction. nextpn is often a block, in which case the next
         * instruction typically comes from the first statement inside.
         */
        JS_ASSERT_IF(nextpn->isKind(PNK_STATEMENTLIST), nextpn->isArity(PN_LIST));
        if (nextpn->isKind(PNK_STATEMENTLIST) && nextpn->pn_head)
            nextpn = nextpn->pn_head;
        if (!UpdateLineNumberNotes(cx, bce, nextpn->pn_pos.begin.lineno))
            return -1;
    }

    uint32 index = bce->traceIndex;
    if (index < UINT16_MAX)
        bce->traceIndex++;
    return Emit3(cx, bce, JSOP_TRACE, UINT16_HI(index), UINT16_LO(index));
}

/*
 * If op is JOF_TYPESET (see the type barriers comment in jsinfer.h), reserve
 * a type set to store its result.
 */
static inline void
CheckTypeSet(JSContext *cx, BytecodeEmitter *bce, JSOp op)
{
    if (js_CodeSpec[op].format & JOF_TYPESET) {
        if (bce->typesetCount < UINT16_MAX)
            bce->typesetCount++;
    }
}

/*
 * Macro to emit a bytecode followed by a uint16 immediate operand stored in
 * big-endian order, used for arg and var numbers as well as for atomIndexes.
 * NB: We use cx and bce from our caller's lexical environment, and return
 * false on error.
 */
#define EMIT_UINT16_IMM_OP(op, i)                                             \
    JS_BEGIN_MACRO                                                            \
        if (Emit3(cx, bce, op, UINT16_HI(i), UINT16_LO(i)) < 0)               \
            return JS_FALSE;                                                  \
        CheckTypeSet(cx, bce, op);                                            \
    JS_END_MACRO

#define EMIT_UINT16PAIR_IMM_OP(op, i, j)                                      \
    JS_BEGIN_MACRO                                                            \
        ptrdiff_t off_ = EmitN(cx, bce, op, 2 * UINT16_LEN);                  \
        if (off_ < 0)                                                         \
            return JS_FALSE;                                                  \
        jsbytecode *pc_ = bce->code(off_);                                 \
        SET_UINT16(pc_, i);                                                   \
        pc_ += UINT16_LEN;                                                    \
        SET_UINT16(pc_, j);                                                   \
    JS_END_MACRO

#define EMIT_UINT16_IN_PLACE(offset, op, i)                                   \
    JS_BEGIN_MACRO                                                            \
        bce->code(offset)[0] = op;                                         \
        bce->code(offset)[1] = UINT16_HI(i);                               \
        bce->code(offset)[2] = UINT16_LO(i);                               \
    JS_END_MACRO

static JSBool
FlushPops(JSContext *cx, BytecodeEmitter *bce, intN *npops)
{
    JS_ASSERT(*npops != 0);
    if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
        return JS_FALSE;
    EMIT_UINT16_IMM_OP(JSOP_POPN, *npops);
    *npops = 0;
    return JS_TRUE;
}

/*
 * Emit additional bytecode(s) for non-local jumps.
 */
static JSBool
EmitNonLocalJumpFixup(JSContext *cx, BytecodeEmitter *bce, StmtInfo *toStmt)
{
    /*
     * The non-local jump fixup we emit will unbalance bce->stackDepth, because
     * the fixup replicates balanced code such as JSOP_LEAVEWITH emitted at the
     * end of a with statement, so we save bce->stackDepth here and restore it
     * just before a successful return.
     */
    intN depth = bce->stackDepth;
    intN npops = 0;

#define FLUSH_POPS() if (npops && !FlushPops(cx, bce, &npops)) return JS_FALSE

    for (StmtInfo *stmt = bce->topStmt; stmt != toStmt; stmt = stmt->down) {
        switch (stmt->type) {
          case STMT_FINALLY:
            FLUSH_POPS();
            if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
                return JS_FALSE;
            if (EmitBackPatchOp(cx, bce, JSOP_BACKPATCH, &GOSUBS(*stmt)) < 0)
                return JS_FALSE;
            break;

          case STMT_WITH:
            /* There's a With object on the stack that we need to pop. */
            FLUSH_POPS();
            if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
                return JS_FALSE;
            if (Emit1(cx, bce, JSOP_LEAVEWITH) < 0)
                return JS_FALSE;
            break;

          case STMT_FOR_IN_LOOP:
            /*
             * The iterator and the object being iterated need to be popped.
             */
            FLUSH_POPS();
            if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
                return JS_FALSE;
            if (Emit1(cx, bce, JSOP_ENDITER) < 0)
                return JS_FALSE;
            break;

          case STMT_SUBROUTINE:
            /*
             * There's a [exception or hole, retsub pc-index] pair on the
             * stack that we need to pop.
             */
            npops += 2;
            break;

          default:;
        }

        if (stmt->flags & SIF_SCOPE) {
            /* There is a Block object with locals on the stack to pop. */
            FLUSH_POPS();
            if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
                return JS_FALSE;
            if (!EmitLeaveBlock(cx, bce, JSOP_LEAVEBLOCK, stmt->blockBox))
                return JS_FALSE;
        }
    }

    FLUSH_POPS();
    bce->stackDepth = depth;
    return JS_TRUE;

#undef FLUSH_POPS
}

static JSBool
EmitKnownBlockChain(JSContext *cx, BytecodeEmitter *bce, ObjectBox *box)
{
    if (box)
        return EmitIndexOp(cx, JSOP_BLOCKCHAIN, box->index, bce);
    return Emit1(cx, bce, JSOP_NULLBLOCKCHAIN) >= 0;
}

static JSBool
EmitBlockChain(JSContext *cx, BytecodeEmitter *bce)
{
    return EmitKnownBlockChain(cx, bce, bce->blockChainBox);
}

static const jsatomid INVALID_ATOMID = -1;

static ptrdiff_t
EmitGoto(JSContext *cx, BytecodeEmitter *bce, StmtInfo *toStmt, ptrdiff_t *lastp,
         jsatomid labelIndex = INVALID_ATOMID, SrcNoteType noteType = SRC_NULL)
{
    intN index;

    if (!EmitNonLocalJumpFixup(cx, bce, toStmt))
        return -1;

    if (labelIndex != INVALID_ATOMID)
        index = NewSrcNote2(cx, bce, noteType, ptrdiff_t(labelIndex));
    else if (noteType != SRC_NULL)
        index = NewSrcNote(cx, bce, noteType);
    else
        index = 0;
    if (index < 0)
        return -1;

    ptrdiff_t result = EmitBackPatchOp(cx, bce, JSOP_BACKPATCH, lastp);
    if (result < 0)
        return result;

    if (!EmitBlockChain(cx, bce))
        return -1;

    return result;
}

static JSBool
BackPatch(JSContext *cx, BytecodeEmitter *bce, ptrdiff_t last, jsbytecode *target, jsbytecode op)
{
    jsbytecode *pc, *stop;
    ptrdiff_t delta, span;

    pc = bce->code(last);
    stop = bce->code(-1);
    while (pc != stop) {
        delta = GetJumpOffset(bce, pc);
        span = target - pc;
        CHECK_AND_SET_JUMP_OFFSET(cx, bce, pc, span);

        /*
         * Set *pc after jump offset in case bpdelta didn't overflow, but span
         * does (if so, CHECK_AND_SET_JUMP_OFFSET might call BuildSpanDepTable
         * and need to see the JSOP_BACKPATCH* op at *pc).
         */
        *pc = op;
        pc -= delta;
    }
    return JS_TRUE;
}

void
frontend::PopStatementTC(TreeContext *tc)
{
    StmtInfo *stmt = tc->topStmt;
    tc->topStmt = stmt->down;
    if (STMT_LINKS_SCOPE(stmt)) {
        tc->topScopeStmt = stmt->downScope;
        if (stmt->flags & SIF_SCOPE) {
            tc->blockChainBox = stmt->blockBox->parent;
        }
    }
}

JSBool
frontend::PopStatementBCE(JSContext *cx, BytecodeEmitter *bce)
{
    StmtInfo *stmt = bce->topStmt;
    if (!STMT_IS_TRYING(stmt) &&
        (!BackPatch(cx, bce, stmt->breaks, bce->next(), JSOP_GOTO) ||
         !BackPatch(cx, bce, stmt->continues, bce->code(stmt->update),
                    JSOP_GOTO))) {
        return JS_FALSE;
    }
    PopStatementTC(bce);
    return JS_TRUE;
}

JSBool
frontend::DefineCompileTimeConstant(JSContext *cx, BytecodeEmitter *bce, JSAtom *atom, ParseNode *pn)
{
    /* XXX just do numbers for now */
    if (pn->isKind(PNK_NUMBER)) {
        if (!bce->constMap.put(atom, NumberValue(pn->pn_dval)))
            return JS_FALSE;
    }
    return JS_TRUE;
}

StmtInfo *
frontend::LexicalLookup(TreeContext *tc, JSAtom *atom, jsint *slotp, StmtInfo *stmt)
{
    if (!stmt)
        stmt = tc->topScopeStmt;
    for (; stmt; stmt = stmt->downScope) {
        if (stmt->type == STMT_WITH)
            break;

        /* Skip "maybe scope" statements that don't contain let bindings. */
        if (!(stmt->flags & SIF_SCOPE))
            continue;

        JSObject *obj = stmt->blockBox->object;
        JS_ASSERT(obj->isStaticBlock());

        const Shape *shape = obj->nativeLookup(tc->parser->context, ATOM_TO_JSID(atom));
        if (shape) {
            JS_ASSERT(shape->hasShortID());

            if (slotp) {
                JS_ASSERT(obj->getSlot(JSSLOT_BLOCK_DEPTH).isInt32());
                *slotp = obj->getSlot(JSSLOT_BLOCK_DEPTH).toInt32() + shape->shortid;
            }
            return stmt;
        }
    }

    if (slotp)
        *slotp = -1;
    return stmt;
}

/*
 * The function sets vp to NO_CONSTANT when the atom does not corresponds to a
 * name defining a constant.
 */
static JSBool
LookupCompileTimeConstant(JSContext *cx, BytecodeEmitter *bce, JSAtom *atom, Value *constp)
{
    /*
     * Chase down the bce stack, but only until we reach the outermost bce.
     * This enables propagating consts from top-level into switch cases in a
     * function compiled along with the top-level script.
     */
    constp->setMagic(JS_NO_CONSTANT);
    do {
        if (bce->inFunction() || bce->compileAndGo()) {
            /* XXX this will need revising if 'const' becomes block-scoped. */
            StmtInfo *stmt = LexicalLookup(bce, atom, NULL);
            if (stmt)
                return JS_TRUE;

            if (BytecodeEmitter::ConstMap::Ptr p = bce->constMap.lookup(atom)) {
                JS_ASSERT(!p->value.isMagic(JS_NO_CONSTANT));
                *constp = p->value;
                return JS_TRUE;
            }

            /*
             * Try looking in the variable object for a direct property that
             * is readonly and permanent.  We know such a property can't be
             * shadowed by another property on obj's prototype chain, or a
             * with object or catch variable; nor can prop's value be changed,
             * nor can prop be deleted.
             */
            if (bce->inFunction()) {
                if (bce->bindings.hasBinding(cx, atom))
                    break;
            } else {
                JS_ASSERT(bce->compileAndGo());
                JSObject *obj = bce->scopeChain();

                const Shape *shape = obj->nativeLookup(cx, ATOM_TO_JSID(atom));
                if (shape) {
                    /*
                     * We're compiling code that will be executed immediately,
                     * not re-executed against a different scope chain and/or
                     * variable object.  Therefore we can get constant values
                     * from our variable object here.
                     */
                    if (!shape->writable() && !shape->configurable() &&
                        shape->hasDefaultGetter() && obj->containsSlot(shape->slot)) {
                        *constp = obj->getSlot(shape->slot);
                    }
                }

                if (shape)
                    break;
            }
        }
    } while (bce->parent && (bce = bce->parent->asBytecodeEmitter()));
    return JS_TRUE;
}

static inline bool
FitsWithoutBigIndex(uintN index)
{
    return index < JS_BIT(16);
}

/*
 * Return JSOP_NOP to indicate that index fits 2 bytes and no index segment
 * reset instruction is necessary, JSOP_FALSE to indicate an error or either
 * JSOP_RESETBASE0 or JSOP_RESETBASE1 to indicate the reset bytecode to issue
 * after the main bytecode sequence.
 */
static JSOp
EmitBigIndexPrefix(JSContext *cx, BytecodeEmitter *bce, uintN index)
{
    uintN indexBase;

    /*
     * We have max 3 bytes for indexes and check for INDEX_LIMIT overflow only
     * for big indexes.
     */
    JS_STATIC_ASSERT(INDEX_LIMIT <= JS_BIT(24));
    JS_STATIC_ASSERT(INDEX_LIMIT >=
                     (JSOP_INDEXBASE3 - JSOP_INDEXBASE1 + 2) << 16);

    if (FitsWithoutBigIndex(index))
        return JSOP_NOP;
    indexBase = index >> 16;
    if (indexBase <= JSOP_INDEXBASE3 - JSOP_INDEXBASE1 + 1) {
        if (Emit1(cx, bce, (JSOp)(JSOP_INDEXBASE1 + indexBase - 1)) < 0)
            return JSOP_FALSE;
        return JSOP_RESETBASE0;
    }

    if (index >= INDEX_LIMIT) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_TOO_MANY_LITERALS);
        return JSOP_FALSE;
    }

    if (Emit2(cx, bce, JSOP_INDEXBASE, (JSOp)indexBase) < 0)
        return JSOP_FALSE;
    return JSOP_RESETBASE;
}

/*
 * Emit a bytecode and its 2-byte constant index immediate operand. If the
 * index requires more than 2 bytes, emit a prefix op whose 8-bit immediate
 * operand effectively extends the 16-bit immediate of the prefixed opcode,
 * by changing index "segment" (see jsinterp.c). We optimize segments 1-3
 * with single-byte JSOP_INDEXBASE[123] codes.
 *
 * Such prefixing currently requires a suffix to restore the "zero segment"
 * register setting, but this could be optimized further.
 */
static bool
EmitIndexOp(JSContext *cx, JSOp op, uintN index, BytecodeEmitter *bce, JSOp *psuffix)
{
    JSOp bigSuffix;

    bigSuffix = EmitBigIndexPrefix(cx, bce, index);
    if (bigSuffix == JSOP_FALSE)
        return false;
    EMIT_UINT16_IMM_OP(op, index);

    /*
     * For decomposed ops, the suffix needs to go after the decomposed version.
     * This means the suffix will run in the interpreter in both the base
     * and decomposed paths, which works as suffix ops are idempotent.
     */
    JS_ASSERT(!!(js_CodeSpec[op].format & JOF_DECOMPOSE) == (psuffix != NULL));
    if (psuffix) {
        *psuffix = bigSuffix;
        return true;
    }

    return bigSuffix == JSOP_NOP || Emit1(cx, bce, bigSuffix) >= 0;
}

/*
 * Slight sugar for EmitIndexOp, again accessing cx and bce from the macro
 * caller's lexical environment, and embedding a false return on error.
 */
#define EMIT_INDEX_OP(op, index)                                              \
    JS_BEGIN_MACRO                                                            \
        if (!EmitIndexOp(cx, op, index, bce))                                 \
            return JS_FALSE;                                                  \
    JS_END_MACRO

static bool
EmitAtomOp(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce, JSOp *psuffix = NULL)
{
    JS_ASSERT(JOF_OPTYPE(op) == JOF_ATOM);

    if (op == JSOP_GETPROP &&
        pn->pn_atom == cx->runtime->atomState.lengthAtom) {
        /* Specialize length accesses for the interpreter. */
        op = JSOP_LENGTH;
    }

    jsatomid index;
    if (!bce->makeAtomIndex(pn->pn_atom, &index))
        return false;

    return EmitIndexOp(cx, op, index, bce, psuffix);
}

static JSBool
EmitObjectOp(JSContext *cx, ObjectBox *objbox, JSOp op, BytecodeEmitter *bce)
{
    JS_ASSERT(JOF_OPTYPE(op) == JOF_OBJECT);
    return EmitIndexOp(cx, op, bce->objectList.index(objbox), bce);
}

/*
 * What good are ARGNO_LEN and SLOTNO_LEN, you ask?  The answer is that, apart
 * from EmitSlotIndexOp, they abstract out the detail that both are 2, and in
 * other parts of the code there's no necessary relationship between the two.
 * The abstraction cracks here in order to share EmitSlotIndexOp code among
 * the JSOP_DEFLOCALFUN and JSOP_GET{ARG,VAR,LOCAL}PROP cases.
 */
JS_STATIC_ASSERT(ARGNO_LEN == 2);
JS_STATIC_ASSERT(SLOTNO_LEN == 2);

static JSBool
EmitSlotIndexOp(JSContext *cx, JSOp op, uintN slot, uintN index, BytecodeEmitter *bce)
{
    JSOp bigSuffix;
    ptrdiff_t off;
    jsbytecode *pc;

    JS_ASSERT(JOF_OPTYPE(op) == JOF_SLOTATOM ||
              JOF_OPTYPE(op) == JOF_SLOTOBJECT);
    bigSuffix = EmitBigIndexPrefix(cx, bce, index);
    if (bigSuffix == JSOP_FALSE)
        return JS_FALSE;

    /* Emit [op, slot, index]. */
    off = EmitN(cx, bce, op, 2 + INDEX_LEN);
    if (off < 0)
        return JS_FALSE;
    pc = bce->code(off);
    SET_UINT16(pc, slot);
    pc += 2;
    SET_INDEX(pc, index);
    return bigSuffix == JSOP_NOP || Emit1(cx, bce, bigSuffix) >= 0;
}

bool
BytecodeEmitter::shouldNoteClosedName(ParseNode *pn)
{
    return !callsEval() && pn->isDefn() && pn->isClosed();
}

/*
 * Adjust the slot for a block local to account for the number of variables
 * that share the same index space with locals. Due to the incremental code
 * generation for top-level script, we do the adjustment via code patching in
 * js::frontend::CompileScript; see comments there.
 *
 * The function returns -1 on failures.
 */
static jsint
AdjustBlockSlot(JSContext *cx, BytecodeEmitter *bce, jsint slot)
{
    JS_ASSERT((jsuint) slot < bce->maxStackDepth);
    if (bce->inFunction()) {
        slot += bce->bindings.countVars();
        if ((uintN) slot >= SLOTNO_LIMIT) {
            ReportCompileErrorNumber(cx, bce->tokenStream(), NULL, JSREPORT_ERROR,
                                     JSMSG_TOO_MANY_LOCALS);
            slot = -1;
        }
    }
    return slot;
}

static bool
EmitEnterBlock(JSContext *cx, ParseNode *pn, BytecodeEmitter *bce)
{
    JS_ASSERT(pn->isKind(PNK_LEXICALSCOPE));
    if (!EmitObjectOp(cx, pn->pn_objbox, JSOP_ENTERBLOCK, bce))
        return false;

    JSObject *blockObj = pn->pn_objbox->object;
    jsint depth = AdjustBlockSlot(cx, bce, OBJ_BLOCK_DEPTH(cx, blockObj));
    if (depth < 0)
        return false;

    uintN base = JSSLOT_FREE(&BlockClass);
    for (uintN slot = base, limit = base + OBJ_BLOCK_COUNT(cx, blockObj); slot < limit; slot++) {
        const Value &v = blockObj->getSlot(slot);

        /* Beware the empty destructuring dummy. */
        if (v.isUndefined()) {
            JS_ASSERT(slot + 1 <= limit);
            continue;
        }

        Definition *dn = (Definition *) v.toPrivate();
        JS_ASSERT(dn->isDefn());
        JS_ASSERT(uintN(dn->frameSlot() + depth) < JS_BIT(16));
        dn->pn_cookie.set(dn->pn_cookie.level(), uint16(dn->frameSlot() + depth));
#ifdef DEBUG
        for (ParseNode *pnu = dn->dn_uses; pnu; pnu = pnu->pn_link) {
            JS_ASSERT(pnu->pn_lexdef == dn);
            JS_ASSERT(!(pnu->pn_dflags & PND_BOUND));
            JS_ASSERT(pnu->pn_cookie.isFree());
        }
#endif

        /*
         * If this variable is closed over, and |eval| is not present, then
         * then set a bit in dslots so the Method JIT can deoptimize this
         * slot.
         */
        bool isClosed = bce->shouldNoteClosedName(dn);
        blockObj->setSlot(slot, BooleanValue(isClosed));
    }

    /*
     * If clones of this block will have any extensible parents, then the
     * clones must get unique shapes; see the comments for
     * js::Bindings::extensibleParents.
     */
    if ((bce->flags & TCF_FUN_EXTENSIBLE_SCOPE) ||
        bce->bindings.extensibleParents())
        blockObj->setBlockOwnShape(cx);

    return true;
}

static JSBool
EmitLeaveBlock(JSContext *cx, BytecodeEmitter *bce, JSOp op, ObjectBox *box)
{
    JSOp bigSuffix;
    uintN count = OBJ_BLOCK_COUNT(cx, box->object);
    
    bigSuffix = EmitBigIndexPrefix(cx, bce, box->index);
    if (bigSuffix == JSOP_FALSE)
        return JS_FALSE;
    if (Emit5(cx, bce, op, count, box->index) < 0)
        return JS_FALSE;
    return bigSuffix == JSOP_NOP || Emit1(cx, bce, bigSuffix) >= 0;
}

/*
 * Try to convert a *NAME op to a *GNAME op, which optimizes access to
 * undeclared globals. Return true if a conversion was made.
 *
 * This conversion is not made if we are in strict mode.  In eval code nested
 * within (strict mode) eval code, access to an undeclared "global" might
 * merely be to a binding local to that outer eval:
 *
 *   "use strict";
 *   var x = "global";
 *   eval('var x = "eval"; eval("x");'); // 'eval', not 'global'
 *
 * Outside eval code, access to an undeclared global is a strict mode error:
 *
 *   "use strict";
 *   function foo()
 *   {
 *     undeclared = 17; // throws ReferenceError
 *   }
 *   foo();
 */
static bool
TryConvertToGname(BytecodeEmitter *bce, ParseNode *pn, JSOp *op)
{
    if (bce->compileAndGo() && 
        bce->globalScope->globalObj &&
        !bce->mightAliasLocals() &&
        !pn->isDeoptimized() &&
        !(bce->flags & TCF_STRICT_MODE_CODE)) { 
        switch (*op) {
          case JSOP_NAME:     *op = JSOP_GETGNAME; break;
          case JSOP_SETNAME:  *op = JSOP_SETGNAME; break;
          case JSOP_INCNAME:  *op = JSOP_INCGNAME; break;
          case JSOP_NAMEINC:  *op = JSOP_GNAMEINC; break;
          case JSOP_DECNAME:  *op = JSOP_DECGNAME; break;
          case JSOP_NAMEDEC:  *op = JSOP_GNAMEDEC; break;
          case JSOP_SETCONST:
          case JSOP_DELNAME:
            /* Not supported. */
            return false;
          default: JS_NOT_REACHED("gname");
        }
        return true;
    }
    return false;
}

// Binds a global, given a |dn| that is known to have the PND_GVAR bit, and a pn
// that is |dn| or whose definition is |dn|. |pn->pn_cookie| is an outparam
// that will be free (meaning no binding), or a slot number.
static bool
BindKnownGlobal(JSContext *cx, BytecodeEmitter *bce, ParseNode *dn, ParseNode *pn, JSAtom *atom)
{
    // Cookie is an outparam; make sure caller knew to clear it.
    JS_ASSERT(pn->pn_cookie.isFree());

    if (bce->mightAliasLocals())
        return true;

    GlobalScope *globalScope = bce->globalScope;

    jsatomid index;
    if (dn->pn_cookie.isFree()) {
        // The definition wasn't bound, so find its atom's index in the
        // mapping of defined globals.
        AtomIndexPtr p = globalScope->names.lookup(atom);
        JS_ASSERT(!!p);
        index = p.value();
    } else {
        BytecodeEmitter *globalbce = globalScope->bce;

        // If the definition is bound, and we're in the same bce, we can re-use
        // its cookie.
        if (globalbce == bce) {
            pn->pn_cookie = dn->pn_cookie;
            pn->pn_dflags |= PND_BOUND;
            return true;
        }

        // Otherwise, find the atom's index by using the originating bce's
        // global use table.
        index = globalbce->globalUses[dn->pn_cookie.asInteger()].slot;
    }

    if (!bce->addGlobalUse(atom, index, &pn->pn_cookie))
        return false;

    if (!pn->pn_cookie.isFree())
        pn->pn_dflags |= PND_BOUND;

    return true;
}

// See BindKnownGlobal()'s comment.
static bool
BindGlobal(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, JSAtom *atom)
{
    pn->pn_cookie.makeFree();

    Definition *dn;
    if (pn->isUsed()) {
        dn = pn->pn_lexdef;
    } else {
        if (!pn->isDefn())
            return true;
        dn = (Definition *)pn;
    }

    // Only optimize for defined globals.
    if (!dn->isGlobal())
        return true;

    return BindKnownGlobal(cx, bce, dn, pn, atom);
}

/*
 * BindNameToSlot attempts to optimize name gets and sets to stack slot loads
 * and stores, given the compile-time information in bce and a PNK_NAME node pn.
 * It returns false on error, true on success.
 *
 * The caller can inspect pn->pn_cookie for FREE_UPVAR_COOKIE to tell whether
 * optimization occurred, in which case BindNameToSlot also updated pn->pn_op.
 * If pn->pn_cookie is still FREE_UPVAR_COOKIE on return, pn->pn_op still may
 * have been optimized, e.g., from JSOP_NAME to JSOP_CALLEE.  Whether or not
 * pn->pn_op was modified, if this function finds an argument or local variable
 * name, PND_CONST will be set in pn_dflags for read-only properties after a
 * successful return.
 *
 * NB: if you add more opcodes specialized from JSOP_NAME, etc., don't forget
 * to update the special cases in EmitFor (for-in) and EmitAssignment (= and
 * op=, e.g. +=).
 */
static JSBool
BindNameToSlot(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    Definition *dn;
    JSOp op;
    JSAtom *atom;
    Definition::Kind dn_kind;

    JS_ASSERT(pn->isKind(PNK_NAME));

    /* Idempotency tests come first, since we may be called more than once. */
    if (pn->pn_dflags & PND_BOUND)
        return JS_TRUE;

    /* No cookie initialized for these two, they're pre-bound by definition. */
    JS_ASSERT(!pn->isOp(JSOP_ARGUMENTS) && !pn->isOp(JSOP_CALLEE));

    /*
     * The parser linked all uses (including forward references) to their
     * definitions, unless a with statement or direct eval intervened.
     */
    if (pn->isUsed()) {
        JS_ASSERT(pn->pn_cookie.isFree());
        dn = pn->pn_lexdef;
        JS_ASSERT(dn->isDefn());
        if (pn->isDeoptimized())
            return JS_TRUE;
        pn->pn_dflags |= (dn->pn_dflags & PND_CONST);
    } else {
        if (!pn->isDefn())
            return JS_TRUE;
        dn = (Definition *) pn;
    }

    op = pn->getOp();
    if (op == JSOP_NOP)
        return JS_TRUE;

    JS_ASSERT(JOF_OPTYPE(op) == JOF_ATOM);
    atom = pn->pn_atom;
    UpvarCookie cookie = dn->pn_cookie;
    dn_kind = dn->kind();

    /*
     * Turn attempts to mutate const-declared bindings into get ops (for
     * pre-increment and pre-decrement ops, our caller will have to emit
     * JSOP_POS, JSOP_ONE, and JSOP_ADD as well).
     *
     * Turn JSOP_DELNAME into JSOP_FALSE if dn is known, as all declared
     * bindings visible to the compiler are permanent in JS unless the
     * declaration originates at top level in eval code.
     */
    switch (op) {
      case JSOP_NAME:
      case JSOP_SETCONST:
        break;
      case JSOP_DELNAME:
        if (dn_kind != Definition::UNKNOWN) {
            if (bce->parser->callerFrame && dn->isTopLevel())
                JS_ASSERT(bce->compileAndGo());
            else
                pn->setOp(JSOP_FALSE);
            pn->pn_dflags |= PND_BOUND;
            return JS_TRUE;
        }
        break;
      default:
        if (pn->isConst()) {
            if (bce->needStrictChecks()) {
                JSAutoByteString name;
                if (!js_AtomToPrintableString(cx, atom, &name) ||
                    !ReportStrictModeError(cx, bce->tokenStream(), bce, pn, JSMSG_READ_ONLY,
                                           name.ptr())) {
                    return JS_FALSE;
                }
            }
            pn->setOp(op = JSOP_NAME);
        }
    }

    if (dn->isGlobal()) {
        if (op == JSOP_NAME) {
            /*
             * If the definition is a defined global, not potentially aliased
             * by a local variable, and not mutating the variable, try and
             * optimize to a fast, unguarded global access.
             */
            if (!pn->pn_cookie.isFree()) {
                pn->setOp(JSOP_GETGNAME);
                pn->pn_dflags |= PND_BOUND;
                return JS_TRUE;
            }
        }

        /*
         * The locally stored cookie here should really come from |pn|, not
         * |dn|. For example, we could have a SETGNAME op's lexdef be a
         * GETGNAME op, and their cookies have very different meanings. As
         * a workaround, just make the cookie free.
         */
        cookie.makeFree();
    }

    if (cookie.isFree()) {
        StackFrame *caller = bce->parser->callerFrame;
        if (caller) {
            JS_ASSERT(bce->compileAndGo());

            /*
             * Don't generate upvars on the left side of a for loop. See
             * bug 470758.
             */
            if (bce->flags & TCF_IN_FOR_INIT)
                return JS_TRUE;

            JS_ASSERT(caller->isScriptFrame());

            /*
             * If this is an eval in the global scope, then unbound variables
             * must be globals, so try to use GNAME ops.
             */
            if (caller->isGlobalFrame() && TryConvertToGname(bce, pn, &op)) {
                jsatomid _;
                if (!bce->makeAtomIndex(atom, &_))
                    return JS_FALSE;

                pn->setOp(op);
                pn->pn_dflags |= PND_BOUND;
                return JS_TRUE;
            }

            /*
             * Out of tricks, so we must rely on PICs to optimize named
             * accesses from direct eval called from function code.
             */
            return JS_TRUE;
        }

        /* Optimize accesses to undeclared globals. */
        if (!bce->mightAliasLocals() && !TryConvertToGname(bce, pn, &op))
            return JS_TRUE;

        jsatomid _;
        if (!bce->makeAtomIndex(atom, &_))
            return JS_FALSE;

        pn->setOp(op);
        pn->pn_dflags |= PND_BOUND;

        return JS_TRUE;
    }

    uint16 level = cookie.level();
    JS_ASSERT(bce->staticLevel >= level);

    const uintN skip = bce->staticLevel - level;
    if (skip != 0) {
        JS_ASSERT(bce->inFunction());
        JS_ASSERT_IF(cookie.slot() != UpvarCookie::CALLEE_SLOT, bce->roLexdeps->lookup(atom));
        JS_ASSERT(JOF_OPTYPE(op) == JOF_ATOM);
        JS_ASSERT(bce->fun()->u.i.skipmin <= skip);

        /*
         * If op is a mutating opcode, this upvar's lookup skips too many levels,
         * or the function is heavyweight, we fall back on JSOP_*NAME*.
         */
        if (op != JSOP_NAME)
            return JS_TRUE;
        if (skip >= UpvarCookie::UPVAR_LEVEL_LIMIT)
            return JS_TRUE;
        if (bce->flags & TCF_FUN_HEAVYWEIGHT)
            return JS_TRUE;

        if (!bce->fun()->isFlatClosure())
            return JS_TRUE;

        if (!bce->upvarIndices.ensureMap(cx))
            return JS_FALSE;

        AtomIndexAddPtr p = bce->upvarIndices->lookupForAdd(atom);
        jsatomid index;
        if (p) {
            index = p.value();
        } else {
            if (!bce->bindings.addUpvar(cx, atom))
                return JS_FALSE;

            index = bce->upvarIndices->count();
            if (!bce->upvarIndices->add(p, atom, index))
                return JS_FALSE;

            UpvarCookies &upvarMap = bce->upvarMap;
            /* upvarMap should have the same number of UpvarCookies as there are lexdeps. */
            size_t lexdepCount = bce->roLexdeps->count();

            JS_ASSERT_IF(!upvarMap.empty(), lexdepCount == upvarMap.length());
            if (upvarMap.empty()) {
                /* Lazily initialize the upvar map with exactly the necessary capacity. */
                if (lexdepCount <= upvarMap.sMaxInlineStorage) {
                    JS_ALWAYS_TRUE(upvarMap.growByUninitialized(lexdepCount));
                } else {
                    void *buf = upvarMap.allocPolicy().malloc_(lexdepCount * sizeof(UpvarCookie));
                    if (!buf)
                        return JS_FALSE;
                    upvarMap.replaceRawBuffer(static_cast<UpvarCookie *>(buf), lexdepCount);
                }
                for (size_t i = 0; i < lexdepCount; ++i)
                    upvarMap[i] = UpvarCookie();
            }

            uintN slot = cookie.slot();
            if (slot != UpvarCookie::CALLEE_SLOT && dn_kind != Definition::ARG) {
                TreeContext *tc = bce;
                do {
                    tc = tc->parent;
                } while (tc->staticLevel != level);
                if (tc->inFunction())
                    slot += tc->fun()->nargs;
            }

            JS_ASSERT(index < upvarMap.length());
            upvarMap[index].set(skip, slot);
        }

        pn->setOp(JSOP_GETFCSLOT);
        JS_ASSERT((index & JS_BITMASK(16)) == index);
        pn->pn_cookie.set(0, index);
        pn->pn_dflags |= PND_BOUND;
        return JS_TRUE;
    }

    /*
     * We are compiling a function body and may be able to optimize name
     * to stack slot. Look for an argument or variable in the function and
     * rewrite pn_op and update pn accordingly.
     */
    switch (dn_kind) {
      case Definition::UNKNOWN:
        return JS_TRUE;

      case Definition::LET:
        switch (op) {
          case JSOP_NAME:     op = JSOP_GETLOCAL; break;
          case JSOP_SETNAME:  op = JSOP_SETLOCAL; break;
          case JSOP_INCNAME:  op = JSOP_INCLOCAL; break;
          case JSOP_NAMEINC:  op = JSOP_LOCALINC; break;
          case JSOP_DECNAME:  op = JSOP_DECLOCAL; break;
          case JSOP_NAMEDEC:  op = JSOP_LOCALDEC; break;
          default: JS_NOT_REACHED("let");
        }
        break;

      case Definition::ARG:
        switch (op) {
          case JSOP_NAME:     op = JSOP_GETARG; break;
          case JSOP_SETNAME:  op = JSOP_SETARG; break;
          case JSOP_INCNAME:  op = JSOP_INCARG; break;
          case JSOP_NAMEINC:  op = JSOP_ARGINC; break;
          case JSOP_DECNAME:  op = JSOP_DECARG; break;
          case JSOP_NAMEDEC:  op = JSOP_ARGDEC; break;
          default: JS_NOT_REACHED("arg");
        }
        JS_ASSERT(!pn->isConst());
        break;

      case Definition::VAR:
        if (dn->isOp(JSOP_CALLEE)) {
            JS_ASSERT(op != JSOP_CALLEE);
            JS_ASSERT((bce->fun()->flags & JSFUN_LAMBDA) && atom == bce->fun()->atom);

            /*
             * Leave pn->isOp(JSOP_NAME) if bce->fun is heavyweight to
             * address two cases: a new binding introduced by eval, and
             * assignment to the name in strict mode.
             *
             *   var fun = (function f(s) { eval(s); return f; });
             *   assertEq(fun("var f = 42"), 42);
             *
             * ECMAScript specifies that a function expression's name is bound
             * in a lexical environment distinct from that used to bind its
             * named parameters, the arguments object, and its variables.  The
             * new binding for "var f = 42" shadows the binding for the
             * function itself, so the name of the function will not refer to
             * the function.
             *
             *    (function f() { "use strict"; f = 12; })();
             *
             * Outside strict mode, assignment to a function expression's name
             * has no effect.  But in strict mode, this attempt to mutate an
             * immutable binding must throw a TypeError.  We implement this by
             * not optimizing such assignments and by marking such functions as
             * heavyweight, ensuring that the function name is represented in
             * the scope chain so that assignment will throw a TypeError.
             */
            JS_ASSERT(op != JSOP_DELNAME);
            if (!(bce->flags & TCF_FUN_HEAVYWEIGHT)) {
                op = JSOP_CALLEE;
                pn->pn_dflags |= PND_CONST;
            }

            pn->setOp(op);
            pn->pn_dflags |= PND_BOUND;
            return JS_TRUE;
        }
        /* FALL THROUGH */

      default:
        JS_ASSERT_IF(dn_kind != Definition::FUNCTION,
                     dn_kind == Definition::VAR ||
                     dn_kind == Definition::CONST);
        switch (op) {
          case JSOP_NAME:     op = JSOP_GETLOCAL; break;
          case JSOP_SETNAME:  op = JSOP_SETLOCAL; break;
          case JSOP_SETCONST: op = JSOP_SETLOCAL; break;
          case JSOP_INCNAME:  op = JSOP_INCLOCAL; break;
          case JSOP_NAMEINC:  op = JSOP_LOCALINC; break;
          case JSOP_DECNAME:  op = JSOP_DECLOCAL; break;
          case JSOP_NAMEDEC:  op = JSOP_LOCALDEC; break;
          default: JS_NOT_REACHED("local");
        }
        JS_ASSERT_IF(dn_kind == Definition::CONST, pn->pn_dflags & PND_CONST);
        break;
    }

    JS_ASSERT(!pn->isOp(op));
    pn->setOp(op);
    pn->pn_cookie.set(0, cookie.slot());
    pn->pn_dflags |= PND_BOUND;
    return JS_TRUE;
}

bool
BytecodeEmitter::addGlobalUse(JSAtom *atom, uint32 slot, UpvarCookie *cookie)
{
    if (!globalMap.ensureMap(context()))
        return false;

    AtomIndexAddPtr p = globalMap->lookupForAdd(atom);
    if (p) {
        jsatomid index = p.value();
        cookie->set(0, index);
        return true;
    }

    /* Don't bother encoding indexes >= uint16 */
    if (globalUses.length() >= UINT16_LIMIT) {
        cookie->makeFree();
        return true;
    }

    /* Find or add an existing atom table entry. */
    jsatomid allAtomIndex;
    if (!makeAtomIndex(atom, &allAtomIndex))
        return false;

    jsatomid globalUseIndex = globalUses.length();
    cookie->set(0, globalUseIndex);

    GlobalSlotArray::Entry entry = { allAtomIndex, slot };
    if (!globalUses.append(entry))
        return false;

    return globalMap->add(p, atom, globalUseIndex);
}

/*
 * If pn contains a useful expression, return true with *answer set to true.
 * If pn contains a useless expression, return true with *answer set to false.
 * Return false on error.
 *
 * The caller should initialize *answer to false and invoke this function on
 * an expression statement or similar subtree to decide whether the tree could
 * produce code that has any side effects.  For an expression statement, we
 * define useless code as code with no side effects, because the main effect,
 * the value left on the stack after the code executes, will be discarded by a
 * pop bytecode.
 */
static JSBool
CheckSideEffects(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, JSBool *answer)
{
    JSBool ok;
    ParseNode *pn2;

    ok = JS_TRUE;
    if (!pn || *answer)
        return ok;

    switch (pn->getArity()) {
      case PN_FUNC:
        /*
         * A named function, contrary to ES3, is no longer useful, because we
         * bind its name lexically (using JSOP_CALLEE) instead of creating an
         * Object instance and binding a readonly, permanent property in it
         * (the object and binding can be detected and hijacked or captured).
         * This is a bug fix to ES3; it is fixed in ES3.1 drafts.
         */
        *answer = JS_FALSE;
        break;

      case PN_LIST:
        if (pn->isOp(JSOP_NOP) || pn->isOp(JSOP_OR) || pn->isOp(JSOP_AND) ||
            pn->isOp(JSOP_STRICTEQ) || pn->isOp(JSOP_STRICTNE)) {
            /*
             * Non-operators along with ||, &&, ===, and !== never invoke
             * toString or valueOf.
             */
            for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next)
                ok &= CheckSideEffects(cx, bce, pn2, answer);
        } else {
            /*
             * All invocation operations (construct: PNK_NEW, call: PNK_LP)
             * are presumed to be useful, because they may have side effects
             * even if their main effect (their return value) is discarded.
             *
             * PNK_LB binary trees of 3 or more nodes are flattened into lists
             * to avoid too much recursion.  All such lists must be presumed
             * to be useful because each index operation could invoke a getter
             * (the JSOP_ARGUMENTS special case below, in the PN_BINARY case,
             * does not apply here: arguments[i][j] might invoke a getter).
             *
             * Likewise, array and object initialisers may call prototype
             * setters (the __defineSetter__ built-in, and writable __proto__
             * on Array.prototype create this hazard). Initialiser list nodes
             * have JSOP_NEWINIT in their pn_op.
             */
            *answer = JS_TRUE;
        }
        break;

      case PN_TERNARY:
        ok = CheckSideEffects(cx, bce, pn->pn_kid1, answer) &&
             CheckSideEffects(cx, bce, pn->pn_kid2, answer) &&
             CheckSideEffects(cx, bce, pn->pn_kid3, answer);
        break;

      case PN_BINARY:
        if (pn->isAssignment()) {
            /*
             * Assignment is presumed to be useful, even if the next operation
             * is another assignment overwriting this one's ostensible effect,
             * because the left operand may be a property with a setter that
             * has side effects.
             *
             * The only exception is assignment of a useless value to a const
             * declared in the function currently being compiled.
             */
            pn2 = pn->pn_left;
            if (!pn2->isKind(PNK_NAME)) {
                *answer = JS_TRUE;
            } else {
                if (!BindNameToSlot(cx, bce, pn2))
                    return JS_FALSE;
                if (!CheckSideEffects(cx, bce, pn->pn_right, answer))
                    return JS_FALSE;
                if (!*answer && (!pn->isOp(JSOP_NOP) || !pn2->isConst()))
                    *answer = JS_TRUE;
            }
        } else {
            if (pn->isOp(JSOP_OR) || pn->isOp(JSOP_AND) || pn->isOp(JSOP_STRICTEQ) ||
                pn->isOp(JSOP_STRICTNE)) {
                /*
                 * ||, &&, ===, and !== do not convert their operands via
                 * toString or valueOf method calls.
                 */
                ok = CheckSideEffects(cx, bce, pn->pn_left, answer) &&
                     CheckSideEffects(cx, bce, pn->pn_right, answer);
            } else {
                /*
                 * We can't easily prove that neither operand ever denotes an
                 * object with a toString or valueOf method.
                 */
                *answer = JS_TRUE;
            }
        }
        break;

      case PN_UNARY:
        switch (pn->getKind()) {
          case PNK_DELETE:
            pn2 = pn->pn_kid;
            switch (pn2->getKind()) {
              case PNK_NAME:
                if (!BindNameToSlot(cx, bce, pn2))
                    return JS_FALSE;
                if (pn2->isConst()) {
                    *answer = JS_FALSE;
                    break;
                }
                /* FALL THROUGH */
              case PNK_DOT:
#if JS_HAS_XML_SUPPORT
              case PNK_DBLDOT:
                JS_ASSERT_IF(pn2->getKind() == PNK_DBLDOT, !bce->inStrictMode());
                /* FALL THROUGH */

#endif
              case PNK_LP:
              case PNK_LB:
                /* All these delete addressing modes have effects too. */
                *answer = JS_TRUE;
                break;
              default:
                ok = CheckSideEffects(cx, bce, pn2, answer);
                break;
            }
            break;

          case PNK_TYPEOF:
          case PNK_VOID:
          case PNK_NOT:
          case PNK_BITNOT:
            if (pn->isOp(JSOP_NOT)) {
                /* ! does not convert its operand via toString or valueOf. */
                ok = CheckSideEffects(cx, bce, pn->pn_kid, answer);
                break;
            }
            /* FALL THROUGH */

          default:
            /*
             * All of PNK_INC, PNK_DEC, PNK_THROW, PNK_YIELD, and PNK_DEFSHARP
             * have direct effects. Of the remaining unary-arity node types,
             * we can't easily prove that the operand never denotes an object
             * with a toString or valueOf method.
             */
            *answer = JS_TRUE;
            break;
        }
        break;

      case PN_NAME:
        /*
         * Take care to avoid trying to bind a label name (labels, both for
         * statements and property values in object initialisers, have pn_op
         * defaulted to JSOP_NOP).
         */
        if (pn->isKind(PNK_NAME) && !pn->isOp(JSOP_NOP)) {
            if (!BindNameToSlot(cx, bce, pn))
                return JS_FALSE;
            if (!pn->isOp(JSOP_ARGUMENTS) && !pn->isOp(JSOP_CALLEE) &&
                pn->pn_cookie.isFree()) {
                /*
                 * Not an argument or local variable use, and not a use of a
                 * unshadowed named function expression's given name, so this
                 * expression could invoke a getter that has side effects.
                 */
                *answer = JS_TRUE;
            }
        }
        pn2 = pn->maybeExpr();
        if (pn->isKind(PNK_DOT)) {
            if (pn2->isKind(PNK_NAME) && !BindNameToSlot(cx, bce, pn2))
                return JS_FALSE;
            if (!(pn2->isOp(JSOP_ARGUMENTS) &&
                  pn->pn_atom == cx->runtime->atomState.lengthAtom)) {
                /*
                 * Any dotted property reference could call a getter, except
                 * for arguments.length where arguments is unambiguous.
                 */
                *answer = JS_TRUE;
            }
        }
        ok = CheckSideEffects(cx, bce, pn2, answer);
        break;

      case PN_NAMESET:
        ok = CheckSideEffects(cx, bce, pn->pn_tree, answer);
        break;

      case PN_NULLARY:
        if (pn->isKind(PNK_DEBUGGER))
            *answer = JS_TRUE;
        break;
    }
    return ok;
}

static JSBool
EmitNameOp(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, JSBool callContext)
{
    JSOp op;

    if (!BindNameToSlot(cx, bce, pn))
        return JS_FALSE;
    op = pn->getOp();

    if (callContext) {
        switch (op) {
          case JSOP_NAME:
            op = JSOP_CALLNAME;
            break;
          case JSOP_GETGNAME:
            op = JSOP_CALLGNAME;
            break;
          case JSOP_GETARG:
            op = JSOP_CALLARG;
            break;
          case JSOP_GETLOCAL:
            op = JSOP_CALLLOCAL;
            break;
          case JSOP_GETFCSLOT:
            op = JSOP_CALLFCSLOT;
            break;
          default:
            JS_ASSERT(op == JSOP_ARGUMENTS || op == JSOP_CALLEE);
            break;
        }
    }

    if (op == JSOP_ARGUMENTS || op == JSOP_CALLEE) {
        if (Emit1(cx, bce, op) < 0)
            return JS_FALSE;
        if (callContext && Emit1(cx, bce, JSOP_PUSH) < 0)
            return JS_FALSE;
    } else {
        if (!pn->pn_cookie.isFree()) {
            EMIT_UINT16_IMM_OP(op, pn->pn_cookie.asInteger());
        } else {
            if (!EmitAtomOp(cx, pn, op, bce))
                return JS_FALSE;
        }
    }

    return JS_TRUE;
}

#if JS_HAS_XML_SUPPORT
static bool
EmitXMLName(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    JS_ASSERT(!bce->inStrictMode());
    JS_ASSERT(pn->isKind(PNK_XMLUNARY));
    JS_ASSERT(pn->isOp(JSOP_XMLNAME));
    JS_ASSERT(op == JSOP_XMLNAME || op == JSOP_CALLXMLNAME);

    ParseNode *pn2 = pn->pn_kid;
    uintN oldflags = bce->flags;
    bce->flags &= ~TCF_IN_FOR_INIT;
    if (!EmitTree(cx, bce, pn2))
        return false;
    bce->flags |= oldflags & TCF_IN_FOR_INIT;
    if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - pn2->pn_offset) < 0)
        return false;

    return Emit1(cx, bce, op) >= 0;
}
#endif

static inline bool
EmitElemOpBase(JSContext *cx, BytecodeEmitter *bce, JSOp op)
{
    if (Emit1(cx, bce, op) < 0)
        return false;
    CheckTypeSet(cx, bce, op);
    return true;
}

static bool
EmitSpecialPropOp(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    /*
     * Special case for obj.__proto__ to deoptimize away from fast paths in the
     * interpreter and trace recorder, which skip dense array instances by
     * going up to Array.prototype before looking up the property name.
     */
    jsatomid index;
    if (!bce->makeAtomIndex(pn->pn_atom, &index))
        return false;
    if (!EmitIndexOp(cx, JSOP_QNAMEPART, index, bce))
        return false;
    return EmitElemOpBase(cx, bce, op);
}

static bool
EmitPropOp(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce,
           JSBool callContext, JSOp *psuffix = NULL)
{
    ParseNode *pn2, *pndot, *pnup, *pndown;
    ptrdiff_t top;

    JS_ASSERT(pn->isArity(PN_NAME));
    pn2 = pn->maybeExpr();

    /* Special case deoptimization for __proto__. */
    if ((op == JSOP_GETPROP || op == JSOP_CALLPROP) &&
        pn->pn_atom == cx->runtime->atomState.protoAtom) {
        if (pn2 && !EmitTree(cx, bce, pn2))
            return false;
        return EmitSpecialPropOp(cx, pn, callContext ? JSOP_CALLELEM : JSOP_GETELEM, bce);
    }

    if (callContext) {
        JS_ASSERT(pn->isKind(PNK_DOT));
        JS_ASSERT(op == JSOP_GETPROP);
        op = JSOP_CALLPROP;
    } else if (op == JSOP_GETPROP && pn->isKind(PNK_DOT)) {
        if (pn2->isKind(PNK_NAME)) {
            if (!BindNameToSlot(cx, bce, pn2))
                return false;
        }
    }

    /*
     * If the object operand is also a dotted property reference, reverse the
     * list linked via pn_expr temporarily so we can iterate over it from the
     * bottom up (reversing again as we go), to avoid excessive recursion.
     */
    if (pn2->isKind(PNK_DOT)) {
        pndot = pn2;
        pnup = NULL;
        top = bce->offset();
        for (;;) {
            /* Reverse pndot->pn_expr to point up, not down. */
            pndot->pn_offset = top;
            JS_ASSERT(!pndot->isUsed());
            pndown = pndot->pn_expr;
            pndot->pn_expr = pnup;
            if (!pndown->isKind(PNK_DOT))
                break;
            pnup = pndot;
            pndot = pndown;
        }

        /* pndown is a primary expression, not a dotted property reference. */
        if (!EmitTree(cx, bce, pndown))
            return false;

        do {
            /* Walk back up the list, emitting annotated name ops. */
            if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - pndown->pn_offset) < 0)
                return false;

            /* Special case deoptimization on __proto__, as above. */
            if (pndot->isArity(PN_NAME) && pndot->pn_atom == cx->runtime->atomState.protoAtom) {
                if (!EmitSpecialPropOp(cx, pndot, JSOP_GETELEM, bce))
                    return false;
            } else if (!EmitAtomOp(cx, pndot, pndot->getOp(), bce)) {
                return false;
            }

            /* Reverse the pn_expr link again. */
            pnup = pndot->pn_expr;
            pndot->pn_expr = pndown;
            pndown = pndot;
        } while ((pndot = pnup) != NULL);
    } else {
        if (!EmitTree(cx, bce, pn2))
            return false;
    }

    if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - pn2->pn_offset) < 0)
        return false;

    return EmitAtomOp(cx, pn, op, bce, psuffix);
}

static bool
EmitPropIncDec(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    JSOp suffix = JSOP_NOP;
    if (!EmitPropOp(cx, pn, op, bce, false, &suffix))
        return false;
    if (Emit1(cx, bce, JSOP_NOP) < 0)
        return false;

    /*
     * The stack is the same depth before/after INCPROP, so no balancing to do
     * before the decomposed version.
     */
    int start = bce->offset();

    if (suffix != JSOP_NOP && Emit1(cx, bce, suffix) < 0)
        return false;

    const JSCodeSpec *cs = &js_CodeSpec[op];
    JS_ASSERT(cs->format & JOF_PROP);
    JS_ASSERT(cs->format & (JOF_INC | JOF_DEC));

    bool post = (cs->format & JOF_POST);
    JSOp binop = (cs->format & JOF_INC) ? JSOP_ADD : JSOP_SUB;

                                                    // OBJ
    if (Emit1(cx, bce, JSOP_DUP) < 0)               // OBJ OBJ
        return false;
    if (!EmitAtomOp(cx, pn, JSOP_GETPROP, bce))     // OBJ V
        return false;
    if (Emit1(cx, bce, JSOP_POS) < 0)               // OBJ N
        return false;
    if (post && Emit1(cx, bce, JSOP_DUP) < 0)       // OBJ N? N
        return false;
    if (Emit1(cx, bce, JSOP_ONE) < 0)               // OBJ N? N 1
        return false;
    if (Emit1(cx, bce, binop) < 0)                  // OBJ N? N+1
        return false;

    if (post) {
        if (Emit2(cx, bce, JSOP_PICK, (jsbytecode)2) < 0)   // N? N+1 OBJ
            return false;
        if (Emit1(cx, bce, JSOP_SWAP) < 0)                  // N? OBJ N+1
            return false;
    }

    if (!EmitAtomOp(cx, pn, JSOP_SETPROP, bce))     // N? N+1
        return false;
    if (post && Emit1(cx, bce, JSOP_POP) < 0)       // RESULT
        return false;

    UpdateDecomposeLength(bce, start);

    if (suffix != JSOP_NOP && Emit1(cx, bce, suffix) < 0)
        return false;

    return true;
}

static bool
EmitNameIncDec(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    JSOp suffix = JSOP_NOP;
    if (!EmitAtomOp(cx, pn, op, bce, &suffix))
        return false;
    if (Emit1(cx, bce, JSOP_NOP) < 0)
        return false;

    /* Remove the result to restore the stack depth before the INCNAME. */
    bce->stackDepth--;

    int start = bce->offset();

    if (suffix != JSOP_NOP && Emit1(cx, bce, suffix) < 0)
        return false;

    const JSCodeSpec *cs = &js_CodeSpec[op];
    JS_ASSERT((cs->format & JOF_NAME) || (cs->format & JOF_GNAME));
    JS_ASSERT(cs->format & (JOF_INC | JOF_DEC));

    bool global = (cs->format & JOF_GNAME);
    bool post = (cs->format & JOF_POST);
    JSOp binop = (cs->format & JOF_INC) ? JSOP_ADD : JSOP_SUB;

    if (!EmitAtomOp(cx, pn, global ? JSOP_BINDGNAME : JSOP_BINDNAME, bce))  // OBJ
        return false;
    if (!EmitAtomOp(cx, pn, global ? JSOP_GETGNAME : JSOP_NAME, bce))       // OBJ V
        return false;
    if (Emit1(cx, bce, JSOP_POS) < 0)               // OBJ N
        return false;
    if (post && Emit1(cx, bce, JSOP_DUP) < 0)       // OBJ N? N
        return false;
    if (Emit1(cx, bce, JSOP_ONE) < 0)               // OBJ N? N 1
        return false;
    if (Emit1(cx, bce, binop) < 0)                  // OBJ N? N+1
        return false;

    if (post) {
        if (Emit2(cx, bce, JSOP_PICK, (jsbytecode)2) < 0)   // N? N+1 OBJ
            return false;
        if (Emit1(cx, bce, JSOP_SWAP) < 0)                  // N? OBJ N+1
            return false;
    }

    if (!EmitAtomOp(cx, pn, global ? JSOP_SETGNAME : JSOP_SETNAME, bce))    // N? N+1
        return false;
    if (post && Emit1(cx, bce, JSOP_POP) < 0)       // RESULT
        return false;

    UpdateDecomposeLength(bce, start);

    if (suffix != JSOP_NOP && Emit1(cx, bce, suffix) < 0)
        return false;

    return true;
}

static JSBool
EmitElemOp(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    ParseNode *left, *right;

    ptrdiff_t top = bce->offset();

    if (pn->isArity(PN_NAME)) {
        /*
         * Set left and right so pn appears to be a PNK_LB node, instead
         * of a PNK_DOT node.  See the PNK_FOR/IN case in EmitTree, and
         * EmitDestructuringOps nearer below.  In the destructuring case,
         * the base expression (pn_expr) of the name may be null, which
         * means we have to emit a JSOP_BINDNAME.
         */
        left = pn->maybeExpr();
        if (!left) {
            left = NullaryNode::create(PNK_STRING, bce);
            if (!left)
                return false;
            left->setOp(JSOP_BINDNAME);
            left->pn_pos = pn->pn_pos;
            left->pn_atom = pn->pn_atom;
        }
        right = NullaryNode::create(PNK_STRING, bce);
        if (!right)
            return false;
        right->setOp(IsIdentifier(pn->pn_atom) ? JSOP_QNAMEPART : JSOP_STRING);
        right->pn_pos = pn->pn_pos;
        right->pn_atom = pn->pn_atom;
    } else {
        JS_ASSERT(pn->isArity(PN_BINARY));
        left = pn->pn_left;
        right = pn->pn_right;
    }

    if (op == JSOP_GETELEM && left->isKind(PNK_NAME) && right->isKind(PNK_NUMBER)) {
        if (!BindNameToSlot(cx, bce, left))
            return false;
    }

    if (!EmitTree(cx, bce, left))
        return false;

    /* The right side of the descendant operator is implicitly quoted. */
    JS_ASSERT(op != JSOP_DESCENDANTS || !right->isKind(PNK_STRING) ||
              right->isOp(JSOP_QNAMEPART));
    if (!EmitTree(cx, bce, right))
        return false;
    if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - top) < 0)
        return false;
    return EmitElemOpBase(cx, bce, op);
}

static bool
EmitElemIncDec(JSContext *cx, ParseNode *pn, JSOp op, BytecodeEmitter *bce)
{
    if (pn) {
        if (!EmitElemOp(cx, pn, op, bce))
            return false;
    } else {
        if (!EmitElemOpBase(cx, bce, op))
            return false;
    }
    if (Emit1(cx, bce, JSOP_NOP) < 0)
        return false;

    /* INCELEM pops two values and pushes one, so restore the initial depth. */
    bce->stackDepth++;

    int start = bce->offset();

    const JSCodeSpec *cs = &js_CodeSpec[op];
    JS_ASSERT(cs->format & JOF_ELEM);
    JS_ASSERT(cs->format & (JOF_INC | JOF_DEC));

    bool post = (cs->format & JOF_POST);
    JSOp binop = (cs->format & JOF_INC) ? JSOP_ADD : JSOP_SUB;

    /*
     * We need to convert the key to an object id first, so that we do not do
     * it inside both the GETELEM and the SETELEM.
     */
                                                    // OBJ KEY*
    if (Emit1(cx, bce, JSOP_TOID) < 0)              // OBJ KEY
        return false;
    if (Emit1(cx, bce, JSOP_DUP2) < 0)              // OBJ KEY OBJ KEY
        return false;
    if (!EmitElemOpBase(cx, bce, JSOP_GETELEM))     // OBJ KEY V
        return false;
    if (Emit1(cx, bce, JSOP_POS) < 0)               // OBJ KEY N
        return false;
    if (post && Emit1(cx, bce, JSOP_DUP) < 0)       // OBJ KEY N? N
        return false;
    if (Emit1(cx, bce, JSOP_ONE) < 0)               // OBJ KEY N? N 1
        return false;
    if (Emit1(cx, bce, binop) < 0)                  // OBJ KEY N? N+1
        return false;

    if (post) {
        if (Emit2(cx, bce, JSOP_PICK, (jsbytecode)3) < 0)   // KEY N N+1 OBJ
            return false;
        if (Emit2(cx, bce, JSOP_PICK, (jsbytecode)3) < 0)   // N N+1 OBJ KEY
            return false;
        if (Emit2(cx, bce, JSOP_PICK, (jsbytecode)2) < 0)   // N OBJ KEY N+1
            return false;
    }

    if (!EmitElemOpBase(cx, bce, JSOP_SETELEM))     // N? N+1
        return false;
    if (post && Emit1(cx, bce, JSOP_POP) < 0)       // RESULT
        return false;

    UpdateDecomposeLength(bce, start);

    return true;
}

static JSBool
EmitNumberOp(JSContext *cx, jsdouble dval, BytecodeEmitter *bce)
{
    int32_t ival;
    uint32 u;
    ptrdiff_t off;
    jsbytecode *pc;

    if (JSDOUBLE_IS_INT32(dval, &ival)) {
        if (ival == 0)
            return Emit1(cx, bce, JSOP_ZERO) >= 0;
        if (ival == 1)
            return Emit1(cx, bce, JSOP_ONE) >= 0;
        if ((jsint)(int8)ival == ival)
            return Emit2(cx, bce, JSOP_INT8, (jsbytecode)(int8)ival) >= 0;

        u = (uint32)ival;
        if (u < JS_BIT(16)) {
            EMIT_UINT16_IMM_OP(JSOP_UINT16, u);
        } else if (u < JS_BIT(24)) {
            off = EmitN(cx, bce, JSOP_UINT24, 3);
            if (off < 0)
                return JS_FALSE;
            pc = bce->code(off);
            SET_UINT24(pc, u);
        } else {
            off = EmitN(cx, bce, JSOP_INT32, 4);
            if (off < 0)
                return JS_FALSE;
            pc = bce->code(off);
            SET_INT32(pc, ival);
        }
        return JS_TRUE;
    }

    if (!bce->constList.append(DoubleValue(dval)))
        return JS_FALSE;

    return EmitIndexOp(cx, JSOP_DOUBLE, bce->constList.length() - 1, bce);
}

/*
 * To avoid bloating all parse nodes for the special case of switch, values are
 * allocated in the temp pool and pointed to by the parse node. These values
 * are not currently recycled (like parse nodes) and the temp pool is only
 * flushed at the end of compiling a script, so these values are technically
 * leaked. This would only be a problem for scripts containing a large number
 * of large switches, which seems unlikely.
 */
static Value *
AllocateSwitchConstant(JSContext *cx)
{
    return cx->tempLifoAlloc().new_<Value>();
}

/*
 * Sometimes, let-slots are pushed to the JS stack before we logically enter
 * the let scope. For example,
 *     let (x = EXPR) BODY
 * compiles to roughly {enterblock; EXPR; setlocal x; BODY; leaveblock} even
 * though EXPR is evaluated in the enclosing scope; it does not see x.
 *
 * In those cases we use TempPopScope around the code to emit EXPR. It
 * temporarily removes the let-scope from the BytecodeEmitter's scope stack and
 * emits extra bytecode to ensure that js::GetBlockChain also finds the correct
 * scope at run time.
 */
class TempPopScope {
    StmtInfo *savedStmt;
    StmtInfo *savedScopeStmt;
    ObjectBox *savedBlockBox;

  public:
    TempPopScope() : savedStmt(NULL), savedScopeStmt(NULL), savedBlockBox(NULL) {}

    bool popBlock(JSContext *cx, BytecodeEmitter *bce) {
        savedStmt = bce->topStmt;
        savedScopeStmt = bce->topScopeStmt;
        savedBlockBox = bce->blockChainBox;

        if (bce->topStmt->type == STMT_FOR_LOOP || bce->topStmt->type == STMT_FOR_IN_LOOP)
            PopStatementTC(bce);
        JS_ASSERT(STMT_LINKS_SCOPE(bce->topStmt));
        JS_ASSERT(bce->topStmt->flags & SIF_SCOPE);
        PopStatementTC(bce);

        /*
         * Since we have changed the block chain, emit an instruction marking
         * the change for the benefit of dynamic GetScopeChain callers such as
         * the debugger.
         *
         * FIXME bug 671360 - The JSOP_NOP instruction should not be necessary.
         */
        return Emit1(cx, bce, JSOP_NOP) >= 0 && EmitBlockChain(cx, bce);
    }

    bool repushBlock(JSContext *cx, BytecodeEmitter *bce) {
        JS_ASSERT(savedStmt);
        bce->topStmt = savedStmt;
        bce->topScopeStmt = savedScopeStmt;
        bce->blockChainBox = savedBlockBox;
        return Emit1(cx, bce, JSOP_NOP) >= 0 && EmitBlockChain(cx, bce);
    }
};

static JSBool
EmitSwitch(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    JSOp switchOp;
    JSBool ok, hasDefault, constPropagated;
    ptrdiff_t top, off, defaultOffset;
    ParseNode *pn2, *pn3, *pn4;
    uint32 caseCount, tableLength;
    ParseNode **table;
    int32_t i, low, high;
    intN noteIndex;
    size_t switchSize, tableSize;
    jsbytecode *pc, *savepc;
#if JS_HAS_BLOCK_SCOPE
    ObjectBox *box;
#endif
    StmtInfo stmtInfo;

    /* Try for most optimal, fall back if not dense ints, and per ECMAv2. */
    switchOp = JSOP_TABLESWITCH;
    ok = JS_TRUE;
    hasDefault = constPropagated = JS_FALSE;
    defaultOffset = -1;

    /*
     * If the switch contains let variables scoped by its body, model the
     * resulting block on the stack first, before emitting the discriminant's
     * bytecode (in case the discriminant contains a stack-model dependency
     * such as a let expression).
     */
    pn2 = pn->pn_right;
#if JS_HAS_BLOCK_SCOPE
    TempPopScope tps;
    if (pn2->isKind(PNK_LEXICALSCOPE)) {
        /*
         * Push the body's block scope before discriminant code-gen to reflect
         * the order of slots on the stack. The block's locals must lie under
         * the discriminant on the stack so that case-dispatch bytecodes can
         * find the discriminant on top of stack.
         */
        box = pn2->pn_objbox;
        PushBlockScope(bce, &stmtInfo, box, -1);
        stmtInfo.type = STMT_SWITCH;

        /* Emit JSOP_ENTERBLOCK before code to evaluate the discriminant. */
        if (!EmitEnterBlock(cx, pn2, bce))
            return JS_FALSE;

        /*
         * Pop the switch's statement info around discriminant code-gen, which
         * belongs in the enclosing scope.
         */
        if (!tps.popBlock(cx, bce))
            return JS_FALSE;
    }
#ifdef __GNUC__
    else {
        box = NULL;
    }
#endif
#endif

    /*
     * Emit code for the discriminant first (or nearly first, in the case of a
     * switch whose body is a block scope).
     */
    if (!EmitTree(cx, bce, pn->pn_left))
        return JS_FALSE;

    /* Switch bytecodes run from here till end of final case. */
    top = bce->offset();
#if !JS_HAS_BLOCK_SCOPE
    PushStatement(bce, &stmtInfo, STMT_SWITCH, top);
#else
    if (pn2->isKind(PNK_STATEMENTLIST)) {
        PushStatement(bce, &stmtInfo, STMT_SWITCH, top);
    } else {
        /* Re-push the switch's statement info record. */
        if (!tps.repushBlock(cx, bce))
            return JS_FALSE;

        /*
         * Set the statement info record's idea of top. Reset top too, since
         * repushBlock emits code.
         */
        stmtInfo.update = top = bce->offset();

        /* Advance pn2 to refer to the switch case list. */
        pn2 = pn2->expr();
    }
#endif

    caseCount = pn2->pn_count;
    tableLength = 0;
    table = NULL;

    if (caseCount == 0 ||
        (caseCount == 1 &&
         (hasDefault = (pn2->pn_head->isKind(PNK_DEFAULT))))) {
        caseCount = 0;
        low = 0;
        high = -1;
    } else {
#define INTMAP_LENGTH   256
        jsbitmap intmap_space[INTMAP_LENGTH];
        jsbitmap *intmap = NULL;
        int32 intmap_bitlen = 0;

        low  = JSVAL_INT_MAX;
        high = JSVAL_INT_MIN;

        for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
            if (pn3->isKind(PNK_DEFAULT)) {
                hasDefault = JS_TRUE;
                caseCount--;    /* one of the "cases" was the default */
                continue;
            }

            JS_ASSERT(pn3->isKind(PNK_CASE));
            if (switchOp == JSOP_CONDSWITCH)
                continue;

            pn4 = pn3->pn_left;
            while (pn4->isKind(PNK_RP))
                pn4 = pn4->pn_kid;

            Value constVal;
            switch (pn4->getKind()) {
              case PNK_NUMBER:
                constVal.setNumber(pn4->pn_dval);
                break;
              case PNK_STRING:
                constVal.setString(pn4->pn_atom);
                break;
              case PNK_TRUE:
                constVal.setBoolean(true);
                break;
              case PNK_FALSE:
                constVal.setBoolean(false);
                break;
              case PNK_NULL:
                constVal.setNull();
                break;
              case PNK_NAME:
                if (!pn4->maybeExpr()) {
                    ok = LookupCompileTimeConstant(cx, bce, pn4->pn_atom, &constVal);
                    if (!ok)
                        goto release;
                    if (!constVal.isMagic(JS_NO_CONSTANT)) {
                        if (constVal.isObject()) {
                            /*
                             * XXX JSOP_LOOKUPSWITCH does not support const-
                             * propagated object values, see bug 407186.
                             */
                            switchOp = JSOP_CONDSWITCH;
                            continue;
                        }
                        constPropagated = JS_TRUE;
                        break;
                    }
                }
                /* FALL THROUGH */
              default:
                switchOp = JSOP_CONDSWITCH;
                continue;
            }
            JS_ASSERT(constVal.isPrimitive());

            pn3->pn_pval = AllocateSwitchConstant(cx);
            if (!pn3->pn_pval) {
                ok = JS_FALSE;
                goto release;
            }

            *pn3->pn_pval = constVal;

            if (switchOp != JSOP_TABLESWITCH)
                continue;
            if (!pn3->pn_pval->isInt32()) {
                switchOp = JSOP_LOOKUPSWITCH;
                continue;
            }
            i = pn3->pn_pval->toInt32();
            if ((jsuint)(i + (jsint)JS_BIT(15)) >= (jsuint)JS_BIT(16)) {
                switchOp = JSOP_LOOKUPSWITCH;
                continue;
            }
            if (i < low)
                low = i;
            if (high < i)
                high = i;

            /*
             * Check for duplicates, which require a JSOP_LOOKUPSWITCH.
             * We bias i by 65536 if it's negative, and hope that's a rare
             * case (because it requires a malloc'd bitmap).
             */
            if (i < 0)
                i += JS_BIT(16);
            if (i >= intmap_bitlen) {
                if (!intmap &&
                    i < (INTMAP_LENGTH << JS_BITS_PER_WORD_LOG2)) {
                    intmap = intmap_space;
                    intmap_bitlen = INTMAP_LENGTH << JS_BITS_PER_WORD_LOG2;
                } else {
                    /* Just grab 8K for the worst-case bitmap. */
                    intmap_bitlen = JS_BIT(16);
                    intmap = (jsbitmap *)
                        cx->malloc_((JS_BIT(16) >> JS_BITS_PER_WORD_LOG2)
                                   * sizeof(jsbitmap));
                    if (!intmap) {
                        JS_ReportOutOfMemory(cx);
                        return JS_FALSE;
                    }
                }
                memset(intmap, 0, intmap_bitlen >> JS_BITS_PER_BYTE_LOG2);
            }
            if (JS_TEST_BIT(intmap, i)) {
                switchOp = JSOP_LOOKUPSWITCH;
                continue;
            }
            JS_SET_BIT(intmap, i);
        }

      release:
        if (intmap && intmap != intmap_space)
            cx->free_(intmap);
        if (!ok)
            return JS_FALSE;

        /*
         * Compute table length and select lookup instead if overlarge or
         * more than half-sparse.
         */
        if (switchOp == JSOP_TABLESWITCH) {
            tableLength = (uint32)(high - low + 1);
            if (tableLength >= JS_BIT(16) || tableLength > 2 * caseCount)
                switchOp = JSOP_LOOKUPSWITCH;
        } else if (switchOp == JSOP_LOOKUPSWITCH) {
            /*
             * Lookup switch supports only atom indexes below 64K limit.
             * Conservatively estimate the maximum possible index during
             * switch generation and use conditional switch if it exceeds
             * the limit.
             */
            if (caseCount + bce->constList.length() > JS_BIT(16))
                switchOp = JSOP_CONDSWITCH;
        }
    }

    /*
     * Emit a note with two offsets: first tells total switch code length,
     * second tells offset to first JSOP_CASE if condswitch.
     */
    noteIndex = NewSrcNote3(cx, bce, SRC_SWITCH, 0, 0);
    if (noteIndex < 0)
        return JS_FALSE;

    if (switchOp == JSOP_CONDSWITCH) {
        /*
         * 0 bytes of immediate for unoptimized ECMAv2 switch.
         */
        switchSize = 0;
    } else if (switchOp == JSOP_TABLESWITCH) {
        /*
         * 3 offsets (len, low, high) before the table, 1 per entry.
         */
        switchSize = (size_t)(JUMP_OFFSET_LEN * (3 + tableLength));
    } else {
        /*
         * JSOP_LOOKUPSWITCH:
         * 1 offset (len) and 1 atom index (npairs) before the table,
         * 1 atom index and 1 jump offset per entry.
         */
        switchSize = (size_t)(JUMP_OFFSET_LEN + INDEX_LEN +
                              (INDEX_LEN + JUMP_OFFSET_LEN) * caseCount);
    }

    /*
     * Emit switchOp followed by switchSize bytes of jump or lookup table.
     *
     * If switchOp is JSOP_LOOKUPSWITCH or JSOP_TABLESWITCH, it is crucial
     * to emit the immediate operand(s) by which bytecode readers such as
     * BuildSpanDepTable discover the length of the switch opcode *before*
     * calling SetJumpOffset (which may call BuildSpanDepTable).  It's
     * also important to zero all unknown jump offset immediate operands,
     * so they can be converted to span dependencies with null targets to
     * be computed later (EmitN zeros switchSize bytes after switchOp).
     */
    if (EmitN(cx, bce, switchOp, switchSize) < 0)
        return JS_FALSE;

    off = -1;
    if (switchOp == JSOP_CONDSWITCH) {
        intN caseNoteIndex = -1;
        JSBool beforeCases = JS_TRUE;

        /* Emit code for evaluating cases and jumping to case statements. */
        for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
            pn4 = pn3->pn_left;
            if (pn4 && !EmitTree(cx, bce, pn4))
                return JS_FALSE;
            if (caseNoteIndex >= 0) {
                /* off is the previous JSOP_CASE's bytecode offset. */
                if (!SetSrcNoteOffset(cx, bce, (uintN)caseNoteIndex, 0, bce->offset() - off))
                    return JS_FALSE;
            }
            if (!pn4) {
                JS_ASSERT(pn3->isKind(PNK_DEFAULT));
                continue;
            }
            caseNoteIndex = NewSrcNote2(cx, bce, SRC_PCDELTA, 0);
            if (caseNoteIndex < 0)
                return JS_FALSE;
            off = EmitJump(cx, bce, JSOP_CASE, 0);
            if (off < 0)
                return JS_FALSE;
            pn3->pn_offset = off;
            if (beforeCases) {
                uintN noteCount, noteCountDelta;

                /* Switch note's second offset is to first JSOP_CASE. */
                noteCount = bce->noteCount();
                if (!SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 1, off - top))
                    return JS_FALSE;
                noteCountDelta = bce->noteCount() - noteCount;
                if (noteCountDelta != 0)
                    caseNoteIndex += noteCountDelta;
                beforeCases = JS_FALSE;
            }
        }

        /*
         * If we didn't have an explicit default (which could fall in between
         * cases, preventing us from fusing this SetSrcNoteOffset with the call
         * in the loop above), link the last case to the implicit default for
         * the decompiler.
         */
        if (!hasDefault &&
            caseNoteIndex >= 0 &&
            !SetSrcNoteOffset(cx, bce, (uintN)caseNoteIndex, 0, bce->offset() - off))
        {
            return JS_FALSE;
        }

        /* Emit default even if no explicit default statement. */
        defaultOffset = EmitJump(cx, bce, JSOP_DEFAULT, 0);
        if (defaultOffset < 0)
            return JS_FALSE;
    } else {
        pc = bce->code(top + JUMP_OFFSET_LEN);

        if (switchOp == JSOP_TABLESWITCH) {
            /* Fill in switch bounds, which we know fit in 16-bit offsets. */
            SET_JUMP_OFFSET(pc, low);
            pc += JUMP_OFFSET_LEN;
            SET_JUMP_OFFSET(pc, high);
            pc += JUMP_OFFSET_LEN;

            /*
             * Use malloc to avoid arena bloat for programs with many switches.
             * We free table if non-null at label out, so all control flow must
             * exit this function through goto out or goto bad.
             */
            if (tableLength != 0) {
                tableSize = (size_t)tableLength * sizeof *table;
                table = (ParseNode **) cx->malloc_(tableSize);
                if (!table)
                    return JS_FALSE;
                memset(table, 0, tableSize);
                for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
                    if (pn3->isKind(PNK_DEFAULT))
                        continue;
                    i = pn3->pn_pval->toInt32();
                    i -= low;
                    JS_ASSERT((uint32)i < tableLength);
                    table[i] = pn3;
                }
            }
        } else {
            JS_ASSERT(switchOp == JSOP_LOOKUPSWITCH);

            /* Fill in the number of cases. */
            SET_INDEX(pc, caseCount);
            pc += INDEX_LEN;
        }

        /*
         * After this point, all control flow involving JSOP_TABLESWITCH
         * must set ok and goto out to exit this function.  To keep things
         * simple, all switchOp cases exit that way.
         */
        MUST_FLOW_THROUGH("out");
        if (bce->spanDeps) {
            /*
             * We have already generated at least one big jump so we must
             * explicitly add span dependencies for the switch jumps. When
             * called below, SetJumpOffset can only do it when patching the
             * first big jump or when bce->spanDeps is null.
             */
            if (!AddSwitchSpanDeps(cx, bce, bce->code(top)))
                goto bad;
        }

        if (constPropagated) {
            /*
             * Skip switchOp, as we are not setting jump offsets in the two
             * for loops below.  We'll restore bce->next() from savepc after,
             * unless there was an error.
             */
            savepc = bce->next();
            bce->current->next = pc + 1;
            if (switchOp == JSOP_TABLESWITCH) {
                for (i = 0; i < (jsint)tableLength; i++) {
                    pn3 = table[i];
                    if (pn3 &&
                        (pn4 = pn3->pn_left) != NULL &&
                        pn4->isKind(PNK_NAME))
                    {
                        /* Note a propagated constant with the const's name. */
                        JS_ASSERT(!pn4->maybeExpr());
                        jsatomid index;
                        if (!bce->makeAtomIndex(pn4->pn_atom, &index))
                            goto bad;
                        bce->current->next = pc;
                        if (NewSrcNote2(cx, bce, SRC_LABEL, ptrdiff_t(index)) < 0)
                            goto bad;
                    }
                    pc += JUMP_OFFSET_LEN;
                }
            } else {
                for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
                    pn4 = pn3->pn_left;
                    if (pn4 && pn4->isKind(PNK_NAME)) {
                        /* Note a propagated constant with the const's name. */
                        JS_ASSERT(!pn4->maybeExpr());
                        jsatomid index;
                        if (!bce->makeAtomIndex(pn4->pn_atom, &index))
                            goto bad;
                        bce->current->next = pc;
                        if (NewSrcNote2(cx, bce, SRC_LABEL, ptrdiff_t(index)) < 0)
                            goto bad;
                    }
                    pc += INDEX_LEN + JUMP_OFFSET_LEN;
                }
            }
            bce->current->next = savepc;
        }
    }

    /* Emit code for each case's statements, copying pn_offset up to pn3. */
    for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
        if (switchOp == JSOP_CONDSWITCH && !pn3->isKind(PNK_DEFAULT))
            CHECK_AND_SET_JUMP_OFFSET_AT_CUSTOM(cx, bce, pn3->pn_offset, goto bad);
        pn4 = pn3->pn_right;
        ok = EmitTree(cx, bce, pn4);
        if (!ok)
            goto out;
        pn3->pn_offset = pn4->pn_offset;
        if (pn3->isKind(PNK_DEFAULT))
            off = pn3->pn_offset - top;
    }

    if (!hasDefault) {
        /* If no default case, offset for default is to end of switch. */
        off = bce->offset() - top;
    }

    /* We better have set "off" by now. */
    JS_ASSERT(off != -1);

    /* Set the default offset (to end of switch if no default). */
    if (switchOp == JSOP_CONDSWITCH) {
        pc = NULL;
        JS_ASSERT(defaultOffset != -1);
        ok = SetJumpOffset(cx, bce, bce->code(defaultOffset), off - (defaultOffset - top));
        if (!ok)
            goto out;
    } else {
        pc = bce->code(top);
        ok = SetJumpOffset(cx, bce, pc, off);
        if (!ok)
            goto out;
        pc += JUMP_OFFSET_LEN;
    }

    /* Set the SRC_SWITCH note's offset operand to tell end of switch. */
    off = bce->offset() - top;
    ok = SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 0, off);
    if (!ok)
        goto out;

    if (switchOp == JSOP_TABLESWITCH) {
        /* Skip over the already-initialized switch bounds. */
        pc += 2 * JUMP_OFFSET_LEN;

        /* Fill in the jump table, if there is one. */
        for (i = 0; i < (jsint)tableLength; i++) {
            pn3 = table[i];
            off = pn3 ? pn3->pn_offset - top : 0;
            ok = SetJumpOffset(cx, bce, pc, off);
            if (!ok)
                goto out;
            pc += JUMP_OFFSET_LEN;
        }
    } else if (switchOp == JSOP_LOOKUPSWITCH) {
        /* Skip over the already-initialized number of cases. */
        pc += INDEX_LEN;

        for (pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
            if (pn3->isKind(PNK_DEFAULT))
                continue;
            if (!bce->constList.append(*pn3->pn_pval))
                goto bad;
            SET_INDEX(pc, bce->constList.length() - 1);
            pc += INDEX_LEN;

            off = pn3->pn_offset - top;
            ok = SetJumpOffset(cx, bce, pc, off);
            if (!ok)
                goto out;
            pc += JUMP_OFFSET_LEN;
        }
    }

out:
    if (table)
        cx->free_(table);
    if (ok) {
        ok = PopStatementBCE(cx, bce);

#if JS_HAS_BLOCK_SCOPE
        if (ok && pn->pn_right->isKind(PNK_LEXICALSCOPE))
            ok = EmitLeaveBlock(cx, bce, JSOP_LEAVEBLOCK, box);
#endif
    }
    return ok;

bad:
    ok = JS_FALSE;
    goto out;
}

JSBool
frontend::EmitFunctionScript(JSContext *cx, BytecodeEmitter *bce, ParseNode *body)
{
    /*
     * The decompiler has assumptions about what may occur immediately after
     * script->main (e.g., in the case of destructuring params). Thus, put the
     * following ops into the range [script->code, script->main). Note:
     * execution starts from script->code, so this has no semantic effect.
     */

    if (bce->flags & TCF_FUN_IS_GENERATOR) {
        /* JSOP_GENERATOR must be the first instruction. */
        bce->switchToProlog();
        JS_ASSERT(bce->next() == bce->base());
        if (Emit1(cx, bce, JSOP_GENERATOR) < 0)
            return false;
        bce->switchToMain();
    }

    /*
     * Strict mode functions' arguments objects copy initial parameter values.
     * We create arguments objects lazily -- but that doesn't work for strict
     * mode functions where a parameter might be modified and arguments might
     * be accessed. For such functions we synthesize an access to arguments to
     * initialize it with the original parameter values.
     */
    if (bce->needsEagerArguments()) {
        bce->switchToProlog();
        if (Emit1(cx, bce, JSOP_ARGUMENTS) < 0 || Emit1(cx, bce, JSOP_POP) < 0)
            return false;
        bce->switchToMain();
    }

    if (bce->flags & TCF_FUN_UNBRAND_THIS) {
        bce->switchToProlog();
        if (Emit1(cx, bce, JSOP_UNBRANDTHIS) < 0)
            return false;
        bce->switchToMain();
    }

    return EmitTree(cx, bce, body) &&
           Emit1(cx, bce, JSOP_STOP) >= 0 &&
           JSScript::NewScriptFromEmitter(cx, bce);
}

static bool
MaybeEmitVarDecl(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp, ParseNode *pn,
                 jsatomid *result)
{
    jsatomid atomIndex;

    if (!pn->pn_cookie.isFree()) {
        atomIndex = pn->pn_cookie.slot();
    } else {
        if (!bce->makeAtomIndex(pn->pn_atom, &atomIndex))
            return false;
    }

    if (JOF_OPTYPE(pn->getOp()) == JOF_ATOM &&
        (!bce->inFunction() || (bce->flags & TCF_FUN_HEAVYWEIGHT)) &&
        !(pn->pn_dflags & PND_GVAR))
    {
        bce->switchToProlog();
        if (!UpdateLineNumberNotes(cx, bce, pn->pn_pos.begin.lineno))
            return false;
        EMIT_INDEX_OP(prologOp, atomIndex);
        bce->switchToMain();
    }

    if (bce->inFunction() &&
        JOF_OPTYPE(pn->getOp()) == JOF_LOCAL &&
        pn->pn_cookie.slot() < bce->bindings.countVars() &&
        bce->shouldNoteClosedName(pn))
    {
        if (!bce->closedVars.append(pn->pn_cookie.slot()))
            return false;
    }

    if (result)
        *result = atomIndex;
    return true;
}

#if JS_HAS_DESTRUCTURING

typedef JSBool
(*DestructuringDeclEmitter)(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp, ParseNode *pn);

static JSBool
EmitDestructuringDecl(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp, ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_NAME));
    if (!BindNameToSlot(cx, bce, pn))
        return JS_FALSE;

    JS_ASSERT(!pn->isOp(JSOP_ARGUMENTS) && !pn->isOp(JSOP_CALLEE));
    return MaybeEmitVarDecl(cx, bce, prologOp, pn, NULL);
}

static JSBool
EmitDestructuringDecls(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp, ParseNode *pn)
{
    ParseNode *pn2, *pn3;
    DestructuringDeclEmitter emitter;

    if (pn->isKind(PNK_RB)) {
        for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            if (pn2->isKind(PNK_COMMA))
                continue;
            emitter = (pn2->isKind(PNK_NAME))
                      ? EmitDestructuringDecl
                      : EmitDestructuringDecls;
            if (!emitter(cx, bce, prologOp, pn2))
                return JS_FALSE;
        }
    } else {
        JS_ASSERT(pn->isKind(PNK_RC));
        for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            pn3 = pn2->pn_right;
            emitter = pn3->isKind(PNK_NAME) ? EmitDestructuringDecl : EmitDestructuringDecls;
            if (!emitter(cx, bce, prologOp, pn3))
                return JS_FALSE;
        }
    }
    return JS_TRUE;
}

static JSBool
EmitDestructuringOpsHelper(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn);

static JSBool
EmitDestructuringLHS(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    /*
     * Now emit the lvalue opcode sequence.  If the lvalue is a nested
     * destructuring initialiser-form, call ourselves to handle it, then
     * pop the matched value.  Otherwise emit an lvalue bytecode sequence
     * ending with a JSOP_ENUMELEM or equivalent op.
     */
    if (pn->isKind(PNK_RB) || pn->isKind(PNK_RC)) {
        if (!EmitDestructuringOpsHelper(cx, bce, pn))
            return JS_FALSE;
        if (Emit1(cx, bce, JSOP_POP) < 0)
            return JS_FALSE;
    } else {
        if (pn->isKind(PNK_NAME)) {
            if (!BindNameToSlot(cx, bce, pn))
                return JS_FALSE;
            if (pn->isConst() && !pn->isInitialized())
                return Emit1(cx, bce, JSOP_POP) >= 0;
        }

        switch (pn->getOp()) {
          case JSOP_SETNAME:
          case JSOP_SETGNAME:
            /*
             * NB: pn is a PN_NAME node, not a PN_BINARY.  Nevertheless,
             * we want to emit JSOP_ENUMELEM, which has format JOF_ELEM.
             * So here and for JSOP_ENUMCONSTELEM, we use EmitElemOp.
             */
            if (!EmitElemOp(cx, pn, JSOP_ENUMELEM, bce))
                return JS_FALSE;
            break;

          case JSOP_SETCONST:
            if (!EmitElemOp(cx, pn, JSOP_ENUMCONSTELEM, bce))
                return JS_FALSE;
            break;

          case JSOP_SETLOCAL:
          {
            jsuint slot = pn->pn_cookie.asInteger();
            EMIT_UINT16_IMM_OP(JSOP_SETLOCALPOP, slot);
            break;
          }

          case JSOP_SETARG:
          {
            jsuint slot = pn->pn_cookie.asInteger();
            EMIT_UINT16_IMM_OP(pn->getOp(), slot);
            if (Emit1(cx, bce, JSOP_POP) < 0)
                return JS_FALSE;
            break;
          }

          default:
          {
            ptrdiff_t top;

            top = bce->offset();
            if (!EmitTree(cx, bce, pn))
                return JS_FALSE;
            if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - top) < 0)
                return JS_FALSE;
            if (!EmitElemOpBase(cx, bce, JSOP_ENUMELEM))
                return JS_FALSE;
            break;
          }

          case JSOP_ENUMELEM:
            JS_ASSERT(0);
        }
    }

    return JS_TRUE;
}

/*
 * Recursive helper for EmitDestructuringOps.
 *
 * Given a value to destructure on the stack, walk over an object or array
 * initialiser at pn, emitting bytecodes to match property values and store
 * them in the lvalues identified by the matched property names.
 */
static JSBool
EmitDestructuringOpsHelper(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    jsuint index;
    ParseNode *pn2, *pn3;
    JSBool doElemOp;

#ifdef DEBUG
    intN stackDepth = bce->stackDepth;
    JS_ASSERT(stackDepth != 0);
    JS_ASSERT(pn->isArity(PN_LIST));
    JS_ASSERT(pn->isKind(PNK_RB) || pn->isKind(PNK_RC));
#endif

    if (pn->pn_count == 0) {
        /* Emit a DUP;POP sequence for the decompiler. */
        return Emit1(cx, bce, JSOP_DUP) >= 0 &&
               Emit1(cx, bce, JSOP_POP) >= 0;
    }

    index = 0;
    for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
        /*
         * Duplicate the value being destructured to use as a reference base.
         * If dup is not the first one, annotate it for the decompiler.
         */
        if (pn2 != pn->pn_head && NewSrcNote(cx, bce, SRC_CONTINUE) < 0)
            return JS_FALSE;
        if (Emit1(cx, bce, JSOP_DUP) < 0)
            return JS_FALSE;

        /*
         * Now push the property name currently being matched, which is either
         * the array initialiser's current index, or the current property name
         * "label" on the left of a colon in the object initialiser.  Set pn3
         * to the lvalue node, which is in the value-initializing position.
         */
        doElemOp = JS_TRUE;
        if (pn->isKind(PNK_RB)) {
            if (!EmitNumberOp(cx, index, bce))
                return JS_FALSE;
            pn3 = pn2;
        } else {
            JS_ASSERT(pn->isKind(PNK_RC));
            JS_ASSERT(pn2->isKind(PNK_COLON));
            pn3 = pn2->pn_left;
            if (pn3->isKind(PNK_NUMBER)) {
                /*
                 * If we are emitting an object destructuring initialiser,
                 * annotate the index op with SRC_INITPROP so we know we are
                 * not decompiling an array initialiser.
                 */
                if (NewSrcNote(cx, bce, SRC_INITPROP) < 0)
                    return JS_FALSE;
                if (!EmitNumberOp(cx, pn3->pn_dval, bce))
                    return JS_FALSE;
            } else {
                JS_ASSERT(pn3->isKind(PNK_STRING) || pn3->isKind(PNK_NAME));
                if (!EmitAtomOp(cx, pn3, JSOP_GETPROP, bce))
                    return JS_FALSE;
                doElemOp = JS_FALSE;
            }
            pn3 = pn2->pn_right;
        }

        if (doElemOp) {
            /*
             * Ok, get the value of the matching property name.  This leaves
             * that value on top of the value being destructured, so the stack
             * is one deeper than when we started.
             */
            if (!EmitElemOpBase(cx, bce, JSOP_GETELEM))
                return JS_FALSE;
            JS_ASSERT(bce->stackDepth == stackDepth + 1);
        }

        /* Nullary comma node makes a hole in the array destructurer. */
        if (pn3->isKind(PNK_COMMA) && pn3->isArity(PN_NULLARY)) {
            JS_ASSERT(pn->isKind(PNK_RB));
            JS_ASSERT(pn2 == pn3);
            if (Emit1(cx, bce, JSOP_POP) < 0)
                return JS_FALSE;
        } else {
            if (!EmitDestructuringLHS(cx, bce, pn3))
                return JS_FALSE;
        }

        JS_ASSERT(bce->stackDepth == stackDepth);
        ++index;
    }

    return JS_TRUE;
}

static ptrdiff_t
OpToDeclType(JSOp op)
{
    switch (op) {
      case JSOP_NOP:
        return SRC_DECL_LET;
      case JSOP_DEFCONST:
        return SRC_DECL_CONST;
      case JSOP_DEFVAR:
        return SRC_DECL_VAR;
      default:
        return SRC_DECL_NONE;
    }
}

static JSBool
EmitDestructuringOps(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp, ParseNode *pn)
{
    /*
     * If we're called from a variable declaration, help the decompiler by
     * annotating the first JSOP_DUP that EmitDestructuringOpsHelper emits.
     * If the destructuring initialiser is empty, our helper will emit a
     * JSOP_DUP followed by a JSOP_POP for the decompiler.
     */
    if (NewSrcNote2(cx, bce, SRC_DESTRUCT, OpToDeclType(prologOp)) < 0)
        return JS_FALSE;

    /*
     * Call our recursive helper to emit the destructuring assignments and
     * related stack manipulations.
     */
    return EmitDestructuringOpsHelper(cx, bce, pn);
}

static JSBool
EmitGroupAssignment(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp,
                    ParseNode *lhs, ParseNode *rhs)
{
    jsuint depth, limit, i, nslots;
    ParseNode *pn;

    depth = limit = (uintN) bce->stackDepth;
    for (pn = rhs->pn_head; pn; pn = pn->pn_next) {
        if (limit == JS_BIT(16)) {
            ReportCompileErrorNumber(cx, bce->tokenStream(), rhs, JSREPORT_ERROR,
                                     JSMSG_ARRAY_INIT_TOO_BIG);
            return JS_FALSE;
        }

        /* MaybeEmitGroupAssignment won't call us if rhs is holey. */
        JS_ASSERT(!(pn->isKind(PNK_COMMA) && pn->isArity(PN_NULLARY)));
        if (!EmitTree(cx, bce, pn))
            return JS_FALSE;
        ++limit;
    }

    if (NewSrcNote2(cx, bce, SRC_GROUPASSIGN, OpToDeclType(prologOp)) < 0)
        return JS_FALSE;

    i = depth;
    for (pn = lhs->pn_head; pn; pn = pn->pn_next, ++i) {
        /* MaybeEmitGroupAssignment requires lhs->pn_count <= rhs->pn_count. */
        JS_ASSERT(i < limit);
        jsint slot = AdjustBlockSlot(cx, bce, i);
        if (slot < 0)
            return JS_FALSE;
        EMIT_UINT16_IMM_OP(JSOP_GETLOCAL, slot);

        if (pn->isKind(PNK_COMMA) && pn->isArity(PN_NULLARY)) {
            if (Emit1(cx, bce, JSOP_POP) < 0)
                return JS_FALSE;
        } else {
            if (!EmitDestructuringLHS(cx, bce, pn))
                return JS_FALSE;
        }
    }

    nslots = limit - depth;
    EMIT_UINT16_IMM_OP(JSOP_POPN, nslots);
    bce->stackDepth = (uintN) depth;
    return JS_TRUE;
}

/*
 * Helper called with pop out param initialized to a JSOP_POP* opcode.  If we
 * can emit a group assignment sequence, which results in 0 stack depth delta,
 * we set *pop to JSOP_NOP so callers can veto emitting pn followed by a pop.
 */
static JSBool
MaybeEmitGroupAssignment(JSContext *cx, BytecodeEmitter *bce, JSOp prologOp, ParseNode *pn,
                         JSOp *pop)
{
    JS_ASSERT(pn->isKind(PNK_ASSIGN));
    JS_ASSERT(pn->isOp(JSOP_NOP));
    JS_ASSERT(*pop == JSOP_POP || *pop == JSOP_POPV);

    ParseNode *lhs = pn->pn_left;
    ParseNode *rhs = pn->pn_right;
    if (lhs->isKind(PNK_RB) && rhs->isKind(PNK_RB) &&
        !(rhs->pn_xflags & PNX_HOLEY) &&
        lhs->pn_count <= rhs->pn_count) {
        if (!EmitGroupAssignment(cx, bce, prologOp, lhs, rhs))
            return JS_FALSE;
        *pop = JSOP_NOP;
    }
    return JS_TRUE;
}

#endif /* JS_HAS_DESTRUCTURING */

static JSBool
EmitVariables(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, JSBool inLetHead,
              ptrdiff_t *headNoteIndex)
{
    bool forInVar, first;
    ptrdiff_t off, noteIndex, tmp;
    ParseNode *pn2, *pn3, *next;
    JSOp op;
    jsatomid atomIndex;
    uintN oldflags;

    /* Default in case of JS_HAS_BLOCK_SCOPE early return, below. */
    *headNoteIndex = -1;

    /*
     * Let blocks and expressions have a parenthesized head in which the new
     * scope is not yet open. Initializer evaluation uses the parent node's
     * lexical scope. If popScope is true below, then we hide the top lexical
     * block from any calls to BindNameToSlot hiding in pn2->pn_expr so that
     * it won't find any names in the new let block.
     *
     * The same goes for let declarations in the head of any kind of for loop.
     * Unlike a let declaration 'let x = i' within a block, where x is hoisted
     * to the start of the block, a 'for (let x = i...) ...' loop evaluates i
     * in the containing scope, and puts x in the loop body's scope.
     */
    DebugOnly<bool> let = (pn->isOp(JSOP_NOP));
    forInVar = (pn->pn_xflags & PNX_FORINVAR) != 0;

    off = noteIndex = -1;
    for (pn2 = pn->pn_head; ; pn2 = next) {
        first = pn2 == pn->pn_head;
        next = pn2->pn_next;

        if (!pn2->isKind(PNK_NAME)) {
#if JS_HAS_DESTRUCTURING
            if (pn2->isKind(PNK_RB) || pn2->isKind(PNK_RC)) {
                /*
                 * Emit variable binding ops, but not destructuring ops.  The
                 * parser (see Parser::variables) has ensured that our caller
                 * will be the PNK_FOR/PNK_FORIN case in EmitTree, and that
                 * case will emit the destructuring code only after emitting an
                 * enumerating opcode and a branch that tests whether the
                 * enumeration ended.
                 */
                JS_ASSERT(forInVar);
                JS_ASSERT(pn->pn_count == 1);
                if (!EmitDestructuringDecls(cx, bce, pn->getOp(), pn2))
                    return JS_FALSE;
                break;
            }
#endif

            /*
             * A destructuring initialiser assignment preceded by var will
             * never occur to the left of 'in' in a for-in loop.  As with 'for
             * (var x = i in o)...', this will cause the entire 'var [a, b] =
             * i' to be hoisted out of the loop.
             */
            JS_ASSERT(pn2->isKind(PNK_ASSIGN));
            JS_ASSERT(pn2->isOp(JSOP_NOP));
            JS_ASSERT(!forInVar);

            /*
             * To allow the front end to rewrite var f = x; as f = x; when a
             * function f(){} precedes the var, detect simple name assignment
             * here and initialize the name.
             */
#if !JS_HAS_DESTRUCTURING
            JS_ASSERT(pn2->pn_left->isKind(PNK_NAME));
#else
            if (pn2->pn_left->isKind(PNK_NAME))
#endif
            {
                pn3 = pn2->pn_right;
                pn2 = pn2->pn_left;
                goto do_name;
            }

#if JS_HAS_DESTRUCTURING
            if (pn->pn_count == 1) {
                /*
                 * If this is the only destructuring assignment in the list,
                 * try to optimize to a group assignment.  If we're in a let
                 * head, pass JSOP_POP rather than the pseudo-prolog JSOP_NOP
                 * in pn->pn_op, to suppress a second (and misplaced) 'let'.
                 */
                JS_ASSERT(noteIndex < 0 && !pn2->pn_next);
                op = JSOP_POP;
                if (!MaybeEmitGroupAssignment(cx, bce,
                                              inLetHead ? JSOP_POP : pn->getOp(),
                                              pn2, &op)) {
                    return JS_FALSE;
                }
                if (op == JSOP_NOP) {
                    pn->pn_xflags = (pn->pn_xflags & ~PNX_POPVAR) | PNX_GROUPINIT;
                    break;
                }
            }

            pn3 = pn2->pn_left;
            if (!EmitDestructuringDecls(cx, bce, pn->getOp(), pn3))
                return JS_FALSE;

            if (!EmitTree(cx, bce, pn2->pn_right))
                return JS_FALSE;

            /*
             * Veto pn->pn_op if inLetHead to avoid emitting a SRC_DESTRUCT
             * that's redundant with respect to the SRC_DECL/SRC_DECL_LET that
             * we will emit at the bottom of this function.
             */
            if (!EmitDestructuringOps(cx, bce,
                                      inLetHead ? JSOP_POP : pn->getOp(),
                                      pn3)) {
                return JS_FALSE;
            }
            goto emit_note_pop;
#endif
        }

        /*
         * Load initializer early to share code above that jumps to do_name.
         * NB: if this var redeclares an existing binding, then pn2 is linked
         * on its definition's use-chain and pn_expr has been overlayed with
         * pn_lexdef.
         */
        pn3 = pn2->maybeExpr();

     do_name:
        if (!BindNameToSlot(cx, bce, pn2))
            return JS_FALSE;

        op = pn2->getOp();
        if (op == JSOP_ARGUMENTS) {
            /* JSOP_ARGUMENTS => no initializer */
            JS_ASSERT(!pn3 && !let);
            pn3 = NULL;
#ifdef __GNUC__
            atomIndex = 0;            /* quell GCC overwarning */
#endif
        } else {
            JS_ASSERT(op != JSOP_CALLEE);
            JS_ASSERT(!pn2->pn_cookie.isFree() || !let);
            if (!MaybeEmitVarDecl(cx, bce, pn->getOp(), pn2, &atomIndex))
                return JS_FALSE;

            if (pn3) {
                JS_ASSERT(!forInVar);
                if (op == JSOP_SETNAME) {
                    JS_ASSERT(!let);
                    EMIT_INDEX_OP(JSOP_BINDNAME, atomIndex);
                } else if (op == JSOP_SETGNAME) {
                    JS_ASSERT(!let);
                    EMIT_INDEX_OP(JSOP_BINDGNAME, atomIndex);
                }
                if (pn->isOp(JSOP_DEFCONST) &&
                    !DefineCompileTimeConstant(cx, bce, pn2->pn_atom, pn3))
                {
                    return JS_FALSE;
                }

                oldflags = bce->flags;
                bce->flags &= ~TCF_IN_FOR_INIT;
                if (!EmitTree(cx, bce, pn3))
                    return JS_FALSE;
                bce->flags |= oldflags & TCF_IN_FOR_INIT;
            }
        }

        /*
         * The parser rewrites 'for (var x = i in o)' to hoist 'var x = i' --
         * likewise 'for (let x = i in o)' becomes 'i; for (let x in o)' using
         * a PNK_SEQ node to make the two statements appear as one. Therefore
         * if this declaration is part of a for-in loop head, we do not need to
         * emit op or any source note. Our caller, the PNK_FOR/PNK_IN case in
         * EmitTree, will annotate appropriately.
         */
        JS_ASSERT_IF(pn2->isDefn(), pn3 == pn2->pn_expr);
        if (forInVar) {
            JS_ASSERT(pn->pn_count == 1);
            JS_ASSERT(!pn3);
            break;
        }

        if (first &&
            !inLetHead &&
            NewSrcNote2(cx, bce, SRC_DECL,
                        (pn->isOp(JSOP_DEFCONST))
                        ? SRC_DECL_CONST
                        : (pn->isOp(JSOP_DEFVAR))
                        ? SRC_DECL_VAR
                        : SRC_DECL_LET) < 0)
        {
            return JS_FALSE;
        }
        if (op == JSOP_ARGUMENTS) {
            if (Emit1(cx, bce, op) < 0)
                return JS_FALSE;
        } else if (!pn2->pn_cookie.isFree()) {
            EMIT_UINT16_IMM_OP(op, atomIndex);
        } else {
            EMIT_INDEX_OP(op, atomIndex);
        }

#if JS_HAS_DESTRUCTURING
    emit_note_pop:
#endif
        tmp = bce->offset();
        if (noteIndex >= 0) {
            if (!SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 0, tmp-off))
                return JS_FALSE;
        }
        if (!next)
            break;
        off = tmp;
        noteIndex = NewSrcNote2(cx, bce, SRC_PCDELTA, 0);
        if (noteIndex < 0 || Emit1(cx, bce, JSOP_POP) < 0)
            return JS_FALSE;
    }

    /* If this is a let head, emit and return a srcnote on the pop. */
    if (inLetHead) {
        *headNoteIndex = NewSrcNote(cx, bce, SRC_DECL);
        if (*headNoteIndex < 0)
            return JS_FALSE;
        if (!(pn->pn_xflags & PNX_POPVAR))
            return Emit1(cx, bce, JSOP_NOP) >= 0;
    }

    return !(pn->pn_xflags & PNX_POPVAR) || Emit1(cx, bce, JSOP_POP) >= 0;
}

static bool
EmitAssignment(JSContext *cx, BytecodeEmitter *bce, ParseNode *lhs, JSOp op, ParseNode *rhs)
{
    ptrdiff_t top = bce->offset();

    /*
     * Check left operand type and generate specialized code for it.
     * Specialize to avoid ECMA "reference type" values on the operand
     * stack, which impose pervasive runtime "GetValue" costs.
     */
    jsatomid atomIndex = (jsatomid) -1;              /* quell GCC overwarning */
    jsbytecode offset = 1;

    switch (lhs->getKind()) {
      case PNK_NAME:
        if (!BindNameToSlot(cx, bce, lhs))
            return false;
        if (!lhs->pn_cookie.isFree()) {
            atomIndex = lhs->pn_cookie.asInteger();
        } else {
            if (!bce->makeAtomIndex(lhs->pn_atom, &atomIndex))
                return false;
            if (!lhs->isConst()) {
                JSOp op = lhs->isOp(JSOP_SETGNAME) ? JSOP_BINDGNAME : JSOP_BINDNAME;
                EMIT_INDEX_OP(op, atomIndex);
                offset++;
            }
        }
        break;
      case PNK_DOT:
        if (!EmitTree(cx, bce, lhs->expr()))
            return false;
        offset++;
        if (!bce->makeAtomIndex(lhs->pn_atom, &atomIndex))
            return false;
        break;
      case PNK_LB:
        JS_ASSERT(lhs->isArity(PN_BINARY));
        if (!EmitTree(cx, bce, lhs->pn_left))
            return false;
        if (!EmitTree(cx, bce, lhs->pn_right))
            return false;
        offset += 2;
        break;
#if JS_HAS_DESTRUCTURING
      case PNK_RB:
      case PNK_RC:
        break;
#endif
      case PNK_LP:
        if (!EmitTree(cx, bce, lhs))
            return false;
        offset++;
        break;
#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        JS_ASSERT(!bce->inStrictMode());
        JS_ASSERT(lhs->isOp(JSOP_SETXMLNAME));
        if (!EmitTree(cx, bce, lhs->pn_kid))
            return false;
        if (Emit1(cx, bce, JSOP_BINDXMLNAME) < 0)
            return false;
        offset++;
        break;
#endif
      default:
        JS_ASSERT(0);
    }

    if (op != JSOP_NOP) {
        JS_ASSERT(rhs);
        switch (lhs->getKind()) {
          case PNK_NAME:
            if (lhs->isConst()) {
                if (lhs->isOp(JSOP_CALLEE)) {
                    if (Emit1(cx, bce, JSOP_CALLEE) < 0)
                        return false;
                } else {
                    EMIT_INDEX_OP(lhs->getOp(), atomIndex);
                }
            } else if (lhs->isOp(JSOP_SETNAME)) {
                if (Emit1(cx, bce, JSOP_DUP) < 0)
                    return false;
                EMIT_INDEX_OP(JSOP_GETXPROP, atomIndex);
            } else if (lhs->isOp(JSOP_SETGNAME)) {
                if (!BindGlobal(cx, bce, lhs, lhs->pn_atom))
                    return false;
                EmitAtomOp(cx, lhs, JSOP_GETGNAME, bce);
            } else {
                EMIT_UINT16_IMM_OP(lhs->isOp(JSOP_SETARG) ? JSOP_GETARG : JSOP_GETLOCAL, atomIndex);
            }
            break;
          case PNK_DOT:
            if (Emit1(cx, bce, JSOP_DUP) < 0)
                return false;
            if (lhs->pn_atom == cx->runtime->atomState.protoAtom) {
                if (!EmitIndexOp(cx, JSOP_QNAMEPART, atomIndex, bce))
                    return false;
                if (!EmitElemOpBase(cx, bce, JSOP_GETELEM))
                    return false;
            } else {
                bool isLength = (lhs->pn_atom == cx->runtime->atomState.lengthAtom);
                EMIT_INDEX_OP(isLength ? JSOP_LENGTH : JSOP_GETPROP, atomIndex);
            }
            break;
          case PNK_LB:
          case PNK_LP:
#if JS_HAS_XML_SUPPORT
          case PNK_XMLUNARY:
#endif
            if (Emit1(cx, bce, JSOP_DUP2) < 0)
                return false;
            if (!EmitElemOpBase(cx, bce, JSOP_GETELEM))
                return false;
            break;
          default:;
        }
    }

    /* Now emit the right operand (it may affect the namespace). */
    if (rhs) {
        if (!EmitTree(cx, bce, rhs))
            return false;
    } else {
        /* The value to assign is the next enumeration value in a for-in loop. */
        if (Emit2(cx, bce, JSOP_ITERNEXT, offset) < 0)
            return false;
    }

    /* If += etc., emit the binary operator with a decompiler note. */
    if (op != JSOP_NOP) {
        /*
         * Take care to avoid SRC_ASSIGNOP if the left-hand side is a const
         * declared in the current compilation unit, as in this case (just
         * a bit further below) we will avoid emitting the assignment op.
         */
        if (!lhs->isKind(PNK_NAME) || !lhs->isConst()) {
            if (NewSrcNote(cx, bce, SRC_ASSIGNOP) < 0)
                return false;
        }
        if (Emit1(cx, bce, op) < 0)
            return false;
    }

    /* Left parts such as a.b.c and a[b].c need a decompiler note. */
    if (!lhs->isKind(PNK_NAME) &&
#if JS_HAS_DESTRUCTURING
        !lhs->isKind(PNK_RB) &&
        !lhs->isKind(PNK_RC) &&
#endif
        NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - top) < 0)
    {
        return false;
    }

    /* Finally, emit the specialized assignment bytecode. */
    switch (lhs->getKind()) {
      case PNK_NAME:
        if (lhs->isConst()) {
            if (!rhs) {
                ReportCompileErrorNumber(cx, bce->tokenStream(), lhs, JSREPORT_ERROR,
                                         JSMSG_BAD_FOR_LEFTSIDE);
                return false;
            }
            break;
        }
        /* FALL THROUGH */
      case PNK_DOT:
        EMIT_INDEX_OP(lhs->getOp(), atomIndex);
        break;
      case PNK_LB:
      case PNK_LP:
        if (Emit1(cx, bce, JSOP_SETELEM) < 0)
            return false;
        break;
#if JS_HAS_DESTRUCTURING
      case PNK_RB:
      case PNK_RC:
        if (!EmitDestructuringOps(cx, bce, JSOP_SETNAME, lhs))
            return false;
        break;
#endif
#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        JS_ASSERT(!bce->inStrictMode());
        if (Emit1(cx, bce, JSOP_SETXMLNAME) < 0)
            return false;
        break;
#endif
      default:
        JS_ASSERT(0);
    }
    return true;
}

#if defined DEBUG_brendan || defined DEBUG_mrbkap
static JSBool
GettableNoteForNextOp(BytecodeEmitter *bce)
{
    ptrdiff_t offset, target;
    jssrcnote *sn, *end;

    offset = 0;
    target = bce->offset();
    for (sn = bce->notes(), end = sn + bce->noteCount(); sn < end;
         sn = SN_NEXT(sn)) {
        if (offset == target && SN_IS_GETTABLE(sn))
            return JS_TRUE;
        offset += SN_DELTA(sn);
    }
    return JS_FALSE;
}
#endif

/* Top-level named functions need a nop for decompilation. */
static JSBool
EmitFunctionDefNop(JSContext *cx, BytecodeEmitter *bce, uintN index)
{
    return NewSrcNote2(cx, bce, SRC_FUNCDEF, (ptrdiff_t)index) >= 0 &&
           Emit1(cx, bce, JSOP_NOP) >= 0;
}

static bool
EmitNewInit(JSContext *cx, BytecodeEmitter *bce, JSProtoKey key, ParseNode *pn, int sharpnum)
{
    if (Emit3(cx, bce, JSOP_NEWINIT, (jsbytecode) key, 0) < 0)
        return false;
#if JS_HAS_SHARP_VARS
    if (bce->hasSharps()) {
        if (pn->pn_count != 0)
            EMIT_UINT16_IMM_OP(JSOP_SHARPINIT, bce->sharpSlotBase);
        if (sharpnum >= 0)
            EMIT_UINT16PAIR_IMM_OP(JSOP_DEFSHARP, bce->sharpSlotBase, sharpnum);
    } else {
        JS_ASSERT(sharpnum < 0);
    }
#endif
    return true;
}

static bool
EmitEndInit(JSContext *cx, BytecodeEmitter *bce, uint32 count)
{
#if JS_HAS_SHARP_VARS
    /* Emit an op for sharp array cleanup and decompilation. */
    if (bce->hasSharps() && count != 0)
        EMIT_UINT16_IMM_OP(JSOP_SHARPINIT, bce->sharpSlotBase);
#endif
    return Emit1(cx, bce, JSOP_ENDINIT) >= 0;
}

bool
ParseNode::getConstantValue(JSContext *cx, bool strictChecks, Value *vp)
{
    switch (getKind()) {
      case PNK_NUMBER:
        vp->setNumber(pn_dval);
        return true;
      case PNK_STRING:
        vp->setString(pn_atom);
        return true;
      case PNK_TRUE:
        vp->setBoolean(true);
        return true;
      case PNK_FALSE:
        vp->setBoolean(false);
        return true;
      case PNK_NULL:
        vp->setNull();
        return true;
      case PNK_RB: {
        JS_ASSERT(isOp(JSOP_NEWINIT) && !(pn_xflags & PNX_NONCONST));

        JSObject *obj = NewDenseAllocatedArray(cx, pn_count);
        if (!obj)
            return false;

        unsigned idx = 0;
        for (ParseNode *pn = pn_head; pn; idx++, pn = pn->pn_next) {
            Value value;
            if (!pn->getConstantValue(cx, strictChecks, &value))
                return false;
            if (!obj->defineGeneric(cx, INT_TO_JSID(idx), value, NULL, NULL, JSPROP_ENUMERATE))
                return false;
        }
        JS_ASSERT(idx == pn_count);

        types::FixArrayType(cx, obj);
        vp->setObject(*obj);
        return true;
      }
      case PNK_RC: {
        JS_ASSERT(isOp(JSOP_NEWINIT) && !(pn_xflags & PNX_NONCONST));

        gc::AllocKind kind = GuessObjectGCKind(pn_count, false);
        JSObject *obj = NewBuiltinClassInstance(cx, &ObjectClass, kind);
        if (!obj)
            return false;

        for (ParseNode *pn = pn_head; pn; pn = pn->pn_next) {
            Value value;
            if (!pn->pn_right->getConstantValue(cx, strictChecks, &value))
                return false;

            ParseNode *pnid = pn->pn_left;
            if (pnid->isKind(PNK_NUMBER)) {
                Value idvalue = NumberValue(pnid->pn_dval);
                jsid id;
                if (idvalue.isInt32() && INT_FITS_IN_JSID(idvalue.toInt32()))
                    id = INT_TO_JSID(idvalue.toInt32());
                else if (!js_InternNonIntElementId(cx, obj, idvalue, &id))
                    return false;
                if (!obj->defineGeneric(cx, id, value, NULL, NULL, JSPROP_ENUMERATE))
                    return false;
            } else {
                JS_ASSERT(pnid->isKind(PNK_NAME) || pnid->isKind(PNK_STRING));
                JS_ASSERT(pnid->pn_atom != cx->runtime->atomState.protoAtom);
                jsid id = ATOM_TO_JSID(pnid->pn_atom);
                if (!DefineNativeProperty(cx, obj, id, value, NULL, NULL,
                                          JSPROP_ENUMERATE, 0, 0)) {
                    return false;
                }
            }
        }

        types::FixObjectType(cx, obj);
        vp->setObject(*obj);
        return true;
      }
      default:
        JS_NOT_REACHED("Unexpected node");
    }
    return false;
}

static bool
EmitSingletonInitialiser(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    Value value;
    if (!pn->getConstantValue(cx, bce->needStrictChecks(), &value))
        return false;

    JS_ASSERT(value.isObject());
    ObjectBox *objbox = bce->parser->newObjectBox(&value.toObject());
    if (!objbox)
        return false;

    return EmitObjectOp(cx, objbox, JSOP_OBJECT, bce);
}

/* See the SRC_FOR source note offsetBias comments later in this file. */
JS_STATIC_ASSERT(JSOP_NOP_LENGTH == 1);
JS_STATIC_ASSERT(JSOP_POP_LENGTH == 1);

class EmitLevelManager
{
    BytecodeEmitter *bce;
  public:
    EmitLevelManager(BytecodeEmitter *bce) : bce(bce) { bce->emitLevel++; }
    ~EmitLevelManager() { bce->emitLevel--; }
};

static bool
EmitCatch(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    ptrdiff_t catchStart, guardJump;

    /*
     * Morph STMT_BLOCK to STMT_CATCH, note the block entry code offset,
     * and save the block object atom.
     */
    StmtInfo *stmt = bce->topStmt;
    JS_ASSERT(stmt->type == STMT_BLOCK && (stmt->flags & SIF_SCOPE));
    stmt->type = STMT_CATCH;
    catchStart = stmt->update;

    /* Go up one statement info record to the TRY or FINALLY record. */
    stmt = stmt->down;
    JS_ASSERT(stmt->type == STMT_TRY || stmt->type == STMT_FINALLY);

    /* Pick up the pending exception and bind it to the catch variable. */
    if (Emit1(cx, bce, JSOP_EXCEPTION) < 0)
        return false;

    /*
     * Dup the exception object if there is a guard for rethrowing to use
     * it later when rethrowing or in other catches.
     */
    if (pn->pn_kid2 && Emit1(cx, bce, JSOP_DUP) < 0)
        return false;

    ParseNode *pn2 = pn->pn_kid1;
    switch (pn2->getKind()) {
#if JS_HAS_DESTRUCTURING
      case PNK_RB:
      case PNK_RC:
        if (!EmitDestructuringOps(cx, bce, JSOP_NOP, pn2))
            return false;
        if (Emit1(cx, bce, JSOP_POP) < 0)
            return false;
        break;
#endif

      case PNK_NAME:
        /* Inline and specialize BindNameToSlot for pn2. */
        JS_ASSERT(!pn2->pn_cookie.isFree());
        EMIT_UINT16_IMM_OP(JSOP_SETLOCALPOP, pn2->pn_cookie.asInteger());
        break;

      default:
        JS_ASSERT(0);
    }

    /* Emit the guard expression, if there is one. */
    if (pn->pn_kid2) {
        if (!EmitTree(cx, bce, pn->pn_kid2))
            return false;
        if (!SetSrcNoteOffset(cx, bce, CATCHNOTE(*stmt), 0, bce->offset() - catchStart))
            return false;
        /* ifeq <next block> */
        guardJump = EmitJump(cx, bce, JSOP_IFEQ, 0);
        if (guardJump < 0)
            return false;
        GUARDJUMP(*stmt) = guardJump;

        /* Pop duplicated exception object as we no longer need it. */
        if (Emit1(cx, bce, JSOP_POP) < 0)
            return false;
    }

    /* Emit the catch body. */
    if (!EmitTree(cx, bce, pn->pn_kid3))
        return false;

    /*
     * Annotate the JSOP_LEAVEBLOCK that will be emitted as we unwind via
     * our PNK_LEXICALSCOPE parent, so the decompiler knows to pop.
     */
    ptrdiff_t off = bce->stackDepth;
    if (NewSrcNote2(cx, bce, SRC_CATCH, off) < 0)
        return false;
    return true;
}

static bool
EmitTry(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    StmtInfo stmtInfo;
    ptrdiff_t catchJump = -1;

    /*
     * Push stmtInfo to track jumps-over-catches and gosubs-to-finally
     * for later fixup.
     *
     * When a finally block is active (STMT_FINALLY in our tree context),
     * non-local jumps (including jumps-over-catches) result in a GOSUB
     * being written into the bytecode stream and fixed-up later (c.f.
     * EmitBackPatchOp and BackPatch).
     */
    PushStatement(bce, &stmtInfo, pn->pn_kid3 ? STMT_FINALLY : STMT_TRY, bce->offset());

    /*
     * Since an exception can be thrown at any place inside the try block,
     * we need to restore the stack and the scope chain before we transfer
     * the control to the exception handler.
     *
     * For that we store in a try note associated with the catch or
     * finally block the stack depth upon the try entry. The interpreter
     * uses this depth to properly unwind the stack and the scope chain.
     */
    intN depth = bce->stackDepth;

    /* Mark try location for decompilation, then emit try block. */
    if (Emit1(cx, bce, JSOP_TRY) < 0)
        return false;
    ptrdiff_t tryStart = bce->offset();
    if (!EmitTree(cx, bce, pn->pn_kid1))
        return false;
    JS_ASSERT(depth == bce->stackDepth);

    /* GOSUB to finally, if present. */
    if (pn->pn_kid3) {
        if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
            return false;
        if (EmitBackPatchOp(cx, bce, JSOP_BACKPATCH, &GOSUBS(stmtInfo)) < 0)
            return false;
    }

    /* Emit (hidden) jump over catch and/or finally. */
    if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
        return false;
    if (EmitBackPatchOp(cx, bce, JSOP_BACKPATCH, &catchJump) < 0)
        return false;

    ptrdiff_t tryEnd = bce->offset();

    ObjectBox *prevBox = NULL;
    /* If this try has a catch block, emit it. */
    ParseNode *lastCatch = NULL;
    if (ParseNode *pn2 = pn->pn_kid2) {
        uintN count = 0;    /* previous catch block's population */

        /*
         * The emitted code for a catch block looks like:
         *
         * blockchain
         * [throwing]                          only if 2nd+ catch block
         * [leaveblock]                        only if 2nd+ catch block
         * enterblock                          with SRC_CATCH
         * exception
         * [dup]                               only if catchguard
         * setlocalpop <slot>                  or destructuring code
         * [< catchguard code >]               if there's a catchguard
         * [ifeq <offset to next catch block>]         " "
         * [pop]                               only if catchguard
         * < catch block contents >
         * leaveblock
         * goto <end of catch blocks>          non-local; finally applies
         *
         * If there's no catch block without a catchguard, the last
         * <offset to next catch block> points to rethrow code.  This
         * code will [gosub] to the finally code if appropriate, and is
         * also used for the catch-all trynote for capturing exceptions
         * thrown from catch{} blocks.
         */
        for (ParseNode *pn3 = pn2->pn_head; pn3; pn3 = pn3->pn_next) {
            ptrdiff_t guardJump, catchNote;

            JS_ASSERT(bce->stackDepth == depth);
            guardJump = GUARDJUMP(stmtInfo);
            if (guardJump != -1) {
                if (EmitKnownBlockChain(cx, bce, prevBox) < 0)
                    return false;
            
                /* Fix up and clean up previous catch block. */
                CHECK_AND_SET_JUMP_OFFSET_AT(cx, bce, guardJump);

                /*
                 * Account for JSOP_ENTERBLOCK (whose block object count
                 * is saved below) and pushed exception object that we
                 * still have after the jumping from the previous guard.
                 */
                bce->stackDepth = depth + count + 1;

                /*
                 * Move exception back to cx->exception to prepare for
                 * the next catch. We hide [throwing] from the decompiler
                 * since it compensates for the hidden JSOP_DUP at the
                 * start of the previous guarded catch.
                 */
                if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0 ||
                    Emit1(cx, bce, JSOP_THROWING) < 0) {
                    return false;
                }
                if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
                    return false;
                if (!EmitLeaveBlock(cx, bce, JSOP_LEAVEBLOCK, prevBox))
                    return false;
                JS_ASSERT(bce->stackDepth == depth);
            }

            /*
             * Annotate the JSOP_ENTERBLOCK that's about to be generated
             * by the call to EmitTree immediately below.  Save this
             * source note's index in stmtInfo for use by the PNK_CATCH:
             * case, where the length of the catch guard is set as the
             * note's offset.
             */
            catchNote = NewSrcNote2(cx, bce, SRC_CATCH, 0);
            if (catchNote < 0)
                return false;
            CATCHNOTE(stmtInfo) = catchNote;

            /*
             * Emit the lexical scope and catch body.  Save the catch's
             * block object population via count, for use when targeting
             * guardJump at the next catch (the guard mismatch case).
             */
            JS_ASSERT(pn3->isKind(PNK_LEXICALSCOPE));
            count = OBJ_BLOCK_COUNT(cx, pn3->pn_objbox->object);
            prevBox = pn3->pn_objbox;
            if (!EmitTree(cx, bce, pn3))
                return false;

            /* gosub <finally>, if required */
            if (pn->pn_kid3) {
                if (EmitBackPatchOp(cx, bce, JSOP_BACKPATCH, &GOSUBS(stmtInfo)) < 0)
                    return false;
                JS_ASSERT(bce->stackDepth == depth);
            }

            /*
             * Jump over the remaining catch blocks.  This will get fixed
             * up to jump to after catch/finally.
             */
            if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
                return false;
            if (EmitBackPatchOp(cx, bce, JSOP_BACKPATCH, &catchJump) < 0)
                return false;

            /*
             * Save a pointer to the last catch node to handle try-finally
             * and try-catch(guard)-finally special cases.
             */
            lastCatch = pn3->expr();
        }
    }

    /*
     * Last catch guard jumps to the rethrow code sequence if none of the
     * guards match. Target guardJump at the beginning of the rethrow
     * sequence, just in case a guard expression throws and leaves the
     * stack unbalanced.
     */
    if (lastCatch && lastCatch->pn_kid2) {
        if (EmitKnownBlockChain(cx, bce, prevBox) < 0)
            return false;
        
        CHECK_AND_SET_JUMP_OFFSET_AT(cx, bce, GUARDJUMP(stmtInfo));

        /* Sync the stack to take into account pushed exception. */
        JS_ASSERT(bce->stackDepth == depth);
        bce->stackDepth = depth + 1;

        /*
         * Rethrow the exception, delegating executing of finally if any
         * to the exception handler.
         */
        if (NewSrcNote(cx, bce, SRC_HIDDEN) < 0 || Emit1(cx, bce, JSOP_THROW) < 0)
            return false;

        if (EmitBlockChain(cx, bce) < 0)
            return false;
    }

    JS_ASSERT(bce->stackDepth == depth);

    /* Emit finally handler if any. */
    ptrdiff_t finallyStart = 0;   /* to quell GCC uninitialized warnings */
    if (pn->pn_kid3) {
        /*
         * Fix up the gosubs that might have been emitted before non-local
         * jumps to the finally code.
         */
        if (!BackPatch(cx, bce, GOSUBS(stmtInfo), bce->next(), JSOP_GOSUB))
            return false;

        finallyStart = bce->offset();

        /* Indicate that we're emitting a subroutine body. */
        stmtInfo.type = STMT_SUBROUTINE;
        if (!UpdateLineNumberNotes(cx, bce, pn->pn_kid3->pn_pos.begin.lineno))
            return false;
        if (Emit1(cx, bce, JSOP_FINALLY) < 0 ||
            !EmitTree(cx, bce, pn->pn_kid3) ||
            Emit1(cx, bce, JSOP_RETSUB) < 0)
        {
            return false;
        }
        JS_ASSERT(bce->stackDepth == depth);
    }
    if (!PopStatementBCE(cx, bce))
        return false;

    if (NewSrcNote(cx, bce, SRC_ENDBRACE) < 0 || Emit1(cx, bce, JSOP_NOP) < 0)
        return false;

    /* Fix up the end-of-try/catch jumps to come here. */
    if (!BackPatch(cx, bce, catchJump, bce->next(), JSOP_GOTO))
        return false;

    /*
     * Add the try note last, to let post-order give us the right ordering
     * (first to last for a given nesting level, inner to outer by level).
     */
    if (pn->pn_kid2 && !NewTryNote(cx, bce, JSTRY_CATCH, depth, tryStart, tryEnd))
        return false;

    /*
     * If we've got a finally, mark try+catch region with additional
     * trynote to catch exceptions (re)thrown from a catch block or
     * for the try{}finally{} case.
     */
    if (pn->pn_kid3 && !NewTryNote(cx, bce, JSTRY_FINALLY, depth, tryStart, finallyStart))
        return false;

    return true;
}

static bool
EmitIf(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    StmtInfo stmtInfo;

    /* Initialize so we can detect else-if chains and avoid recursion. */
    stmtInfo.type = STMT_IF;
    ptrdiff_t beq = -1;
    ptrdiff_t jmp = -1;
    ptrdiff_t noteIndex = -1;

  if_again:
    /* Emit code for the condition before pushing stmtInfo. */
    if (!EmitTree(cx, bce, pn->pn_kid1))
        return JS_FALSE;
    ptrdiff_t top = bce->offset();
    if (stmtInfo.type == STMT_IF) {
        PushStatement(bce, &stmtInfo, STMT_IF, top);
    } else {
        /*
         * We came here from the goto further below that detects else-if
         * chains, so we must mutate stmtInfo back into a STMT_IF record.
         * Also (see below for why) we need a note offset for SRC_IF_ELSE
         * to help the decompiler.  Actually, we need two offsets, one for
         * decompiling any else clause and the second for decompiling an
         * else-if chain without bracing, overindenting, or incorrectly
         * scoping let declarations.
         */
        JS_ASSERT(stmtInfo.type == STMT_ELSE);
        stmtInfo.type = STMT_IF;
        stmtInfo.update = top;
        if (!SetSrcNoteOffset(cx, bce, noteIndex, 0, jmp - beq))
            return JS_FALSE;
        if (!SetSrcNoteOffset(cx, bce, noteIndex, 1, top - beq))
            return JS_FALSE;
    }

    /* Emit an annotated branch-if-false around the then part. */
    ParseNode *pn3 = pn->pn_kid3;
    noteIndex = NewSrcNote(cx, bce, pn3 ? SRC_IF_ELSE : SRC_IF);
    if (noteIndex < 0)
        return JS_FALSE;
    beq = EmitJump(cx, bce, JSOP_IFEQ, 0);
    if (beq < 0)
        return JS_FALSE;

    /* Emit code for the then and optional else parts. */
    if (!EmitTree(cx, bce, pn->pn_kid2))
        return JS_FALSE;
    if (pn3) {
        /* Modify stmtInfo so we know we're in the else part. */
        stmtInfo.type = STMT_ELSE;

        /*
         * Emit a JSOP_BACKPATCH op to jump from the end of our then part
         * around the else part.  The PopStatementBCE call at the bottom of
         * this function will fix up the backpatch chain linked from
         * stmtInfo.breaks.
         */
        jmp = EmitGoto(cx, bce, &stmtInfo, &stmtInfo.breaks);
        if (jmp < 0)
            return JS_FALSE;

        /* Ensure the branch-if-false comes here, then emit the else. */
        CHECK_AND_SET_JUMP_OFFSET_AT(cx, bce, beq);
        if (pn3->isKind(PNK_IF)) {
            pn = pn3;
            goto if_again;
        }

        if (!EmitTree(cx, bce, pn3))
            return JS_FALSE;

        /*
         * Annotate SRC_IF_ELSE with the offset from branch to jump, for
         * the decompiler's benefit.  We can't just "back up" from the pc
         * of the else clause, because we don't know whether an extended
         * jump was required to leap from the end of the then clause over
         * the else clause.
         */
        if (!SetSrcNoteOffset(cx, bce, noteIndex, 0, jmp - beq))
            return JS_FALSE;
    } else {
        /* No else part, fixup the branch-if-false to come here. */
        CHECK_AND_SET_JUMP_OFFSET_AT(cx, bce, beq);
    }
    return PopStatementBCE(cx, bce);
}

#if JS_HAS_BLOCK_SCOPE
static bool
EmitLet(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    /*
     * pn represents one of these syntactic constructs:
     *   let-expression:                        (let (x = y) EXPR)
     *   let-statement:                         let (x = y) { ... }
     *   let-declaration in statement context:  let x = y;
     *   let-declaration in for-loop head:      for (let ...) ...
     *
     * Let-expressions and let-statements are represented as binary nodes
     * with their variable declarations on the left and the body on the
     * right.
     */
    ParseNode *pn2;
    if (pn->isArity(PN_BINARY)) {
        pn2 = pn->pn_right;
        pn = pn->pn_left;
    } else {
        pn2 = NULL;
    }

    /*
     * Non-null pn2 means that pn is the variable list from a let head.
     *
     * Use TempPopScope to evaluate the expressions in the enclosing scope.
     * This also causes the initializing assignments to be emitted in the
     * enclosing scope, but the assignment opcodes emitted here
     * (essentially just setlocal, though destructuring assignment uses
     * other additional opcodes) do not care about the block chain.
     */
    JS_ASSERT(pn->isArity(PN_LIST));
    TempPopScope tps;
    bool popScope = pn2 || (bce->flags & TCF_IN_FOR_INIT);
    if (popScope && !tps.popBlock(cx, bce))
        return false;
    ptrdiff_t noteIndex;
    if (!EmitVariables(cx, bce, pn, pn2 != NULL, &noteIndex))
        return false;
    ptrdiff_t tmp = bce->offset();
    if (popScope && !tps.repushBlock(cx, bce))
        return false;

    /* Thus non-null pn2 is the body of the let block or expression. */
    if (pn2 && !EmitTree(cx, bce, pn2))
        return false;

    if (noteIndex >= 0 && !SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 0, bce->offset() - tmp))
        return false;

    return true;
}
#endif

#if JS_HAS_XML_SUPPORT
static bool
EmitXMLTag(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    JS_ASSERT(!bce->inStrictMode());

    if (Emit1(cx, bce, JSOP_STARTXML) < 0)
        return false;

    {
        jsatomid index;
        JSAtom *tagAtom = (pn->isKind(PNK_XMLETAGO))
                          ? cx->runtime->atomState.etagoAtom
                          : cx->runtime->atomState.stagoAtom;
        if (!bce->makeAtomIndex(tagAtom, &index))
            return false;
        EMIT_INDEX_OP(JSOP_STRING, index);
    }

    JS_ASSERT(pn->pn_count != 0);
    ParseNode *pn2 = pn->pn_head;
    if (pn2->isKind(PNK_XMLCURLYEXPR) && Emit1(cx, bce, JSOP_STARTXMLEXPR) < 0)
        return false;
    if (!EmitTree(cx, bce, pn2))
        return false;
    if (Emit1(cx, bce, JSOP_ADD) < 0)
        return false;

    uint32 i;
    for (pn2 = pn2->pn_next, i = 0; pn2; pn2 = pn2->pn_next, i++) {
        if (pn2->isKind(PNK_XMLCURLYEXPR) && Emit1(cx, bce, JSOP_STARTXMLEXPR) < 0)
            return false;
        if (!EmitTree(cx, bce, pn2))
            return false;
        if ((i & 1) && pn2->isKind(PNK_XMLCURLYEXPR)) {
            if (Emit1(cx, bce, JSOP_TOATTRVAL) < 0)
                return false;
        }
        if (Emit1(cx, bce, (i & 1) ? JSOP_ADDATTRVAL : JSOP_ADDATTRNAME) < 0)
            return false;
    }

    {
        jsatomid index;
        JSAtom *tmp = (pn->isKind(PNK_XMLPTAGC)) ? cx->runtime->atomState.ptagcAtom
                                                 : cx->runtime->atomState.tagcAtom;
        if (!bce->makeAtomIndex(tmp, &index))
            return false;
        EMIT_INDEX_OP(JSOP_STRING, index);
    }
    if (Emit1(cx, bce, JSOP_ADD) < 0)
        return false;

    if ((pn->pn_xflags & PNX_XMLROOT) && Emit1(cx, bce, pn->getOp()) < 0)
        return false;

    return true;
}

static bool
EmitXMLProcessingInstruction(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    JS_ASSERT(!bce->inStrictMode());

    jsatomid index;
    if (!bce->makeAtomIndex(pn->pn_pidata, &index))
        return false;
    if (!EmitIndexOp(cx, JSOP_QNAMEPART, index, bce))
        return false;
    if (!EmitAtomOp(cx, pn, JSOP_XMLPI, bce))
        return false;
    return true;
}
#endif

static bool
EmitLexicalScope(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    StmtInfo stmtInfo;
    StmtInfo *stmt;
    ObjectBox *objbox = pn->pn_objbox;
    PushBlockScope(bce, &stmtInfo, objbox, bce->offset());

    /*
     * If this lexical scope is not for a catch block, let block or let
     * expression, or any kind of for loop (where the scope starts in the
     * head after the first part if for (;;), else in the body if for-in);
     * and if our container is top-level but not a function body, or else
     * a block statement; then emit a SRC_BRACE note.  All other container
     * statements get braces by default from the decompiler.
     */
    ptrdiff_t noteIndex = -1;
    ParseNodeKind kind = pn->expr()->getKind();
    if (kind != PNK_CATCH && kind != PNK_LET && kind != PNK_FOR &&
        (!(stmt = stmtInfo.down)
         ? !bce->inFunction()
         : stmt->type == STMT_BLOCK))
    {
#if defined DEBUG_brendan || defined DEBUG_mrbkap
        /* There must be no source note already output for the next op. */
        JS_ASSERT(bce->noteCount() == 0 ||
                  bce->lastNoteOffset() != bce->offset() ||
                  !GettableNoteForNextOp(bce));
#endif
        noteIndex = NewSrcNote2(cx, bce, SRC_BRACE, 0);
        if (noteIndex < 0)
            return false;
    }

    ptrdiff_t top = bce->offset();
    if (!EmitEnterBlock(cx, pn, bce))
        return false;

    if (!EmitTree(cx, bce, pn->pn_expr))
        return false;

    JSOp op = pn->getOp();
    if (op == JSOP_LEAVEBLOCKEXPR) {
        if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - top) < 0)
            return false;
    } else {
        if (noteIndex >= 0 && !SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 0, bce->offset() - top))
            return false;
    }

    /* Emit the JSOP_LEAVEBLOCK or JSOP_LEAVEBLOCKEXPR opcode. */
    if (!EmitLeaveBlock(cx, bce, op, objbox))
        return false;

    return PopStatementBCE(cx, bce);
}

static bool
EmitWith(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    StmtInfo stmtInfo;
    if (!EmitTree(cx, bce, pn->pn_left))
        return false;
    PushStatement(bce, &stmtInfo, STMT_WITH, bce->offset());
    if (Emit1(cx, bce, JSOP_ENTERWITH) < 0)
        return false;

    /* Make blockChain determination quicker. */
    if (EmitBlockChain(cx, bce) < 0)
        return false;
    if (!EmitTree(cx, bce, pn->pn_right))
        return false;
    if (Emit1(cx, bce, JSOP_LEAVEWITH) < 0)
        return false;
    return PopStatementBCE(cx, bce);
}

static bool
EmitForIn(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, ptrdiff_t top)
{
    StmtInfo stmtInfo;
    PushStatement(bce, &stmtInfo, STMT_FOR_IN_LOOP, top);

    ParseNode *forHead = pn->pn_left;
    ParseNode *forBody = pn->pn_right;

    /*
     * If the left part is 'var x', emit code to define x if necessary
     * using a prolog opcode, but do not emit a pop. If the left part
     * was originally 'var x = i', the parser will have rewritten it;
     * see Parser::forStatement. 'for (let x = i in o)' is mercifully
     * banned.
     */
    bool forLet = false;
    if (ParseNode *decl = forHead->pn_kid1) {
        JS_ASSERT(decl->isKind(PNK_VAR) || decl->isKind(PNK_LET));
        forLet = decl->isKind(PNK_LET);
        bce->flags |= TCF_IN_FOR_INIT;
        if (!EmitTree(cx, bce, decl))
            return false;
        bce->flags &= ~TCF_IN_FOR_INIT;
    }

    /* Compile the object expression to the right of 'in'. */
    {
        TempPopScope tps;
        if (forLet && !tps.popBlock(cx, bce))
            return false;
        if (!EmitTree(cx, bce, forHead->pn_kid3))
            return false;
        if (forLet && !tps.repushBlock(cx, bce))
            return false;
    }

    /*
     * Emit a bytecode to convert top of stack value to the iterator
     * object depending on the loop variant (for-in, for-each-in, or
     * destructuring for-in).
     */
    JS_ASSERT(pn->isOp(JSOP_ITER));
    if (Emit2(cx, bce, JSOP_ITER, (uint8) pn->pn_iflags) < 0)
        return false;

    /* Annotate so the decompiler can find the loop-closing jump. */
    intN noteIndex = NewSrcNote(cx, bce, SRC_FOR_IN);
    if (noteIndex < 0)
        return false;

    /*
     * Jump down to the loop condition to minimize overhead assuming at
     * least one iteration, as the other loop forms do.
     */
    ptrdiff_t jmp = EmitJump(cx, bce, JSOP_GOTO, 0);
    if (jmp < 0)
        return false;

    intN noteIndex2 = NewSrcNote(cx, bce, SRC_TRACE);
    if (noteIndex2 < 0)
        return false;

    top = bce->offset();
    SET_STATEMENT_TOP(&stmtInfo, top);
    if (EmitTraceOp(cx, bce, NULL) < 0)
        return false;

#ifdef DEBUG
    intN loopDepth = bce->stackDepth;
#endif

    /*
     * Emit code to get the next enumeration value and assign it to the
     * left hand side. The JSOP_POP after this assignment is annotated
     * so that the decompiler can distinguish 'for (x in y)' from
     * 'for (var x in y)'.
     */
    if (!EmitAssignment(cx, bce, forHead->pn_kid2, JSOP_NOP, NULL))
        return false;
    ptrdiff_t tmp2 = bce->offset();
    if (forHead->pn_kid1 && NewSrcNote2(cx, bce, SRC_DECL,
                                        (forHead->pn_kid1->isOp(JSOP_DEFVAR))
                                        ? SRC_DECL_VAR
                                        : SRC_DECL_LET) < 0) {
        return false;
    }
    if (Emit1(cx, bce, JSOP_POP) < 0)
        return false;

    /* The stack should be balanced around the assignment opcode sequence. */
    JS_ASSERT(bce->stackDepth == loopDepth);

    /* Emit code for the loop body. */
    if (!EmitTree(cx, bce, forBody))
        return false;

    /* Set loop and enclosing "update" offsets, for continue. */
    StmtInfo *stmt = &stmtInfo;
    do {
        stmt->update = bce->offset();
    } while ((stmt = stmt->down) != NULL && stmt->type == STMT_LABEL);

    /*
     * Fixup the goto that starts the loop to jump down to JSOP_MOREITER.
     */
    CHECK_AND_SET_JUMP_OFFSET_AT(cx, bce, jmp);
    if (Emit1(cx, bce, JSOP_MOREITER) < 0)
        return false;
    ptrdiff_t beq = EmitJump(cx, bce, JSOP_IFNE, top - bce->offset());
    if (beq < 0)
        return false;

    /*
     * Be careful: We must set noteIndex2 before noteIndex in case the noteIndex
     * note gets bigger.
     */
    if (!SetSrcNoteOffset(cx, bce, (uintN)noteIndex2, 0, beq - top))
        return false;
    /* Set the first srcnote offset so we can find the start of the loop body. */
    if (!SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 0, tmp2 - jmp))
        return false;
    /* Set the second srcnote offset so we can find the closing jump. */
    if (!SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 1, beq - jmp))
        return false;

    /* Now fixup all breaks and continues (before the JSOP_ENDITER). */
    if (!PopStatementBCE(cx, bce))
        return false;

    if (!NewTryNote(cx, bce, JSTRY_ITER, bce->stackDepth, top, bce->offset()))
        return false;

    return Emit1(cx, bce, JSOP_ENDITER) >= 0;
}

static bool
EmitNormalFor(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, ptrdiff_t top)
{
    StmtInfo stmtInfo;
    PushStatement(bce, &stmtInfo, STMT_FOR_LOOP, top);

    ParseNode *forHead = pn->pn_left;
    ParseNode *forBody = pn->pn_right;

    /* C-style for (init; cond; update) ... loop. */
    JSOp op = JSOP_POP;
    ParseNode *pn3 = forHead->pn_kid1;
    if (!pn3) {
        /* No initializer: emit an annotated nop for the decompiler. */
        op = JSOP_NOP;
    } else {
        bce->flags |= TCF_IN_FOR_INIT;
#if JS_HAS_DESTRUCTURING
        if (pn3->isKind(PNK_ASSIGN)) {
            JS_ASSERT(pn3->isOp(JSOP_NOP));
            if (!MaybeEmitGroupAssignment(cx, bce, op, pn3, &op))
                return false;
        }
#endif
        if (op == JSOP_POP) {
            if (!EmitTree(cx, bce, pn3))
                return false;
            if (pn3->isKind(PNK_VAR) || pn3->isKind(PNK_CONST) || pn3->isKind(PNK_LET)) {
                /*
                 * Check whether a destructuring-initialized var decl
                 * was optimized to a group assignment.  If so, we do
                 * not need to emit a pop below, so switch to a nop,
                 * just for the decompiler.
                 */
                JS_ASSERT(pn3->isArity(PN_LIST));
                if (pn3->pn_xflags & PNX_GROUPINIT)
                    op = JSOP_NOP;
            }
        }
        bce->flags &= ~TCF_IN_FOR_INIT;
    }

    /*
     * NB: the SRC_FOR note has offsetBias 1 (JSOP_{NOP,POP}_LENGTH).
     * Use tmp to hold the biased srcnote "top" offset, which differs
     * from the top local variable by the length of the JSOP_GOTO{,X}
     * emitted in between tmp and top if this loop has a condition.
     */
    intN noteIndex = NewSrcNote(cx, bce, SRC_FOR);
    if (noteIndex < 0 || Emit1(cx, bce, op) < 0)
        return false;
    ptrdiff_t tmp = bce->offset();

    ptrdiff_t jmp = -1;
    if (forHead->pn_kid2) {
        /* Goto the loop condition, which branches back to iterate. */
        jmp = EmitJump(cx, bce, JSOP_GOTO, 0);
        if (jmp < 0)
            return false;
    }

    top = bce->offset();
    SET_STATEMENT_TOP(&stmtInfo, top);

    intN noteIndex2 = NewSrcNote(cx, bce, SRC_TRACE);
    if (noteIndex2 < 0)
        return false;

    /* Emit code for the loop body. */
    if (EmitTraceOp(cx, bce, forBody) < 0)
        return false;
    if (!EmitTree(cx, bce, forBody))
        return false;

    /* Set the second note offset so we can find the update part. */
    JS_ASSERT(noteIndex != -1);
    ptrdiff_t tmp2 = bce->offset();

    /* Set loop and enclosing "update" offsets, for continue. */
    StmtInfo *stmt = &stmtInfo;
    do {
        stmt->update = bce->offset();
    } while ((stmt = stmt->down) != NULL && stmt->type == STMT_LABEL);

    /* Check for update code to do before the condition (if any). */
    pn3 = forHead->pn_kid3;
    if (pn3) {
        op = JSOP_POP;
#if JS_HAS_DESTRUCTURING
        if (pn3->isKind(PNK_ASSIGN)) {
            JS_ASSERT(pn3->isOp(JSOP_NOP));
            if (!MaybeEmitGroupAssignment(cx, bce, op, pn3, &op))
                return false;
        }
#endif
        if (op == JSOP_POP && !EmitTree(cx, bce, pn3))
            return false;

        /* Always emit the POP or NOP, to help the decompiler. */
        if (Emit1(cx, bce, op) < 0)
            return false;

        /* Restore the absolute line number for source note readers. */
        ptrdiff_t lineno = pn->pn_pos.end.lineno;
        if (bce->currentLine() != (uintN) lineno) {
            if (NewSrcNote2(cx, bce, SRC_SETLINE, lineno) < 0)
                return false;
            bce->current->currentLine = (uintN) lineno;
        }
    }

    ptrdiff_t tmp3 = bce->offset();

    if (forHead->pn_kid2) {
        /* Fix up the goto from top to target the loop condition. */
        JS_ASSERT(jmp >= 0);
        CHECK_AND_SET_JUMP_OFFSET_AT(cx, bce, jmp);

        if (!EmitTree(cx, bce, forHead->pn_kid2))
            return false;
    }

    /*
     * Be careful: We must set noteIndex2 before noteIndex in case the noteIndex
     * note gets bigger.
     */
    if (!SetSrcNoteOffset(cx, bce, (uintN)noteIndex2, 0, bce->offset() - top))
        return false;
    /* Set the first note offset so we can find the loop condition. */
    if (!SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 0, tmp3 - tmp))
        return false;
    if (!SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 1, tmp2 - tmp))
        return false;
    /* The third note offset helps us find the loop-closing jump. */
    if (!SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 2, bce->offset() - tmp))
        return false;

    /* If no loop condition, just emit a loop-closing jump. */
    op = forHead->pn_kid2 ? JSOP_IFNE : JSOP_GOTO;
    if (EmitJump(cx, bce, op, top - bce->offset()) < 0)
        return false;

    /* Now fixup all breaks and continues. */
    return PopStatementBCE(cx, bce);
}

static inline bool
EmitFor(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn, ptrdiff_t top)
{
    JS_ASSERT(pn->pn_left->isKind(PNK_FORIN) || pn->pn_left->isKind(PNK_FORHEAD));
    return pn->pn_left->isKind(PNK_FORIN)
           ? EmitForIn(cx, bce, pn, top)
           : EmitNormalFor(cx, bce, pn, top);
}

JSBool
frontend::EmitTree(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn)
{
    JSBool useful, wantval;
    StmtInfo stmtInfo;
    StmtInfo *stmt;
    ptrdiff_t top, off, tmp, beq, jmp;
    ParseNode *pn2, *pn3;
    JSAtom *atom;
    jsatomid atomIndex;
    uintN index;
    ptrdiff_t noteIndex, noteIndex2;
    SrcNoteType noteType;
    jsbytecode *pc;
    JSOp op;
    uint32 argc;
    EmitLevelManager elm(bce);
    jsint sharpnum = -1;

    JS_CHECK_RECURSION(cx, return JS_FALSE);

    JSBool ok = true;
    pn->pn_offset = top = bce->offset();

    /* Emit notes to tell the current bytecode's source line number. */
    UPDATE_LINE_NUMBER_NOTES(cx, bce, pn->pn_pos.begin.lineno);

    switch (pn->getKind()) {
      case PNK_FUNCTION:
      {
        JSFunction *fun;
        uintN slot;

#if JS_HAS_XML_SUPPORT
        if (pn->isArity(PN_NULLARY)) {
            if (Emit1(cx, bce, JSOP_GETFUNNS) < 0)
                return JS_FALSE;
            break;
        }
#endif

        fun = pn->pn_funbox->function();
        JS_ASSERT(fun->isInterpreted());
        if (fun->script()) {
            /*
             * This second pass is needed to emit JSOP_NOP with a source note
             * for the already-emitted function definition prolog opcode. See
             * comments in the PNK_STATEMENTLIST case.
             */
            JS_ASSERT(pn->isOp(JSOP_NOP));
            JS_ASSERT(bce->inFunction());
            if (!EmitFunctionDefNop(cx, bce, pn->pn_index))
                return JS_FALSE;
            break;
        }

        JS_ASSERT_IF(pn->pn_funbox->tcflags & TCF_FUN_HEAVYWEIGHT,
                     fun->kind() == JSFUN_INTERPRETED);

        /*
         * Generate code for the function's body.  bce2 is not allocated on the
         * stack because doing so significantly reduces the maximum depth of
         * nested functions we can handle.  See bug 696284.
         */
        BytecodeEmitter *bce2 = cx->new_<BytecodeEmitter>(bce->parser, pn->pn_pos.begin.lineno);
        if (!bce2) {
            js_ReportOutOfMemory(cx);
            return JS_FALSE;
        }
        if (!bce2->init(cx))
            return JS_FALSE;

        bce2->flags = pn->pn_funbox->tcflags | TCF_COMPILING | TCF_IN_FUNCTION |
                     (bce->flags & TCF_FUN_MIGHT_ALIAS_LOCALS);
        bce2->bindings.transfer(cx, &pn->pn_funbox->bindings);
#if JS_HAS_SHARP_VARS
        if (bce2->flags & TCF_HAS_SHARPS) {
            bce2->sharpSlotBase = bce2->bindings.sharpSlotBase(cx);
            if (bce2->sharpSlotBase < 0)
                return JS_FALSE;
        }
#endif
        bce2->setFunction(fun);
        bce2->funbox = pn->pn_funbox;
        bce2->parent = bce;
        bce2->globalScope = bce->globalScope;

        /*
         * js::frontend::SetStaticLevel limited static nesting depth to fit in
         * 16 bits and to reserve the all-ones value, thereby reserving the
         * magic FREE_UPVAR_COOKIE value. Note the bce2->staticLevel assignment
         * below.
         */
        JS_ASSERT(bce->staticLevel < JS_BITMASK(16) - 1);
        bce2->staticLevel = bce->staticLevel + 1;

        /* We measured the max scope depth when we parsed the function. */
        if (!EmitFunctionScript(cx, bce2, pn->pn_body))
            pn = NULL;

        cx->delete_(bce2);
        bce2 = NULL;
        if (!pn)
            return JS_FALSE;

        /* Make the function object a literal in the outer script's pool. */
        index = bce->objectList.index(pn->pn_funbox);

        /* Emit a bytecode pointing to the closure object in its immediate. */
        op = pn->getOp();
        if (op != JSOP_NOP) {
            if ((pn->pn_funbox->tcflags & TCF_GENEXP_LAMBDA) &&
                NewSrcNote(cx, bce, SRC_GENEXP) < 0)
            {
                return JS_FALSE;
            }
            EMIT_INDEX_OP(op, index);

            /* Make blockChain determination quicker. */
            if (EmitBlockChain(cx, bce) < 0)
                return JS_FALSE;
            break;
        }

        /*
         * For a script we emit the code as we parse. Thus the bytecode for
         * top-level functions should go in the prolog to predefine their
         * names in the variable object before the already-generated main code
         * is executed. This extra work for top-level scripts is not necessary
         * when we emit the code for a function. It is fully parsed prior to
         * invocation of the emitter and calls to EmitTree for function
         * definitions can be scheduled before generating the rest of code.
         */
        if (!bce->inFunction()) {
            JS_ASSERT(!bce->topStmt);
            if (!BindGlobal(cx, bce, pn, fun->atom))
                return false;
            if (pn->pn_cookie.isFree()) {
                bce->switchToProlog();
                op = fun->isFlatClosure() ? JSOP_DEFFUN_FC : JSOP_DEFFUN;
                EMIT_INDEX_OP(op, index);

                /* Make blockChain determination quicker. */
                if (EmitBlockChain(cx, bce) < 0)
                    return JS_FALSE;
                bce->switchToMain();
            }

            /* Emit NOP for the decompiler. */
            if (!EmitFunctionDefNop(cx, bce, index))
                return JS_FALSE;
        } else {
            DebugOnly<BindingKind> kind = bce->bindings.lookup(cx, fun->atom, &slot);
            JS_ASSERT(kind == VARIABLE || kind == CONSTANT);
            JS_ASSERT(index < JS_BIT(20));
            pn->pn_index = index;
            op = fun->isFlatClosure() ? JSOP_DEFLOCALFUN_FC : JSOP_DEFLOCALFUN;
            if (pn->isClosed() &&
                !bce->callsEval() &&
                !bce->closedVars.append(pn->pn_cookie.slot())) {
                return JS_FALSE;
            }
            if (!EmitSlotIndexOp(cx, op, slot, index, bce))
                return JS_FALSE;

            /* Make blockChain determination quicker. */
            if (EmitBlockChain(cx, bce) < 0)
                return JS_FALSE;
        }
        break;
      }

      case PNK_ARGSBODY:
      {
        ParseNode *pnlast = pn->last();
        for (ParseNode *pn2 = pn->pn_head; pn2 != pnlast; pn2 = pn2->pn_next) {
            if (!pn2->isDefn())
                continue;
            if (!BindNameToSlot(cx, bce, pn2))
                return JS_FALSE;
            if (JOF_OPTYPE(pn2->getOp()) == JOF_QARG && bce->shouldNoteClosedName(pn2)) {
                if (!bce->closedArgs.append(pn2->pn_cookie.slot()))
                    return JS_FALSE;
            }
        }
        ok = EmitTree(cx, bce, pnlast);
        break;
      }

      case PNK_UPVARS:
        JS_ASSERT(pn->pn_names->count() != 0);
        bce->roLexdeps = pn->pn_names;
        ok = EmitTree(cx, bce, pn->pn_tree);
        bce->roLexdeps.clearMap();
        pn->pn_names.releaseMap(cx);
        break;

      case PNK_IF:
        ok = EmitIf(cx, bce, pn);
        break;

      case PNK_SWITCH:
        ok = EmitSwitch(cx, bce, pn);
        break;

      case PNK_WHILE:
        /*
         * Minimize bytecodes issued for one or more iterations by jumping to
         * the condition below the body and closing the loop if the condition
         * is true with a backward branch. For iteration count i:
         *
         *  i    test at the top                 test at the bottom
         *  =    ===============                 ==================
         *  0    ifeq-pass                       goto; ifne-fail
         *  1    ifeq-fail; goto; ifne-pass      goto; ifne-pass; ifne-fail
         *  2    2*(ifeq-fail; goto); ifeq-pass  goto; 2*ifne-pass; ifne-fail
         *  . . .
         *  N    N*(ifeq-fail; goto); ifeq-pass  goto; N*ifne-pass; ifne-fail
         *
         * SpiderMonkey, pre-mozilla.org, emitted while parsing and so used
         * test at the top. When ParseNode trees were added during the ES3
         * work (1998-9), the code generation scheme was not optimized, and
         * the decompiler continued to take advantage of the branch and jump
         * that bracketed the body. But given the SRC_WHILE note, it is easy
         * to support the more efficient scheme.
         */
        PushStatement(bce, &stmtInfo, STMT_WHILE_LOOP, top);
        noteIndex = NewSrcNote(cx, bce, SRC_WHILE);
        if (noteIndex < 0)
            return JS_FALSE;
        jmp = EmitJump(cx, bce, JSOP_GOTO, 0);
        if (jmp < 0)
            return JS_FALSE;
        noteIndex2 = NewSrcNote(cx, bce, SRC_TRACE);
        if (noteIndex2 < 0)
            return JS_FALSE;
        top = EmitTraceOp(cx, bce, pn->pn_right);
        if (top < 0)
            return JS_FALSE;
        if (!EmitTree(cx, bce, pn->pn_right))
            return JS_FALSE;
        CHECK_AND_SET_JUMP_OFFSET_AT(cx, bce, jmp);
        if (!EmitTree(cx, bce, pn->pn_left))
            return JS_FALSE;
        beq = EmitJump(cx, bce, JSOP_IFNE, top - bce->offset());
        if (beq < 0)
            return JS_FALSE;
        /*
         * Be careful: We must set noteIndex2 before noteIndex in case the noteIndex
         * note gets bigger.
         */
        if (!SetSrcNoteOffset(cx, bce, noteIndex2, 0, beq - top))
            return JS_FALSE;
        if (!SetSrcNoteOffset(cx, bce, noteIndex, 0, beq - jmp))
            return JS_FALSE;
        ok = PopStatementBCE(cx, bce);
        break;

      case PNK_DOWHILE:
        /* Emit an annotated nop so we know to decompile a 'do' keyword. */
        noteIndex = NewSrcNote(cx, bce, SRC_WHILE);
        if (noteIndex < 0 || Emit1(cx, bce, JSOP_NOP) < 0)
            return JS_FALSE;

        noteIndex2 = NewSrcNote(cx, bce, SRC_TRACE);
        if (noteIndex2 < 0)
            return JS_FALSE;

        /* Compile the loop body. */
        top = EmitTraceOp(cx, bce, pn->pn_left);
        if (top < 0)
            return JS_FALSE;
        PushStatement(bce, &stmtInfo, STMT_DO_LOOP, top);
        if (!EmitTree(cx, bce, pn->pn_left))
            return JS_FALSE;

        /* Set loop and enclosing label update offsets, for continue. */
        off = bce->offset();
        stmt = &stmtInfo;
        do {
            stmt->update = off;
        } while ((stmt = stmt->down) != NULL && stmt->type == STMT_LABEL);

        /* Compile the loop condition, now that continues know where to go. */
        if (!EmitTree(cx, bce, pn->pn_right))
            return JS_FALSE;

        /*
         * Since we use JSOP_IFNE for other purposes as well as for do-while
         * loops, we must store 1 + (beq - top) in the SRC_WHILE note offset,
         * and the decompiler must get that delta and decompile recursively.
         */
        beq = EmitJump(cx, bce, JSOP_IFNE, top - bce->offset());
        if (beq < 0)
            return JS_FALSE;
        /*
         * Be careful: We must set noteIndex2 before noteIndex in case the noteIndex
         * note gets bigger.
         */
        if (!SetSrcNoteOffset(cx, bce, noteIndex2, 0, beq - top))
            return JS_FALSE;
        if (!SetSrcNoteOffset(cx, bce, noteIndex, 0, 1 + (off - top)))
            return JS_FALSE;
        ok = PopStatementBCE(cx, bce);
        break;

      case PNK_FOR:
        ok = EmitFor(cx, bce, pn, top);
        break;

      case PNK_BREAK: {
        stmt = bce->topStmt;
        atom = pn->pn_atom;

        jsatomid labelIndex;
        if (atom) {
            if (!bce->makeAtomIndex(atom, &labelIndex))
                return JS_FALSE;

            while (stmt->type != STMT_LABEL || stmt->label != atom)
                stmt = stmt->down;
            noteType = SRC_BREAK2LABEL;
        } else {
            labelIndex = INVALID_ATOMID;
            while (!STMT_IS_LOOP(stmt) && stmt->type != STMT_SWITCH)
                stmt = stmt->down;
            noteType = (stmt->type == STMT_SWITCH) ? SRC_SWITCHBREAK : SRC_BREAK;
        }

        if (EmitGoto(cx, bce, stmt, &stmt->breaks, labelIndex, noteType) < 0)
            return JS_FALSE;
        break;
      }

      case PNK_CONTINUE: {
        stmt = bce->topStmt;
        atom = pn->pn_atom;

        jsatomid labelIndex;
        if (atom) {
            /* Find the loop statement enclosed by the matching label. */
            StmtInfo *loop = NULL;
            if (!bce->makeAtomIndex(atom, &labelIndex))
                return JS_FALSE;
            while (stmt->type != STMT_LABEL || stmt->label != atom) {
                if (STMT_IS_LOOP(stmt))
                    loop = stmt;
                stmt = stmt->down;
            }
            stmt = loop;
            noteType = SRC_CONT2LABEL;
        } else {
            labelIndex = INVALID_ATOMID;
            while (!STMT_IS_LOOP(stmt))
                stmt = stmt->down;
            noteType = SRC_CONTINUE;
        }

        if (EmitGoto(cx, bce, stmt, &stmt->continues, labelIndex, noteType) < 0)
            return JS_FALSE;
        break;
      }

      case PNK_WITH:
        ok = EmitWith(cx, bce, pn);
        break;

      case PNK_TRY:
        if (!EmitTry(cx, bce, pn))
            return false;
        break;

      case PNK_CATCH:
        if (!EmitCatch(cx, bce, pn))
            return false;
        break;

      case PNK_VAR:
      case PNK_CONST:
        if (!EmitVariables(cx, bce, pn, JS_FALSE, &noteIndex))
            return JS_FALSE;
        break;

      case PNK_RETURN:
        /* Push a return value */
        pn2 = pn->pn_kid;
        if (pn2) {
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;
        } else {
            if (Emit1(cx, bce, JSOP_PUSH) < 0)
                return JS_FALSE;
        }

        /*
         * EmitNonLocalJumpFixup may add fixup bytecode to close open try
         * blocks having finally clauses and to exit intermingled let blocks.
         * We can't simply transfer control flow to our caller in that case,
         * because we must gosub to those finally clauses from inner to outer,
         * with the correct stack pointer (i.e., after popping any with,
         * for/in, etc., slots nested inside the finally's try).
         *
         * In this case we mutate JSOP_RETURN into JSOP_SETRVAL and add an
         * extra JSOP_RETRVAL after the fixups.
         */
        top = bce->offset();
        if (Emit1(cx, bce, JSOP_RETURN) < 0)
            return JS_FALSE;
        if (!EmitNonLocalJumpFixup(cx, bce, NULL))
            return JS_FALSE;
        if (top + JSOP_RETURN_LENGTH != bce->offset()) {
            bce->base()[top] = JSOP_SETRVAL;
            if (Emit1(cx, bce, JSOP_RETRVAL) < 0)
                return JS_FALSE;
            if (EmitBlockChain(cx, bce) < 0)
                return JS_FALSE;
        }
        break;

#if JS_HAS_GENERATORS
      case PNK_YIELD:
        JS_ASSERT(bce->inFunction());
        if (pn->pn_kid) {
            if (!EmitTree(cx, bce, pn->pn_kid))
                return JS_FALSE;
        } else {
            if (Emit1(cx, bce, JSOP_PUSH) < 0)
                return JS_FALSE;
        }
        if (pn->pn_hidden && NewSrcNote(cx, bce, SRC_HIDDEN) < 0)
            return JS_FALSE;
        if (Emit1(cx, bce, JSOP_YIELD) < 0)
            return JS_FALSE;
        break;
#endif

#if JS_HAS_XML_SUPPORT
      case PNK_XMLCURLYEXPR:
        JS_ASSERT(pn->isArity(PN_UNARY));
        if (!EmitTree(cx, bce, pn->pn_kid))
            return JS_FALSE;
        if (Emit1(cx, bce, pn->getOp()) < 0)
            return JS_FALSE;
        break;
#endif

      case PNK_STATEMENTLIST:
      {
        JS_ASSERT(pn->isArity(PN_LIST));

        noteIndex = -1;
        tmp = bce->offset();
        if (pn->pn_xflags & PNX_NEEDBRACES) {
            noteIndex = NewSrcNote2(cx, bce, SRC_BRACE, 0);
            if (noteIndex < 0 || Emit1(cx, bce, JSOP_NOP) < 0)
                return JS_FALSE;
        }

        PushStatement(bce, &stmtInfo, STMT_BLOCK, top);

        ParseNode *pnchild = pn->pn_head;
        if (pn->pn_xflags & PNX_FUNCDEFS) {
            /*
             * This block contains top-level function definitions. To ensure
             * that we emit the bytecode defining them before the rest of code
             * in the block we use a separate pass over functions. During the
             * main pass later the emitter will add JSOP_NOP with source notes
             * for the function to preserve the original functions position
             * when decompiling.
             *
             * Currently this is used only for functions, as compile-as-we go
             * mode for scripts does not allow separate emitter passes.
             */
            JS_ASSERT(bce->inFunction());
            if (pn->pn_xflags & PNX_DESTRUCT) {
                /*
                 * Assign the destructuring arguments before defining any
                 * functions, see bug 419662.
                 */
                JS_ASSERT(pnchild->isKind(PNK_SEMI));
                JS_ASSERT(pnchild->pn_kid->isKind(PNK_VAR) || pnchild->pn_kid->isKind(PNK_CONST));
                if (!EmitTree(cx, bce, pnchild))
                    return JS_FALSE;
                pnchild = pnchild->pn_next;
            }

            for (pn2 = pnchild; pn2; pn2 = pn2->pn_next) {
                if (pn2->isKind(PNK_FUNCTION)) {
                    if (pn2->isOp(JSOP_NOP)) {
                        if (!EmitTree(cx, bce, pn2))
                            return JS_FALSE;
                    } else {
                        /*
                         * JSOP_DEFFUN in a top-level block with function
                         * definitions appears, for example, when "if (true)"
                         * is optimized away from "if (true) function x() {}".
                         * See bug 428424.
                         */
                        JS_ASSERT(pn2->isOp(JSOP_DEFFUN));
                    }
                }
            }
        }
        for (pn2 = pnchild; pn2; pn2 = pn2->pn_next) {
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;
        }

        if (noteIndex >= 0 && !SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 0, bce->offset() - tmp))
            return JS_FALSE;

        ok = PopStatementBCE(cx, bce);
        break;
      }

      case PNK_SEQ:
        JS_ASSERT(pn->isArity(PN_LIST));
        PushStatement(bce, &stmtInfo, STMT_SEQ, top);
        for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;
        }
        ok = PopStatementBCE(cx, bce);
        break;

      case PNK_SEMI:
        pn2 = pn->pn_kid;
        if (pn2) {
            /*
             * Top-level or called-from-a-native JS_Execute/EvaluateScript,
             * debugger, and eval frames may need the value of the ultimate
             * expression statement as the script's result, despite the fact
             * that it appears useless to the compiler.
             *
             * API users may also set the JSOPTION_NO_SCRIPT_RVAL option when
             * calling JS_Compile* to suppress JSOP_POPV.
             */
            useful = wantval = !(bce->flags & (TCF_IN_FUNCTION | TCF_NO_SCRIPT_RVAL));

            /* Don't eliminate expressions with side effects. */
            if (!useful) {
                if (!CheckSideEffects(cx, bce, pn2, &useful))
                    return JS_FALSE;
            }

            /*
             * Don't eliminate apparently useless expressions if they are
             * labeled expression statements.  The tc->topStmt->update test
             * catches the case where we are nesting in EmitTree for a labeled
             * compound statement.
             */
            if (!useful &&
                bce->topStmt &&
                bce->topStmt->type == STMT_LABEL &&
                bce->topStmt->update >= bce->offset()) {
                useful = true;
            }

            if (!useful) {
                /* Don't complain about directive prologue members; just don't emit their code. */
                if (!pn->isDirectivePrologueMember()) {
                    bce->current->currentLine = pn2->pn_pos.begin.lineno;
                    if (!ReportCompileErrorNumber(cx, bce->tokenStream(), pn2,
                                                  JSREPORT_WARNING | JSREPORT_STRICT,
                                                  JSMSG_USELESS_EXPR)) {
                        return JS_FALSE;
                    }
                }
            } else {
                op = wantval ? JSOP_POPV : JSOP_POP;
                JS_ASSERT_IF(pn2->isKind(PNK_ASSIGN), pn2->isOp(JSOP_NOP));
#if JS_HAS_DESTRUCTURING
                if (!wantval &&
                    pn2->isKind(PNK_ASSIGN) &&
                    !MaybeEmitGroupAssignment(cx, bce, op, pn2, &op)) {
                    return JS_FALSE;
                }
#endif
                if (op != JSOP_NOP) {
                    /*
                     * Specialize JSOP_SETPROP to JSOP_SETMETHOD to defer or
                     * avoid null closure cloning. Do this only for assignment
                     * statements that are not completion values wanted by a
                     * script evaluator, to ensure that the joined function
                     * can't escape directly.
                     */
                    if (!wantval &&
                        pn2->isKind(PNK_ASSIGN) &&
                        pn2->pn_left->isOp(JSOP_SETPROP) &&
                        pn2->pn_right->isOp(JSOP_LAMBDA) &&
                        pn2->pn_right->pn_funbox->joinable()) {
                        pn2->pn_left->setOp(JSOP_SETMETHOD);
                    }
                    if (!EmitTree(cx, bce, pn2))
                        return JS_FALSE;
                    if (Emit1(cx, bce, op) < 0)
                        return JS_FALSE;
                }
            }
        }
        break;

      case PNK_COLON:
        /*
         * Emit a JSOP_LABEL instruction. The argument is the offset to the statement
         * following the labeled statement. This op has either a SRC_LABEL or
         * SRC_LABELBRACE source note for the decompiler.
         */
        atom = pn->pn_atom;

        jsatomid index;
        if (!bce->makeAtomIndex(atom, &index))
            return JS_FALSE;

        pn2 = pn->expr();
        noteType = (pn2->isKind(PNK_STATEMENTLIST) ||
                    (pn2->isKind(PNK_LEXICALSCOPE) &&
                     pn2->expr()->isKind(PNK_STATEMENTLIST)))
                   ? SRC_LABELBRACE
                   : SRC_LABEL;
        noteIndex = NewSrcNote2(cx, bce, noteType, ptrdiff_t(index));
        if (noteIndex < 0)
            return JS_FALSE;

        top = EmitJump(cx, bce, JSOP_LABEL, 0);
        if (top < 0)
            return JS_FALSE;

        /* Emit code for the labeled statement. */
        PushStatement(bce, &stmtInfo, STMT_LABEL, bce->offset());
        stmtInfo.label = atom;
        if (!EmitTree(cx, bce, pn2))
            return JS_FALSE;
        if (!PopStatementBCE(cx, bce))
            return JS_FALSE;

        /* Patch the JSOP_LABEL offset. */
        CHECK_AND_SET_JUMP_OFFSET_AT(cx, bce, top);

        /* If the statement was compound, emit a note for the end brace. */
        if (noteType == SRC_LABELBRACE) {
            if (NewSrcNote(cx, bce, SRC_ENDBRACE) < 0 ||
                Emit1(cx, bce, JSOP_NOP) < 0) {
                return JS_FALSE;
            }
        }
        break;

      case PNK_COMMA:
        /*
         * Emit SRC_PCDELTA notes on each JSOP_POP between comma operands.
         * These notes help the decompiler bracket the bytecodes generated
         * from each sub-expression that follows a comma.
         */
        off = noteIndex = -1;
        for (pn2 = pn->pn_head; ; pn2 = pn2->pn_next) {
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;
            tmp = bce->offset();
            if (noteIndex >= 0) {
                if (!SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 0, tmp-off))
                    return JS_FALSE;
            }
            if (!pn2->pn_next)
                break;
            off = tmp;
            noteIndex = NewSrcNote2(cx, bce, SRC_PCDELTA, 0);
            if (noteIndex < 0 ||
                Emit1(cx, bce, JSOP_POP) < 0) {
                return JS_FALSE;
            }
        }
        break;

      case PNK_ASSIGN:
      case PNK_ADDASSIGN:
      case PNK_SUBASSIGN:
      case PNK_BITORASSIGN:
      case PNK_BITXORASSIGN:
      case PNK_BITANDASSIGN:
      case PNK_LSHASSIGN:
      case PNK_RSHASSIGN:
      case PNK_URSHASSIGN:
      case PNK_MULASSIGN:
      case PNK_DIVASSIGN:
      case PNK_MODASSIGN:
        if (!EmitAssignment(cx, bce, pn->pn_left, pn->getOp(), pn->pn_right))
            return false;
        break;

      case PNK_HOOK:
        /* Emit the condition, then branch if false to the else part. */
        if (!EmitTree(cx, bce, pn->pn_kid1))
            return JS_FALSE;
        noteIndex = NewSrcNote(cx, bce, SRC_COND);
        if (noteIndex < 0)
            return JS_FALSE;
        beq = EmitJump(cx, bce, JSOP_IFEQ, 0);
        if (beq < 0 || !EmitTree(cx, bce, pn->pn_kid2))
            return JS_FALSE;

        /* Jump around else, fixup the branch, emit else, fixup jump. */
        jmp = EmitJump(cx, bce, JSOP_GOTO, 0);
        if (jmp < 0)
            return JS_FALSE;
        CHECK_AND_SET_JUMP_OFFSET_AT(cx, bce, beq);

        /*
         * Because each branch pushes a single value, but our stack budgeting
         * analysis ignores branches, we now have to adjust bce->stackDepth to
         * ignore the value pushed by the first branch.  Execution will follow
         * only one path, so we must decrement bce->stackDepth.
         *
         * Failing to do this will foil code, such as the try/catch/finally
         * exception handling code generator, that samples bce->stackDepth for
         * use at runtime (JSOP_SETSP), or in let expression and block code
         * generation, which must use the stack depth to compute local stack
         * indexes correctly.
         */
        JS_ASSERT(bce->stackDepth > 0);
        bce->stackDepth--;
        if (!EmitTree(cx, bce, pn->pn_kid3))
            return JS_FALSE;
        CHECK_AND_SET_JUMP_OFFSET_AT(cx, bce, jmp);
        if (!SetSrcNoteOffset(cx, bce, noteIndex, 0, jmp - beq))
            return JS_FALSE;
        break;

      case PNK_OR:
      case PNK_AND:
        /*
         * JSOP_OR converts the operand on the stack to boolean, leaves the original
         * value on the stack and jumps if true; otherwise it falls into the next
         * bytecode, which pops the left operand and then evaluates the right operand.
         * The jump goes around the right operand evaluation.
         *
         * JSOP_AND converts the operand on the stack to boolean and jumps if false;
         * otherwise it falls into the right operand's bytecode.
         */
        if (pn->isArity(PN_BINARY)) {
            if (!EmitTree(cx, bce, pn->pn_left))
                return JS_FALSE;
            top = EmitJump(cx, bce, JSOP_BACKPATCH, 0);
            if (top < 0)
                return JS_FALSE;
            if (Emit1(cx, bce, JSOP_POP) < 0)
                return JS_FALSE;
            if (!EmitTree(cx, bce, pn->pn_right))
                return JS_FALSE;
            off = bce->offset();
            pc = bce->code(top);
            CHECK_AND_SET_JUMP_OFFSET(cx, bce, pc, off - top);
            *pc = pn->getOp();
        } else {
            JS_ASSERT(pn->isArity(PN_LIST));
            JS_ASSERT(pn->pn_head->pn_next->pn_next);

            /* Left-associative operator chain: avoid too much recursion. */
            pn2 = pn->pn_head;
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;
            top = EmitJump(cx, bce, JSOP_BACKPATCH, 0);
            if (top < 0)
                return JS_FALSE;
            if (Emit1(cx, bce, JSOP_POP) < 0)
                return JS_FALSE;

            /* Emit nodes between the head and the tail. */
            jmp = top;
            while ((pn2 = pn2->pn_next)->pn_next) {
                if (!EmitTree(cx, bce, pn2))
                    return JS_FALSE;
                off = EmitJump(cx, bce, JSOP_BACKPATCH, 0);
                if (off < 0)
                    return JS_FALSE;
                if (Emit1(cx, bce, JSOP_POP) < 0)
                    return JS_FALSE;
                if (!SetBackPatchDelta(cx, bce, bce->code(jmp), off - jmp))
                    return JS_FALSE;
                jmp = off;

            }
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;

            pn2 = pn->pn_head;
            off = bce->offset();
            do {
                pc = bce->code(top);
                tmp = GetJumpOffset(bce, pc);
                CHECK_AND_SET_JUMP_OFFSET(cx, bce, pc, off - top);
                *pc = pn->getOp();
                top += tmp;
            } while ((pn2 = pn2->pn_next)->pn_next);
        }
        break;

      case PNK_ADD:
      case PNK_SUB:
      case PNK_BITOR:
      case PNK_BITXOR:
      case PNK_BITAND:
      case PNK_STRICTEQ:
      case PNK_EQ:
      case PNK_STRICTNE:
      case PNK_NE:
      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_IN:
      case PNK_INSTANCEOF:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:
      case PNK_STAR:
      case PNK_DIV:
      case PNK_MOD:
        if (pn->isArity(PN_LIST)) {
            /* Left-associative operator chain: avoid too much recursion. */
            pn2 = pn->pn_head;
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;
            op = pn->getOp();
            while ((pn2 = pn2->pn_next) != NULL) {
                if (!EmitTree(cx, bce, pn2))
                    return JS_FALSE;
                if (Emit1(cx, bce, op) < 0)
                    return JS_FALSE;
            }
        } else {
#if JS_HAS_XML_SUPPORT
            uintN oldflags;

      case PNK_DBLCOLON:
            JS_ASSERT(pn->getOp() != JSOP_XMLNAME);
            if (pn->isArity(PN_NAME)) {
                if (!EmitTree(cx, bce, pn->expr()))
                    return JS_FALSE;
                if (!EmitAtomOp(cx, pn, pn->getOp(), bce))
                    return JS_FALSE;
                break;
            }

            /*
             * Binary :: has a right operand that brackets arbitrary code,
             * possibly including a let (a = b) ... expression.  We must clear
             * TCF_IN_FOR_INIT to avoid mis-compiling such beasts.
             */
            oldflags = bce->flags;
            bce->flags &= ~TCF_IN_FOR_INIT;
#endif

            /* Binary operators that evaluate both operands unconditionally. */
            if (!EmitTree(cx, bce, pn->pn_left))
                return JS_FALSE;
            if (!EmitTree(cx, bce, pn->pn_right))
                return JS_FALSE;
#if JS_HAS_XML_SUPPORT
            bce->flags |= oldflags & TCF_IN_FOR_INIT;
#endif
            if (Emit1(cx, bce, pn->getOp()) < 0)
                return JS_FALSE;
        }
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_XMLUNARY:
        if (pn->getOp() == JSOP_XMLNAME) {
            if (!EmitXMLName(cx, pn, JSOP_XMLNAME, bce))
                return false;
        } else {
            JSOp op = pn->getOp();
            JS_ASSERT(op == JSOP_BINDXMLNAME || op == JSOP_SETXMLNAME);
            uintN oldflags = bce->flags;
            bce->flags &= ~TCF_IN_FOR_INIT;
            if (!EmitTree(cx, bce, pn->pn_kid))
                return false;
            bce->flags |= oldflags & TCF_IN_FOR_INIT;
            if (Emit1(cx, bce, op) < 0)
                return false;
        }
        break;
#endif

      case PNK_THROW:
#if JS_HAS_XML_SUPPORT
      case PNK_AT:
      case PNK_DEFXMLNS:
        JS_ASSERT(pn->isArity(PN_UNARY));
        /* FALL THROUGH */
#endif
      case PNK_TYPEOF:
      case PNK_VOID:
      case PNK_NOT:
      case PNK_BITNOT:
      case PNK_POS:
      case PNK_NEG:
      {
        /* Unary op, including unary +/-. */
        op = pn->getOp();
        pn2 = pn->pn_kid;

        JS_ASSERT(op != JSOP_XMLNAME);
        if (op == JSOP_TYPEOF && !pn2->isKind(PNK_NAME))
            op = JSOP_TYPEOFEXPR;

        uintN oldflags = bce->flags;
        bce->flags &= ~TCF_IN_FOR_INIT;
        if (!EmitTree(cx, bce, pn2))
            return JS_FALSE;
        bce->flags |= oldflags & TCF_IN_FOR_INIT;
        if (Emit1(cx, bce, op) < 0)
            return JS_FALSE;
        break;
      }

      case PNK_INC:
      case PNK_DEC:
        /* Emit lvalue-specialized code for ++/-- operators. */
        pn2 = pn->pn_kid;
        JS_ASSERT(!pn2->isKind(PNK_RP));
        op = pn->getOp();
        switch (pn2->getKind()) {
          default:
            JS_ASSERT(pn2->isKind(PNK_NAME));
            pn2->setOp(op);
            if (!BindNameToSlot(cx, bce, pn2))
                return JS_FALSE;
            op = pn2->getOp();
            if (op == JSOP_CALLEE) {
                if (Emit1(cx, bce, op) < 0)
                    return JS_FALSE;
            } else if (!pn2->pn_cookie.isFree()) {
                atomIndex = pn2->pn_cookie.asInteger();
                EMIT_UINT16_IMM_OP(op, atomIndex);
            } else {
                JS_ASSERT(JOF_OPTYPE(op) == JOF_ATOM);
                if (js_CodeSpec[op].format & (JOF_INC | JOF_DEC)) {
                    if (!EmitNameIncDec(cx, pn2, op, bce))
                        return JS_FALSE;
                } else {
                    if (!EmitAtomOp(cx, pn2, op, bce))
                        return JS_FALSE;
                }
                break;
            }
            if (pn2->isConst()) {
                if (Emit1(cx, bce, JSOP_POS) < 0)
                    return JS_FALSE;
                op = pn->getOp();
                if (!(js_CodeSpec[op].format & JOF_POST)) {
                    if (Emit1(cx, bce, JSOP_ONE) < 0)
                        return JS_FALSE;
                    op = (js_CodeSpec[op].format & JOF_INC) ? JSOP_ADD : JSOP_SUB;
                    if (Emit1(cx, bce, op) < 0)
                        return JS_FALSE;
                }
            }
            break;
          case PNK_DOT:
            if (!EmitPropIncDec(cx, pn2, op, bce))
                return JS_FALSE;
            break;
          case PNK_LB:
            if (!EmitElemIncDec(cx, pn2, op, bce))
                return JS_FALSE;
            break;
          case PNK_LP:
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;
            if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - pn2->pn_offset) < 0)
                return JS_FALSE;
            if (Emit1(cx, bce, op) < 0)
                return JS_FALSE;
            /*
             * This is dead code for the decompiler, don't generate
             * a decomposed version of the opcode. We do need to balance
             * the stacks in the decomposed version.
             */
            JS_ASSERT(js_CodeSpec[op].format & JOF_DECOMPOSE);
            JS_ASSERT(js_CodeSpec[op].format & JOF_ELEM);
            if (Emit1(cx, bce, (JSOp)1) < 0)
                return JS_FALSE;
            if (Emit1(cx, bce, JSOP_POP) < 0)
                return JS_FALSE;
            break;
#if JS_HAS_XML_SUPPORT
          case PNK_XMLUNARY:
            JS_ASSERT(!bce->inStrictMode());
            JS_ASSERT(pn2->isOp(JSOP_SETXMLNAME));
            if (!EmitTree(cx, bce, pn2->pn_kid))
                return JS_FALSE;
            if (Emit1(cx, bce, JSOP_BINDXMLNAME) < 0)
                return JS_FALSE;
            if (!EmitElemIncDec(cx, NULL, op, bce))
                return JS_FALSE;
            break;
#endif
        }
        break;

      case PNK_DELETE:
        /*
         * Under ECMA 3, deleting a non-reference returns true -- but alas we
         * must evaluate the operand if it appears it might have side effects.
         */
        pn2 = pn->pn_kid;
        switch (pn2->getKind()) {
          case PNK_NAME:
            if (!BindNameToSlot(cx, bce, pn2))
                return JS_FALSE;
            op = pn2->getOp();
            if (op == JSOP_FALSE) {
                if (Emit1(cx, bce, op) < 0)
                    return JS_FALSE;
            } else {
                if (!EmitAtomOp(cx, pn2, op, bce))
                    return JS_FALSE;
            }
            break;
          case PNK_DOT:
            if (!EmitPropOp(cx, pn2, JSOP_DELPROP, bce, JS_FALSE))
                return JS_FALSE;
            break;
#if JS_HAS_XML_SUPPORT
          case PNK_DBLDOT:
            JS_ASSERT(!bce->inStrictMode());
            if (!EmitElemOp(cx, pn2, JSOP_DELDESC, bce))
                return JS_FALSE;
            break;
#endif
          case PNK_LB:
            if (!EmitElemOp(cx, pn2, JSOP_DELELEM, bce))
                return JS_FALSE;
            break;
          default:
            /*
             * If useless, just emit JSOP_TRUE; otherwise convert delete foo()
             * to foo(), true (a comma expression, requiring SRC_PCDELTA).
             */
            useful = JS_FALSE;
            if (!CheckSideEffects(cx, bce, pn2, &useful))
                return JS_FALSE;
            if (!useful) {
                off = noteIndex = -1;
            } else {
                JS_ASSERT_IF(pn2->isKind(PNK_LP), !(pn2->pn_xflags & PNX_SETCALL));
                if (!EmitTree(cx, bce, pn2))
                    return JS_FALSE;
                off = bce->offset();
                noteIndex = NewSrcNote2(cx, bce, SRC_PCDELTA, 0);
                if (noteIndex < 0 || Emit1(cx, bce, JSOP_POP) < 0)
                    return JS_FALSE;
            }
            if (Emit1(cx, bce, JSOP_TRUE) < 0)
                return JS_FALSE;
            if (noteIndex >= 0) {
                tmp = bce->offset();
                if (!SetSrcNoteOffset(cx, bce, (uintN)noteIndex, 0, tmp-off))
                    return JS_FALSE;
            }
        }
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_FILTER:
        JS_ASSERT(!bce->inStrictMode());

        if (!EmitTree(cx, bce, pn->pn_left))
            return JS_FALSE;
        jmp = EmitJump(cx, bce, JSOP_FILTER, 0);
        if (jmp < 0)
            return JS_FALSE;
        top = EmitTraceOp(cx, bce, pn->pn_right);
        if (top < 0)
            return JS_FALSE;
        if (!EmitTree(cx, bce, pn->pn_right))
            return JS_FALSE;
        CHECK_AND_SET_JUMP_OFFSET_AT(cx, bce, jmp);
        if (EmitJump(cx, bce, JSOP_ENDFILTER, top - bce->offset()) < 0)
            return JS_FALSE;

        /* Make blockChain determination quicker. */
        if (EmitBlockChain(cx, bce) < 0)
            return JS_FALSE;
        break;
#endif

      case PNK_DOT:
        /*
         * Pop a stack operand, convert it to object, get a property named by
         * this bytecode's immediate-indexed atom operand, and push its value
         * (not a reference to it).
         */
        ok = EmitPropOp(cx, pn, pn->getOp(), bce, JS_FALSE);
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_DBLDOT:
        JS_ASSERT(!bce->inStrictMode());
        /* FALL THROUGH */
#endif
      case PNK_LB:
        /*
         * Pop two operands, convert the left one to object and the right one
         * to property name (atom or tagged int), get the named property, and
         * push its value.  Set the "obj" register to the result of ToObject
         * on the left operand.
         */
        ok = EmitElemOp(cx, pn, pn->getOp(), bce);
        break;

      case PNK_NEW:
      case PNK_LP:
      {
        bool callop = pn->isKind(PNK_LP);

        /*
         * Emit callable invocation or operator new (constructor call) code.
         * First, emit code for the left operand to evaluate the callable or
         * constructable object expression.
         *
         * For operator new applied to other expressions than E4X ones, we emit
         * JSOP_GETPROP instead of JSOP_CALLPROP, etc. This is necessary to
         * interpose the lambda-initialized method read barrier -- see the code
         * in jsinterp.cpp for JSOP_LAMBDA followed by JSOP_{SET,INIT}PROP.
         *
         * Then (or in a call case that has no explicit reference-base
         * object) we emit JSOP_PUSH to produce the |this| slot required
         * for calls (which non-strict mode functions will box into the
         * global object).
         */
        pn2 = pn->pn_head;
        switch (pn2->getKind()) {
          case PNK_NAME:
            if (!EmitNameOp(cx, bce, pn2, callop))
                return JS_FALSE;
            break;
          case PNK_DOT:
            if (!EmitPropOp(cx, pn2, pn2->getOp(), bce, callop))
                return JS_FALSE;
            break;
          case PNK_LB:
            JS_ASSERT(pn2->isOp(JSOP_GETELEM));
            if (!EmitElemOp(cx, pn2, callop ? JSOP_CALLELEM : JSOP_GETELEM, bce))
                return JS_FALSE;
            break;
#if JS_HAS_XML_SUPPORT
          case PNK_XMLUNARY:
            JS_ASSERT(pn2->isOp(JSOP_XMLNAME));
            if (!EmitXMLName(cx, pn2, JSOP_CALLXMLNAME, bce))
                return JS_FALSE;
            callop = true;          /* suppress JSOP_PUSH after */
            break;
#endif
          default:
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;
            callop = false;             /* trigger JSOP_PUSH after */
            break;
        }
        if (!callop && Emit1(cx, bce, JSOP_PUSH) < 0)
            return JS_FALSE;

        /* Remember start of callable-object bytecode for decompilation hint. */
        off = top;

        /*
         * Emit code for each argument in order, then emit the JSOP_*CALL or
         * JSOP_NEW bytecode with a two-byte immediate telling how many args
         * were pushed on the operand stack.
         */
        uintN oldflags = bce->flags;
        bce->flags &= ~TCF_IN_FOR_INIT;
        for (pn3 = pn2->pn_next; pn3; pn3 = pn3->pn_next) {
            if (!EmitTree(cx, bce, pn3))
                return JS_FALSE;
        }
        bce->flags |= oldflags & TCF_IN_FOR_INIT;
        if (NewSrcNote2(cx, bce, SRC_PCBASE, bce->offset() - off) < 0)
            return JS_FALSE;

        argc = pn->pn_count - 1;
        if (Emit3(cx, bce, pn->getOp(), ARGC_HI(argc), ARGC_LO(argc)) < 0)
            return JS_FALSE;
        CheckTypeSet(cx, bce, pn->getOp());
        if (pn->isOp(JSOP_EVAL)) {
            EMIT_UINT16_IMM_OP(JSOP_LINENO, pn->pn_pos.begin.lineno);
            if (EmitBlockChain(cx, bce) < 0)
                return JS_FALSE;
        }
        if (pn->pn_xflags & PNX_SETCALL) {
            if (Emit1(cx, bce, JSOP_SETCALL) < 0)
                return JS_FALSE;
        }
        break;
      }

      case PNK_LEXICALSCOPE:
        ok = EmitLexicalScope(cx, bce, pn);
        break;

#if JS_HAS_BLOCK_SCOPE
      case PNK_LET:
        if (!EmitLet(cx, bce, pn))
            return false;
        break;
#endif /* JS_HAS_BLOCK_SCOPE */
#if JS_HAS_GENERATORS
      case PNK_ARRAYPUSH: {
        jsint slot;

        /*
         * The array object's stack index is in bce->arrayCompDepth. See below
         * under the array initialiser code generator for array comprehension
         * special casing.
         */
        if (!EmitTree(cx, bce, pn->pn_kid))
            return JS_FALSE;
        slot = AdjustBlockSlot(cx, bce, bce->arrayCompDepth);
        if (slot < 0)
            return JS_FALSE;
        EMIT_UINT16_IMM_OP(pn->getOp(), slot);
        break;
      }
#endif

      case PNK_RB:
#if JS_HAS_GENERATORS
      case PNK_ARRAYCOMP:
#endif
        /*
         * Emit code for [a, b, c] that is equivalent to constructing a new
         * array and in source order evaluating each element value and adding
         * it to the array, without invoking latent setters.  We use the
         * JSOP_NEWINIT and JSOP_INITELEM bytecodes to ignore setters and to
         * avoid dup'ing and popping the array as each element is added, as
         * JSOP_SETELEM/JSOP_SETPROP would do.
         */
#if JS_HAS_SHARP_VARS
        sharpnum = -1;
      do_emit_array:
#endif

#if JS_HAS_GENERATORS
        if (pn->isKind(PNK_ARRAYCOMP)) {
            uintN saveDepth;

            if (!EmitNewInit(cx, bce, JSProto_Array, pn, sharpnum))
                return JS_FALSE;

            /*
             * Pass the new array's stack index to the PNK_ARRAYPUSH case via
             * bce->arrayCompDepth, then simply traverse the PNK_FOR node and
             * its kids under pn2 to generate this comprehension.
             */
            JS_ASSERT(bce->stackDepth > 0);
            saveDepth = bce->arrayCompDepth;
            bce->arrayCompDepth = (uint32) (bce->stackDepth - 1);
            if (!EmitTree(cx, bce, pn->pn_head))
                return JS_FALSE;
            bce->arrayCompDepth = saveDepth;

            /* Emit the usual op needed for decompilation. */
            if (!EmitEndInit(cx, bce, 1))
                return JS_FALSE;
            break;
        }
#endif /* JS_HAS_GENERATORS */

        if (!bce->hasSharps() && !(pn->pn_xflags & PNX_NONCONST) && pn->pn_head &&
            bce->checkSingletonContext()) {
            if (!EmitSingletonInitialiser(cx, bce, pn))
                return JS_FALSE;
            break;
        }

        /* Use the slower NEWINIT for arrays in scripts containing sharps. */
        if (bce->hasSharps()) {
            if (!EmitNewInit(cx, bce, JSProto_Array, pn, sharpnum))
                return JS_FALSE;
        } else {
            ptrdiff_t off = EmitN(cx, bce, JSOP_NEWARRAY, 3);
            if (off < 0)
                return JS_FALSE;
            pc = bce->code(off);
            SET_UINT24(pc, pn->pn_count);
        }

        pn2 = pn->pn_head;
        for (atomIndex = 0; pn2; atomIndex++, pn2 = pn2->pn_next) {
            if (!EmitNumberOp(cx, atomIndex, bce))
                return JS_FALSE;
            if (pn2->isKind(PNK_COMMA) && pn2->isArity(PN_NULLARY)) {
                if (Emit1(cx, bce, JSOP_HOLE) < 0)
                    return JS_FALSE;
            } else {
                if (!EmitTree(cx, bce, pn2))
                    return JS_FALSE;
            }
            if (Emit1(cx, bce, JSOP_INITELEM) < 0)
                return JS_FALSE;
        }
        JS_ASSERT(atomIndex == pn->pn_count);

        if (pn->pn_xflags & PNX_ENDCOMMA) {
            /* Emit a source note so we know to decompile an extra comma. */
            if (NewSrcNote(cx, bce, SRC_CONTINUE) < 0)
                return JS_FALSE;
        }

        /*
         * Emit an op to finish the array and, secondarily, to aid in sharp
         * array cleanup (if JS_HAS_SHARP_VARS) and decompilation.
         */
        if (!EmitEndInit(cx, bce, atomIndex))
            return JS_FALSE;
        break;

      case PNK_RC: {
#if JS_HAS_SHARP_VARS
        sharpnum = -1;
      do_emit_object:
#endif
#if JS_HAS_DESTRUCTURING_SHORTHAND
        if (pn->pn_xflags & PNX_DESTRUCT) {
            ReportCompileErrorNumber(cx, bce->tokenStream(), pn, JSREPORT_ERROR,
                                     JSMSG_BAD_OBJECT_INIT);
            return JS_FALSE;
        }
#endif

        if (!bce->hasSharps() && !(pn->pn_xflags & PNX_NONCONST) && pn->pn_head &&
            bce->checkSingletonContext()) {
            if (!EmitSingletonInitialiser(cx, bce, pn))
                return JS_FALSE;
            break;
        }

        /*
         * Emit code for {p:a, '%q':b, 2:c} that is equivalent to constructing
         * a new object and in source order evaluating each property value and
         * adding the property to the object, without invoking latent setters.
         * We use the JSOP_NEWINIT and JSOP_INITELEM/JSOP_INITPROP bytecodes to
         * ignore setters and to avoid dup'ing and popping the object as each
         * property is added, as JSOP_SETELEM/JSOP_SETPROP would do.
         */
        ptrdiff_t offset = bce->next() - bce->base();
        if (!EmitNewInit(cx, bce, JSProto_Object, pn, sharpnum))
            return JS_FALSE;

        /*
         * Try to construct the shape of the object as we go, so we can emit a
         * JSOP_NEWOBJECT with the final shape instead.
         */
        JSObject *obj = NULL;
        if (!bce->hasSharps() && bce->compileAndGo()) {
            gc::AllocKind kind = GuessObjectGCKind(pn->pn_count, false);
            obj = NewBuiltinClassInstance(cx, &ObjectClass, kind);
            if (!obj)
                return JS_FALSE;
        }

        uintN methodInits = 0, slowMethodInits = 0;
        for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            /* Emit an index for t[2] for later consumption by JSOP_INITELEM. */
            pn3 = pn2->pn_left;
            if (pn3->isKind(PNK_NUMBER)) {
                if (!EmitNumberOp(cx, pn3->pn_dval, bce))
                    return JS_FALSE;
            }

            /* Emit code for the property initializer. */
            if (!EmitTree(cx, bce, pn2->pn_right))
                return JS_FALSE;

            op = pn2->getOp();
            if (op == JSOP_GETTER || op == JSOP_SETTER) {
                obj = NULL;
                if (Emit1(cx, bce, op) < 0)
                    return JS_FALSE;
            }

            /* Annotate JSOP_INITELEM so we decompile 2:c and not just c. */
            if (pn3->isKind(PNK_NUMBER)) {
                obj = NULL;
                if (NewSrcNote(cx, bce, SRC_INITPROP) < 0)
                    return JS_FALSE;
                if (Emit1(cx, bce, JSOP_INITELEM) < 0)
                    return JS_FALSE;
            } else {
                JS_ASSERT(pn3->isKind(PNK_NAME) || pn3->isKind(PNK_STRING));
                jsatomid index;
                if (!bce->makeAtomIndex(pn3->pn_atom, &index))
                    return JS_FALSE;

                /* Check whether we can optimize to JSOP_INITMETHOD. */
                ParseNode *init = pn2->pn_right;
                bool lambda = init->isOp(JSOP_LAMBDA);
                if (lambda)
                    ++methodInits;
                if (op == JSOP_INITPROP && lambda && init->pn_funbox->joinable()) {
                    obj = NULL;
                    op = JSOP_INITMETHOD;
                    pn2->setOp(op);
                } else {
                    /*
                     * Disable NEWOBJECT on initializers that set __proto__, which has
                     * a non-standard setter on objects.
                     */
                    if (pn3->pn_atom == cx->runtime->atomState.protoAtom)
                        obj = NULL;
                    op = JSOP_INITPROP;
                    if (lambda)
                        ++slowMethodInits;
                }

                if (obj) {
                    JS_ASSERT(!obj->inDictionaryMode());
                    if (!DefineNativeProperty(cx, obj, ATOM_TO_JSID(pn3->pn_atom),
                                              UndefinedValue(), NULL, NULL,
                                              JSPROP_ENUMERATE, 0, 0)) {
                        return false;
                    }
                    if (obj->inDictionaryMode())
                        obj = NULL;
                }

                EMIT_INDEX_OP(op, index);
            }
        }

        if (bce->funbox && bce->funbox->shouldUnbrand(methodInits, slowMethodInits)) {
            obj = NULL;
            if (Emit1(cx, bce, JSOP_UNBRAND) < 0)
                return JS_FALSE;
        }
        if (!EmitEndInit(cx, bce, pn->pn_count))
            return JS_FALSE;

        if (obj) {
            /*
             * The object survived and has a predictable shape.  Update the original bytecode,
             * as long as we can do so without using a big index prefix/suffix.
             */
            ObjectBox *objbox = bce->parser->newObjectBox(obj);
            if (!objbox)
                return JS_FALSE;
            unsigned index = bce->objectList.index(objbox);
            if (FitsWithoutBigIndex(index))
                EMIT_UINT16_IN_PLACE(offset, JSOP_NEWOBJECT, uint16(index));
        }

        break;
      }

#if JS_HAS_SHARP_VARS
      case PNK_DEFSHARP:
        JS_ASSERT(bce->hasSharps());
        sharpnum = pn->pn_num;
        pn = pn->pn_kid;
        if (pn->isKind(PNK_RB))
            goto do_emit_array;
# if JS_HAS_GENERATORS
        if (pn->isKind(PNK_ARRAYCOMP))
            goto do_emit_array;
# endif
        if (pn->isKind(PNK_RC))
            goto do_emit_object;

        if (!EmitTree(cx, bce, pn))
            return JS_FALSE;
        EMIT_UINT16PAIR_IMM_OP(JSOP_DEFSHARP, bce->sharpSlotBase, (jsatomid) sharpnum);
        break;

      case PNK_USESHARP:
        JS_ASSERT(bce->hasSharps());
        EMIT_UINT16PAIR_IMM_OP(JSOP_USESHARP, bce->sharpSlotBase, (jsatomid) pn->pn_num);
        break;
#endif /* JS_HAS_SHARP_VARS */

      case PNK_NAME:
        /*
         * Cope with a left-over function definition that was replaced by a use
         * of a later function definition of the same name. See FunctionDef and
         * MakeDefIntoUse in Parser.cpp.
         */
        if (pn->isOp(JSOP_NOP))
            break;
        if (!EmitNameOp(cx, bce, pn, JS_FALSE))
            return JS_FALSE;
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_XMLATTR:
      case PNK_XMLSPACE:
      case PNK_XMLTEXT:
      case PNK_XMLCDATA:
      case PNK_XMLCOMMENT:
        JS_ASSERT(!bce->inStrictMode());
        /* FALL THROUGH */
#endif
      case PNK_STRING:
        ok = EmitAtomOp(cx, pn, pn->getOp(), bce);
        break;

      case PNK_NUMBER:
        ok = EmitNumberOp(cx, pn->pn_dval, bce);
        break;

      case PNK_REGEXP:
        JS_ASSERT(pn->isOp(JSOP_REGEXP));
        ok = EmitIndexOp(cx, JSOP_REGEXP, bce->regexpList.index(pn->pn_objbox), bce);
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_ANYNAME:
#endif
      case PNK_TRUE:
      case PNK_FALSE:
      case PNK_THIS:
      case PNK_NULL:
        if (Emit1(cx, bce, pn->getOp()) < 0)
            return JS_FALSE;
        break;

      case PNK_DEBUGGER:
        if (Emit1(cx, bce, JSOP_DEBUGGER) < 0)
            return JS_FALSE;
        break;

#if JS_HAS_XML_SUPPORT
      case PNK_XMLELEM:
      case PNK_XMLLIST:
        JS_ASSERT(!bce->inStrictMode());
        JS_ASSERT(pn->isKind(PNK_XMLLIST) || pn->pn_count != 0);

        switch (pn->pn_head ? pn->pn_head->getKind() : PNK_XMLLIST) {
          case PNK_XMLETAGO:
            JS_ASSERT(0);
            /* FALL THROUGH */
          case PNK_XMLPTAGC:
          case PNK_XMLSTAGO:
            break;
          default:
            if (Emit1(cx, bce, JSOP_STARTXML) < 0)
                return JS_FALSE;
        }

        for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
            if (pn2->isKind(PNK_XMLCURLYEXPR) && Emit1(cx, bce, JSOP_STARTXMLEXPR) < 0)
                return JS_FALSE;
            if (!EmitTree(cx, bce, pn2))
                return JS_FALSE;
            if (pn2 != pn->pn_head && Emit1(cx, bce, JSOP_ADD) < 0)
                return JS_FALSE;
        }

        if (pn->pn_xflags & PNX_XMLROOT) {
            if (pn->pn_count == 0) {
                JS_ASSERT(pn->isKind(PNK_XMLLIST));
                atom = cx->runtime->atomState.emptyAtom;
                jsatomid index;
                if (!bce->makeAtomIndex(atom, &index))
                    return JS_FALSE;
                EMIT_INDEX_OP(JSOP_STRING, index);
            }
            if (Emit1(cx, bce, pn->getOp()) < 0)
                return JS_FALSE;
        }
#ifdef DEBUG
        else
            JS_ASSERT(pn->pn_count != 0);
#endif
        break;

      case PNK_XMLPTAGC:
      case PNK_XMLSTAGO:
      case PNK_XMLETAGO:
        if (!EmitXMLTag(cx, bce, pn))
            return false;
        break;

      case PNK_XMLNAME:
        JS_ASSERT(!bce->inStrictMode());

        if (pn->isArity(PN_LIST)) {
            JS_ASSERT(pn->pn_count != 0);
            for (pn2 = pn->pn_head; pn2; pn2 = pn2->pn_next) {
                if (pn2->isKind(PNK_XMLCURLYEXPR) && Emit1(cx, bce, JSOP_STARTXMLEXPR) < 0)
                    return JS_FALSE;
                if (!EmitTree(cx, bce, pn2))
                    return JS_FALSE;
                if (pn2 != pn->pn_head && Emit1(cx, bce, JSOP_ADD) < 0)
                    return JS_FALSE;
            }
        } else {
            JS_ASSERT(pn->isArity(PN_NULLARY));
            ok = pn->isOp(JSOP_OBJECT)
                 ? EmitObjectOp(cx, pn->pn_objbox, pn->getOp(), bce)
                 : EmitAtomOp(cx, pn, pn->getOp(), bce);
        }
        break;

      case PNK_XMLPI:
        if (!EmitXMLProcessingInstruction(cx, bce, pn))
            return false;
        break;
#endif /* JS_HAS_XML_SUPPORT */

      default:
        JS_ASSERT(0);
    }

    /* bce->emitLevel == 1 means we're last on the stack, so finish up. */
    if (ok && bce->emitLevel == 1) {
        if (bce->spanDeps)
            ok = OptimizeSpanDeps(cx, bce);
        if (!UpdateLineNumberNotes(cx, bce, pn->pn_pos.end.lineno))
            return JS_FALSE;
    }

    return ok;
}

static intN
AllocSrcNote(JSContext *cx, BytecodeEmitter *bce)
{
    jssrcnote *notes = bce->notes();
    jssrcnote *newnotes;
    uintN index = bce->noteCount();
    uintN max = bce->noteLimit();

    if (index == max) {
        size_t newlength;
        if (!notes) {
            JS_ASSERT(!index && !max);
            newlength = SRCNOTE_CHUNK_LENGTH;
            newnotes = (jssrcnote *) cx->malloc_(SRCNOTE_SIZE(newlength));
        } else {
            JS_ASSERT(index <= max);
            newlength = max * 2;
            newnotes = (jssrcnote *) cx->realloc_(notes, SRCNOTE_SIZE(newlength));
        }
        if (!newnotes) {
            js_ReportOutOfMemory(cx);
            return -1;
        }
        bce->current->notes = newnotes;
        bce->current->noteLimit = newlength;
    }

    bce->current->noteCount = index + 1;
    return (intN)index;
}

intN
frontend::NewSrcNote(JSContext *cx, BytecodeEmitter *bce, SrcNoteType type)
{
    intN index, n;
    jssrcnote *sn;
    ptrdiff_t offset, delta, xdelta;

    /*
     * Claim a note slot in bce->notes() by growing it if necessary and then
     * incrementing bce->noteCount().
     */
    index = AllocSrcNote(cx, bce);
    if (index < 0)
        return -1;
    sn = &bce->notes()[index];

    /*
     * Compute delta from the last annotated bytecode's offset.  If it's too
     * big to fit in sn, allocate one or more xdelta notes and reset sn.
     */
    offset = bce->offset();
    delta = offset - bce->lastNoteOffset();
    bce->current->lastNoteOffset = offset;
    if (delta >= SN_DELTA_LIMIT) {
        do {
            xdelta = JS_MIN(delta, SN_XDELTA_MASK);
            SN_MAKE_XDELTA(sn, xdelta);
            delta -= xdelta;
            index = AllocSrcNote(cx, bce);
            if (index < 0)
                return -1;
            sn = &bce->notes()[index];
        } while (delta >= SN_DELTA_LIMIT);
    }

    /*
     * Initialize type and delta, then allocate the minimum number of notes
     * needed for type's arity.  Usually, we won't need more, but if an offset
     * does take two bytes, SetSrcNoteOffset will grow bce->notes().
     */
    SN_MAKE_NOTE(sn, type, delta);
    for (n = (intN)js_SrcNoteSpec[type].arity; n > 0; n--) {
        if (NewSrcNote(cx, bce, SRC_NULL) < 0)
            return -1;
    }
    return index;
}

intN
frontend::NewSrcNote2(JSContext *cx, BytecodeEmitter *bce, SrcNoteType type, ptrdiff_t offset)
{
    intN index;

    index = NewSrcNote(cx, bce, type);
    if (index >= 0) {
        if (!SetSrcNoteOffset(cx, bce, index, 0, offset))
            return -1;
    }
    return index;
}

intN
frontend::NewSrcNote3(JSContext *cx, BytecodeEmitter *bce, SrcNoteType type, ptrdiff_t offset1,
            ptrdiff_t offset2)
{
    intN index;

    index = NewSrcNote(cx, bce, type);
    if (index >= 0) {
        if (!SetSrcNoteOffset(cx, bce, index, 0, offset1))
            return -1;
        if (!SetSrcNoteOffset(cx, bce, index, 1, offset2))
            return -1;
    }
    return index;
}

static JSBool
GrowSrcNotes(JSContext *cx, BytecodeEmitter *bce)
{
    size_t newlength = bce->noteLimit() * 2;
    jssrcnote *newnotes = (jssrcnote *) cx->realloc_(bce->notes(), newlength);
    if (!newnotes) {
        js_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    bce->current->notes = newnotes;
    bce->current->noteLimit = newlength;
    return JS_TRUE;
}

jssrcnote *
frontend::AddToSrcNoteDelta(JSContext *cx, BytecodeEmitter *bce, jssrcnote *sn, ptrdiff_t delta)
{
    ptrdiff_t base, limit, newdelta, diff;
    intN index;

    /*
     * Called only from OptimizeSpanDeps and FinishTakingSrcNotes to add to
     * main script note deltas, and only by a small positive amount.
     */
    JS_ASSERT(bce->current == &bce->main);
    JS_ASSERT((unsigned) delta < (unsigned) SN_XDELTA_LIMIT);

    base = SN_DELTA(sn);
    limit = SN_IS_XDELTA(sn) ? SN_XDELTA_LIMIT : SN_DELTA_LIMIT;
    newdelta = base + delta;
    if (newdelta < limit) {
        SN_SET_DELTA(sn, newdelta);
    } else {
        index = sn - bce->main.notes;
        if (bce->main.noteCount == bce->main.noteLimit) {
            if (!GrowSrcNotes(cx, bce))
                return NULL;
            sn = bce->main.notes + index;
        }
        diff = bce->main.noteCount - index;
        bce->main.noteCount++;
        memmove(sn + 1, sn, SRCNOTE_SIZE(diff));
        SN_MAKE_XDELTA(sn, delta);
        sn++;
    }
    return sn;
}

static JSBool
SetSrcNoteOffset(JSContext *cx, BytecodeEmitter *bce, uintN index, uintN which, ptrdiff_t offset)
{
    jssrcnote *sn;
    ptrdiff_t diff;

    if ((jsuword)offset >= (jsuword)((ptrdiff_t)SN_3BYTE_OFFSET_FLAG << 16)) {
        ReportStatementTooLarge(cx, bce);
        return JS_FALSE;
    }

    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    sn = &bce->notes()[index];
    JS_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    JS_ASSERT((intN) which < js_SrcNoteSpec[SN_TYPE(sn)].arity);
    for (sn++; which; sn++, which--) {
        if (*sn & SN_3BYTE_OFFSET_FLAG)
            sn += 2;
    }

    /* See if the new offset requires three bytes. */
    if (offset > (ptrdiff_t)SN_3BYTE_OFFSET_MASK) {
        /* Maybe this offset was already set to a three-byte value. */
        if (!(*sn & SN_3BYTE_OFFSET_FLAG)) {
            /* Losing, need to insert another two bytes for this offset. */
            index = sn - bce->notes();

            /*
             * Test to see if the source note array must grow to accommodate
             * either the first or second byte of additional storage required
             * by this 3-byte offset.
             */
            if (bce->noteCount() + 1 >= bce->noteLimit()) {
                if (!GrowSrcNotes(cx, bce))
                    return JS_FALSE;
                sn = bce->notes() + index;
            }
            bce->current->noteCount += 2;

            diff = bce->noteCount() - (index + 3);
            JS_ASSERT(diff >= 0);
            if (diff > 0)
                memmove(sn + 3, sn + 1, SRCNOTE_SIZE(diff));
        }
        *sn++ = (jssrcnote)(SN_3BYTE_OFFSET_FLAG | (offset >> 16));
        *sn++ = (jssrcnote)(offset >> 8);
    }
    *sn = (jssrcnote)offset;
    return JS_TRUE;
}

#ifdef DEBUG_notme
#define DEBUG_srcnotesize
#endif

#ifdef DEBUG_srcnotesize
#define NBINS 10
static uint32 hist[NBINS];

static void
DumpSrcNoteSizeHist()
{
    static FILE *fp;
    int i, n;

    if (!fp) {
        fp = fopen("/tmp/srcnotes.hist", "w");
        if (!fp)
            return;
        setvbuf(fp, NULL, _IONBF, 0);
    }
    fprintf(fp, "SrcNote size histogram:\n");
    for (i = 0; i < NBINS; i++) {
        fprintf(fp, "%4u %4u ", JS_BIT(i), hist[i]);
        for (n = (int) JS_HOWMANY(hist[i], 10); n > 0; --n)
            fputc('*', fp);
        fputc('\n', fp);
    }
    fputc('\n', fp);
}
#endif

/*
 * Fill in the storage at notes with prolog and main srcnotes; the space at
 * notes was allocated using the BytecodeEmitter::countFinalSourceNotes()
 * method from BytecodeEmitter.h. SO DON'T CHANGE THIS FUNCTION WITHOUT AT
 * LEAST CHECKING WHETHER BytecodeEmitter::countFinalSourceNotes() NEEDS
 * CORRESPONDING CHANGES!
 */
JSBool
frontend::FinishTakingSrcNotes(JSContext *cx, BytecodeEmitter *bce, jssrcnote *notes)
{
    uintN prologCount, mainCount, totalCount;
    ptrdiff_t offset, delta;
    jssrcnote *sn;

    JS_ASSERT(bce->current == &bce->main);

    prologCount = bce->prolog.noteCount;
    if (prologCount && bce->prolog.currentLine != bce->firstLine) {
        bce->switchToProlog();
        if (NewSrcNote2(cx, bce, SRC_SETLINE, (ptrdiff_t)bce->firstLine) < 0)
            return false;
        prologCount = bce->prolog.noteCount;
        bce->switchToMain();
    } else {
        /*
         * Either no prolog srcnotes, or no line number change over prolog.
         * We don't need a SRC_SETLINE, but we may need to adjust the offset
         * of the first main note, by adding to its delta and possibly even
         * prepending SRC_XDELTA notes to it to account for prolog bytecodes
         * that came at and after the last annotated bytecode.
         */
        offset = bce->prologOffset() - bce->prolog.lastNoteOffset;
        JS_ASSERT(offset >= 0);
        if (offset > 0 && bce->main.noteCount != 0) {
            /* NB: Use as much of the first main note's delta as we can. */
            sn = bce->main.notes;
            delta = SN_IS_XDELTA(sn)
                    ? SN_XDELTA_MASK - (*sn & SN_XDELTA_MASK)
                    : SN_DELTA_MASK - (*sn & SN_DELTA_MASK);
            if (offset < delta)
                delta = offset;
            for (;;) {
                if (!AddToSrcNoteDelta(cx, bce, sn, delta))
                    return false;
                offset -= delta;
                if (offset == 0)
                    break;
                delta = JS_MIN(offset, SN_XDELTA_MASK);
                sn = bce->main.notes;
            }
        }
    }

    mainCount = bce->main.noteCount;
    totalCount = prologCount + mainCount;
    if (prologCount)
        memcpy(notes, bce->prolog.notes, SRCNOTE_SIZE(prologCount));
    memcpy(notes + prologCount, bce->main.notes, SRCNOTE_SIZE(mainCount));
    SN_MAKE_TERMINATOR(&notes[totalCount]);

    return true;
}

static JSBool
NewTryNote(JSContext *cx, BytecodeEmitter *bce, JSTryNoteKind kind, uintN stackDepth, size_t start,
           size_t end)
{
    JS_ASSERT((uintN)(uint16)stackDepth == stackDepth);
    JS_ASSERT(start <= end);
    JS_ASSERT((size_t)(uint32)start == start);
    JS_ASSERT((size_t)(uint32)end == end);

    TryNode *tryNode = cx->tempLifoAlloc().new_<TryNode>();
    if (!tryNode) {
        js_ReportOutOfMemory(cx);
        return JS_FALSE;
    }

    tryNode->note.kind = kind;
    tryNode->note.stackDepth = (uint16)stackDepth;
    tryNode->note.start = (uint32)start;
    tryNode->note.length = (uint32)(end - start);
    tryNode->prev = bce->lastTryNode;
    bce->lastTryNode = tryNode;
    bce->ntrynotes++;
    return JS_TRUE;
}

void
frontend::FinishTakingTryNotes(BytecodeEmitter *bce, JSTryNoteArray *array)
{
    TryNode *tryNode;
    JSTryNote *tn;

    JS_ASSERT(array->length > 0 && array->length == bce->ntrynotes);
    tn = array->vector + array->length;
    tryNode = bce->lastTryNode;
    do {
        *--tn = tryNode->note;
    } while ((tryNode = tryNode->prev) != NULL);
    JS_ASSERT(tn == array->vector);
}

/*
 * Find the index of the given object for code generator.
 *
 * Since the emitter refers to each parsed object only once, for the index we
 * use the number of already indexes objects. We also add the object to a list
 * to convert the list to a fixed-size array when we complete code generation,
 * see js::CGObjectList::finish below.
 *
 * Most of the objects go to BytecodeEmitter::objectList but for regexp we use
 * a separated BytecodeEmitter::regexpList. In this way the emitted index can
 * be directly used to store and fetch a reference to a cloned RegExp object
 * that shares the same JSRegExp private data created for the object literal in
 * objbox. We need a cloned object to hold lastIndex and other direct
 * properties that should not be shared among threads sharing a precompiled
 * function or script.
 *
 * If the code being compiled is function code, allocate a reserved slot in
 * the cloned function object that shares its precompiled script with other
 * cloned function objects and with the compiler-created clone-parent. There
 * are nregexps = script->regexps()->length such reserved slots in each
 * function object cloned from fun->object. NB: during compilation, a funobj
 * slots element must never be allocated, because JSObject::allocSlot could
 * hand out one of the slots that should be given to a regexp clone.
 *
 * If the code being compiled is global code, the cloned regexp are stored in
 * fp->vars slot and to protect regexp slots from GC we set fp->nvars to
 * nregexps.
 *
 * The slots initially contain undefined or null. We populate them lazily when
 * JSOP_REGEXP is executed for the first time.
 *
 * Why clone regexp objects?  ECMA specifies that when a regular expression
 * literal is scanned, a RegExp object is created.  In the spec, compilation
 * and execution happen indivisibly, but in this implementation and many of
 * its embeddings, code is precompiled early and re-executed in multiple
 * threads, or using multiple global objects, or both, for efficiency.
 *
 * In such cases, naively following ECMA leads to wrongful sharing of RegExp
 * objects, which makes for collisions on the lastIndex property (especially
 * for global regexps) and on any ad-hoc properties.  Also, __proto__ refers to
 * the pre-compilation prototype, a pigeon-hole problem for instanceof tests.
 */
uintN
CGObjectList::index(ObjectBox *objbox)
{
    JS_ASSERT(!objbox->emitLink);
    objbox->emitLink = lastbox;
    lastbox = objbox;
    objbox->index = length++;
    return objbox->index;
}

void
CGObjectList::finish(JSObjectArray *array)
{
    JS_ASSERT(length <= INDEX_LIMIT);
    JS_ASSERT(length == array->length);

    js::HeapPtrObject *cursor = array->vector + array->length;
    ObjectBox *objbox = lastbox;
    do {
        --cursor;
        JS_ASSERT(!*cursor);
        *cursor = objbox->object;
    } while ((objbox = objbox->emitLink) != NULL);
    JS_ASSERT(cursor == array->vector);
}

void
GCConstList::finish(JSConstArray *array)
{
    JS_ASSERT(array->length == list.length());
    Value *src = list.begin(), *srcend = list.end();
    HeapValue *dst = array->vector;
    for (; src != srcend; ++src, ++dst)
        *dst = *src;
}

/*
 * We should try to get rid of offsetBias (always 0 or 1, where 1 is
 * JSOP_{NOP,POP}_LENGTH), which is used only by SRC_FOR and SRC_DECL.
 */
JS_FRIEND_DATA(JSSrcNoteSpec) js_SrcNoteSpec[] = {
    {"null",            0,      0,      0},
    {"if",              0,      0,      0},
    {"if-else",         2,      0,      1},
    {"for",             3,      1,      1},
    {"while",           1,      0,      1},
    {"continue",        0,      0,      0},
    {"decl",            1,      1,      1},
    {"pcdelta",         1,      0,      1},
    {"assignop",        0,      0,      0},
    {"cond",            1,      0,      1},
    {"brace",           1,      0,      1},
    {"hidden",          0,      0,      0},
    {"pcbase",          1,      0,     -1},
    {"label",           1,      0,      0},
    {"labelbrace",      1,      0,      0},
    {"endbrace",        0,      0,      0},
    {"break2label",     1,      0,      0},
    {"cont2label",      1,      0,      0},
    {"switch",          2,      0,      1},
    {"funcdef",         1,      0,      0},
    {"catch",           1,      0,      1},
    {"extended",       -1,      0,      0},
    {"newline",         0,      0,      0},
    {"setline",         1,      0,      0},
    {"xdelta",          0,      0,      0},
};

JS_FRIEND_API(uintN)
js_SrcNoteLength(jssrcnote *sn)
{
    uintN arity;
    jssrcnote *base;

    arity = (intN)js_SrcNoteSpec[SN_TYPE(sn)].arity;
    for (base = sn++; arity; sn++, arity--) {
        if (*sn & SN_3BYTE_OFFSET_FLAG)
            sn += 2;
    }
    return sn - base;
}

JS_FRIEND_API(ptrdiff_t)
js_GetSrcNoteOffset(jssrcnote *sn, uintN which)
{
    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    JS_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    JS_ASSERT((intN) which < js_SrcNoteSpec[SN_TYPE(sn)].arity);
    for (sn++; which; sn++, which--) {
        if (*sn & SN_3BYTE_OFFSET_FLAG)
            sn += 2;
    }
    if (*sn & SN_3BYTE_OFFSET_FLAG) {
        return (ptrdiff_t)(((uint32)(sn[0] & SN_3BYTE_OFFSET_MASK) << 16)
                           | (sn[1] << 8)
                           | sn[2]);
    }
    return (ptrdiff_t)*sn;
}
