/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BytecodeEmitter_h__
#define BytecodeEmitter_h__

/*
 * JS bytecode generation.
 */
#include "jstypes.h"
#include "jsatom.h"
#include "jsopcode.h"
#include "jsscript.h"
#include "jsprvtd.h"
#include "jspubtd.h"

#include "frontend/BytecodeCompiler.h"
#include "frontend/Parser.h"
#include "frontend/ParseMaps.h"
#include "frontend/SharedContext.h"

#include "vm/ScopeObject.h"

namespace js {
namespace frontend {

struct CGTryNoteList {
    Vector<JSTryNote> list;
    CGTryNoteList(JSContext *cx) : list(cx) {}

    bool append(JSTryNoteKind kind, unsigned stackDepth, size_t start, size_t end);
    size_t length() const { return list.length(); }
    void finish(TryNoteArray *array);
};

struct CGObjectList {
    uint32_t            length;     /* number of emitted so far objects */
    ObjectBox           *lastbox;   /* last emitted object */

    CGObjectList() : length(0), lastbox(NULL) {}

    unsigned add(ObjectBox *objbox);
    unsigned indexOf(JSObject *obj);
    void finish(ObjectArray *array);
};

class CGConstList {
    Vector<Value> list;
  public:
    CGConstList(JSContext *cx) : list(cx) {}
    bool append(Value v) { JS_ASSERT_IF(v.isString(), v.toString()->isAtom()); return list.append(v); }
    size_t length() const { return list.length(); }
    void finish(ConstArray *array);
};

struct StmtInfoBCE;

// Use zero inline elements because these go on the stack and affect how many
// nested functions are possible.
typedef Vector<jsbytecode, 0> BytecodeVector;
typedef Vector<jssrcnote, 0> SrcNotesVector;

struct BytecodeEmitter
{
    typedef StmtInfoBCE StmtInfo;

    SharedContext   *const sc;      /* context shared between parsing and bytecode generation */

    BytecodeEmitter *const parent;  /* enclosing function or global context */

    Rooted<JSScript*> script;       /* the JSScript we're ultimately producing */

    struct EmitSection {
        BytecodeVector code;        /* bytecode */
        SrcNotesVector notes;       /* source notes, see below */
        ptrdiff_t   lastNoteOffset; /* code offset for last source note */
        uint32_t    currentLine;    /* line number for tree-based srcnote gen */
        uint32_t    lastColumn;     /* zero-based column index on currentLine of
                                       last SRC_COLSPAN-annotated opcode */

        EmitSection(JSContext *cx, uint32_t lineNum)
          : code(cx), notes(cx), lastNoteOffset(0), currentLine(lineNum), lastColumn(0)
        {}
    };
    EmitSection prolog, main, *current;

    /* the parser */
    Parser<FullParseHandler> *const parser;

    HandleScript    evalCaller;     /* scripted caller info for eval and dbgapi */

    StmtInfoBCE     *topStmt;       /* top of statement info stack */
    StmtInfoBCE     *topScopeStmt;  /* top lexical scope statement */
    Rooted<StaticBlockObject *> blockChain;
                                    /* compile time block scope chain */

    OwnedAtomIndexMapPtr atomIndices; /* literals indexed for mapping */
    unsigned        firstLine;      /* first line, for JSScript::initFromEmitter */

    int             stackDepth;     /* current stack depth in script frame */
    unsigned        maxStackDepth;  /* maximum stack depth so far */

    CGTryNoteList   tryNoteList;    /* list of emitted try notes */

    unsigned        arrayCompDepth; /* stack depth of array in comprehension */

    unsigned        emitLevel;      /* js::frontend::EmitTree recursion level */

    CGConstList     constList;      /* constants to be included with the script */

    CGObjectList    objectList;     /* list of emitted objects */
    CGObjectList    regexpList;     /* list of emitted regexp that will be
                                       cloned during execution */

    uint16_t        typesetCount;   /* Number of JOF_TYPESET opcodes generated */

    bool            hasSingletons:1;    /* script contains singleton initializer JSOP_OBJECT */

    bool            emittingForInit:1;  /* true while emitting init expr of for; exclude 'in' */

    bool            emittingRunOnceLambda:1; /* true while emitting a lambda which is only
                                                expected to run once. */

    bool            insideEval:1;       /* True if compiling an eval-expression or a function
                                           nested inside an eval. */

    const bool      hasGlobalScope:1;   /* frontend::CompileScript's scope chain is the
                                           global object */

    const bool      selfHostingMode:1;  /* Emit JSOP_CALLINTRINSIC instead of JSOP_NAME
                                           and assert that JSOP_NAME and JSOP_*GNAME
                                           don't ever get emitted. See the comment for
                                           the field |selfHostingMode| in Parser.h for details. */

    /*
     * Note that BytecodeEmitters are magic: they own the arena "top-of-stack"
     * space above their tempMark points. This means that you cannot alloc from
     * tempLifoAlloc and save the pointer beyond the next BytecodeEmitter
     * destruction.
     */
    BytecodeEmitter(BytecodeEmitter *parent, Parser<FullParseHandler> *parser, SharedContext *sc,
                    HandleScript script, bool insideEval, HandleScript evalCaller,
                    bool hasGlobalScope, uint32_t lineNum, bool selfHostingMode = false);
    bool init();

    bool isAliasedName(ParseNode *pn);

    JS_ALWAYS_INLINE
    bool makeAtomIndex(JSAtom *atom, jsatomid *indexp) {
        AtomIndexAddPtr p = atomIndices->lookupForAdd(atom);
        if (p) {
            *indexp = p.value();
            return true;
        }

        jsatomid index = atomIndices->count();
        if (!atomIndices->add(p, atom, index))
            return false;

        *indexp = index;
        return true;
    }

    bool isInLoop();
    bool checkSingletonContext();

    bool needsImplicitThis();

    void tellDebuggerAboutCompiledScript(JSContext *cx);

    TokenStream *tokenStream() { return &parser->tokenStream; }

    BytecodeVector &code() const { return current->code; }
    jsbytecode *code(ptrdiff_t offset) const { return current->code.begin() + offset; }
    ptrdiff_t offset() const { return current->code.end() - current->code.begin(); }
    ptrdiff_t prologOffset() const { return prolog.code.end() - prolog.code.begin(); }
    void switchToMain() { current = &main; }
    void switchToProlog() { current = &prolog; }

    SrcNotesVector &notes() const { return current->notes; }
    ptrdiff_t lastNoteOffset() const { return current->lastNoteOffset; }
    unsigned currentLine() const { return current->currentLine; }
    unsigned lastColumn() const { return current->lastColumn; }

    inline ptrdiff_t countFinalSourceNotes();

    bool reportError(ParseNode *pn, unsigned errorNumber, ...);
    bool reportStrictWarning(ParseNode *pn, unsigned errorNumber, ...);
    bool reportStrictModeError(ParseNode *pn, unsigned errorNumber, ...);
};

/*
 * Emit one bytecode.
 */
ptrdiff_t
Emit1(JSContext *cx, BytecodeEmitter *bce, JSOp op);

/*
 * Emit two bytecodes, an opcode (op) with a byte of immediate operand (op1).
 */
ptrdiff_t
Emit2(JSContext *cx, BytecodeEmitter *bce, JSOp op, jsbytecode op1);

/*
 * Emit three bytecodes, an opcode with two bytes of immediate operands.
 */
ptrdiff_t
Emit3(JSContext *cx, BytecodeEmitter *bce, JSOp op, jsbytecode op1, jsbytecode op2);

/*
 * Emit (1 + extra) bytecodes, for N bytes of op and its immediate operand.
 */
ptrdiff_t
EmitN(JSContext *cx, BytecodeEmitter *bce, JSOp op, size_t extra);

/*
 * Emit code into bce for the tree rooted at pn.
 */
bool
EmitTree(JSContext *cx, BytecodeEmitter *bce, ParseNode *pn);

/*
 * Emit function code using bce for the tree rooted at body.
 */
bool
EmitFunctionScript(JSContext *cx, BytecodeEmitter *bce, ParseNode *body);

} /* namespace frontend */

/*
 * Source notes generated along with bytecode for decompiling and debugging.
 * A source note is a uint8_t with 5 bits of type and 3 of offset from the pc
 * of the previous note. If 3 bits of offset aren't enough, extended delta
 * notes (SRC_XDELTA) consisting of 2 set high order bits followed by 6 offset
 * bits are emitted before the next note. Some notes have operand offsets
 * encoded immediately after them, in note bytes or byte-triples.
 *
 *                 Source Note               Extended Delta
 *              +7-6-5-4-3+2-1-0+           +7-6-5+4-3-2-1-0+
 *              |note-type|delta|           |1 1| ext-delta |
 *              +---------+-----+           +---+-----------+
 *
 * At most one "gettable" note (i.e., a note of type other than SRC_NEWLINE,
 * SRC_COLSPAN, SRC_SETLINE, and SRC_XDELTA) applies to a given bytecode.
 *
 * NB: the js_SrcNoteSpec array in BytecodeEmitter.cpp is indexed by this
 * enum, so its initializers need to match the order here.
 *
 * Don't forget to update XDR_BYTECODE_VERSION in vm/Xdr.h for all such
 * incompatible source note or other bytecode changes.
 */
enum SrcNoteType {
    SRC_NULL        = 0,        /* terminates a note vector */

    SRC_IF          = 1,        /* JSOP_IFEQ bytecode is from an if-then */
    SRC_IF_ELSE     = 2,        /* JSOP_IFEQ bytecode is from an if-then-else */
    SRC_COND        = 3,        /* JSOP_IFEQ is from conditional ?: operator */

    SRC_FOR         = 4,        /* JSOP_NOP or JSOP_POP in for(;;) loop head */

    SRC_WHILE       = 5,        /* JSOP_GOTO to for or while loop condition
                                   from before loop, else JSOP_NOP at top of
                                   do-while loop */
    SRC_FOR_IN      = 6,        /* JSOP_GOTO to for-in loop condition from
                                   before loop */
    SRC_CONTINUE    = 7,        /* JSOP_GOTO is a continue */
    SRC_BREAK       = 8,        /* JSOP_GOTO is a break */
    SRC_BREAK2LABEL = 9,        /* JSOP_GOTO for 'break label' */
    SRC_SWITCHBREAK = 10,       /* JSOP_GOTO is a break in a switch */

    SRC_TABLESWITCH = 11,       /* JSOP_TABLESWITCH, offset points to end of
                                   switch */
    SRC_CONDSWITCH  = 12,       /* JSOP_CONDSWITCH, 1st offset points to end of
                                   switch, 2nd points to first JSOP_CASE */

    SRC_NEXTCASE    = 13,       /* distance forward from one CASE in a
                                   CONDSWITCH to the next */

    SRC_ASSIGNOP    = 14,       /* += or another assign-op follows */

    SRC_HIDDEN      = 15,       /* opcode shouldn't be decompiled */

    SRC_CATCH       = 16,       /* catch block has guard */

    /* All notes below here are "gettable".  See SN_IS_GETTABLE below. */
    SRC_LAST_GETTABLE = SRC_CATCH,

    SRC_COLSPAN     = 17,       /* number of columns this opcode spans */
    SRC_NEWLINE     = 18,       /* bytecode follows a source newline */
    SRC_SETLINE     = 19,       /* a file-absolute source line number note */

    SRC_UNUSED20    = 20,
    SRC_UNUSED21    = 21,
    SRC_UNUSED22    = 22,
    SRC_UNUSED23    = 23,

    SRC_XDELTA      = 24        /* 24-31 are for extended delta notes */
};

#define SN_TYPE_BITS            5
#define SN_DELTA_BITS           3
#define SN_XDELTA_BITS          6
#define SN_TYPE_MASK            (JS_BITMASK(SN_TYPE_BITS) << SN_DELTA_BITS)
#define SN_DELTA_MASK           ((ptrdiff_t)JS_BITMASK(SN_DELTA_BITS))
#define SN_XDELTA_MASK          ((ptrdiff_t)JS_BITMASK(SN_XDELTA_BITS))

#define SN_MAKE_NOTE(sn,t,d)    (*(sn) = (jssrcnote)                          \
                                          (((t) << SN_DELTA_BITS)             \
                                           | ((d) & SN_DELTA_MASK)))
#define SN_MAKE_XDELTA(sn,d)    (*(sn) = (jssrcnote)                          \
                                          ((SRC_XDELTA << SN_DELTA_BITS)      \
                                           | ((d) & SN_XDELTA_MASK)))

#define SN_IS_XDELTA(sn)        ((*(sn) >> SN_DELTA_BITS) >= SRC_XDELTA)
#define SN_TYPE(sn)             ((js::SrcNoteType)(SN_IS_XDELTA(sn)           \
                                                   ? SRC_XDELTA               \
                                                   : *(sn) >> SN_DELTA_BITS))
#define SN_SET_TYPE(sn,type)    SN_MAKE_NOTE(sn, type, SN_DELTA(sn))
#define SN_IS_GETTABLE(sn)      (SN_TYPE(sn) <= SRC_LAST_GETTABLE)

#define SN_DELTA(sn)            ((ptrdiff_t)(SN_IS_XDELTA(sn)                 \
                                             ? *(sn) & SN_XDELTA_MASK         \
                                             : *(sn) & SN_DELTA_MASK))
#define SN_SET_DELTA(sn,delta)  (SN_IS_XDELTA(sn)                             \
                                 ? SN_MAKE_XDELTA(sn, delta)                  \
                                 : SN_MAKE_NOTE(sn, SN_TYPE(sn), delta))

#define SN_DELTA_LIMIT          ((ptrdiff_t)JS_BIT(SN_DELTA_BITS))
#define SN_XDELTA_LIMIT         ((ptrdiff_t)JS_BIT(SN_XDELTA_BITS))

/*
 * Offset fields follow certain notes and are frequency-encoded: an offset in
 * [0,0x7f] consumes one byte, an offset in [0x80,0x7fffff] takes three, and
 * the high bit of the first byte is set.
 */
#define SN_3BYTE_OFFSET_FLAG    0x80
#define SN_3BYTE_OFFSET_MASK    0x7f

/*
 * Negative SRC_COLSPAN offsets are rare, but can arise with for(;;) loops and
 * other constructs that generate code in non-source order. They can also arise
 * due to failure to update pn->pn_pos.end to be the last child's end -- such
 * failures are bugs to fix.
 *
 * Source note offsets in general must be non-negative and less than 0x800000,
 * per the above SN_3BYTE_* definitions. To encode negative colspans, we bias
 * them by the offset domain size and restrict non-negative colspans to less
 * than half this domain.
 */
#define SN_COLSPAN_DOMAIN       ptrdiff_t(SN_3BYTE_OFFSET_FLAG << 16)

#define SN_MAX_OFFSET ((size_t)((ptrdiff_t)SN_3BYTE_OFFSET_FLAG << 16) - 1)

#define SN_LENGTH(sn)           ((js_SrcNoteSpec[SN_TYPE(sn)].arity == 0) ? 1 \
                                 : js_SrcNoteLength(sn))
#define SN_NEXT(sn)             ((sn) + SN_LENGTH(sn))

/* A source note array is terminated by an all-zero element. */
#define SN_MAKE_TERMINATOR(sn)  (*(sn) = SRC_NULL)
#define SN_IS_TERMINATOR(sn)    (*(sn) == SRC_NULL)

namespace frontend {

/*
 * Append a new source note of the given type (and therefore size) to bce's
 * notes dynamic array, updating bce->noteCount. Return the new note's index
 * within the array pointed at by bce->current->notes. Return -1 if out of
 * memory.
 */
int
NewSrcNote(JSContext *cx, BytecodeEmitter *bce, SrcNoteType type);

int
NewSrcNote2(JSContext *cx, BytecodeEmitter *bce, SrcNoteType type, ptrdiff_t offset);

int
NewSrcNote3(JSContext *cx, BytecodeEmitter *bce, SrcNoteType type, ptrdiff_t offset1,
               ptrdiff_t offset2);

/* NB: this function can add at most one extra extended delta note. */
bool
AddToSrcNoteDelta(JSContext *cx, BytecodeEmitter *bce, jssrcnote *sn, ptrdiff_t delta);

bool
FinishTakingSrcNotes(JSContext *cx, BytecodeEmitter *bce, jssrcnote *notes);

/*
 * Finish taking source notes in cx's notePool, copying final notes to the new
 * stable store allocated by the caller and passed in via notes. Return false
 * on malloc failure, which means this function reported an error.
 *
 * Use this to compute the number of jssrcnotes to allocate and pass in via
 * notes. This method knows a lot about details of FinishTakingSrcNotes, so
 * DON'T CHANGE js::frontend::FinishTakingSrcNotes WITHOUT CHECKING WHETHER
 * THIS METHOD NEEDS CORRESPONDING CHANGES!
 */
inline ptrdiff_t
BytecodeEmitter::countFinalSourceNotes()
{
    ptrdiff_t diff = prologOffset() - prolog.lastNoteOffset;
    ptrdiff_t cnt = prolog.notes.length() + main.notes.length() + 1;
    if (prolog.notes.length() && prolog.currentLine != firstLine) {
        if (diff > SN_DELTA_MASK)
            cnt += JS_HOWMANY(diff - SN_DELTA_MASK, SN_XDELTA_MASK);
        cnt += 2 + ((firstLine > SN_3BYTE_OFFSET_MASK) << 1);
    } else if (diff > 0) {
        if (main.notes.length()) {
            jssrcnote *sn = main.notes.begin();
            diff -= SN_IS_XDELTA(sn)
                    ? SN_XDELTA_MASK - (*sn & SN_XDELTA_MASK)
                    : SN_DELTA_MASK - (*sn & SN_DELTA_MASK);
        }
        if (diff > 0)
            cnt += JS_HOWMANY(diff, SN_XDELTA_MASK);
    }
    return cnt;
}

} /* namespace frontend */
} /* namespace js */

struct JSSrcNoteSpec {
    const char      *name;      /* name for disassembly/debugging output */
    int8_t          arity;      /* number of offset operands */
};

extern JS_FRIEND_DATA(JSSrcNoteSpec)  js_SrcNoteSpec[];
extern JS_FRIEND_API(unsigned)         js_SrcNoteLength(jssrcnote *sn);

/*
 * Get and set the offset operand identified by which (0 for the first, etc.).
 */
extern JS_FRIEND_API(ptrdiff_t)
js_GetSrcNoteOffset(jssrcnote *sn, unsigned which);

#endif /* BytecodeEmitter_h__ */
