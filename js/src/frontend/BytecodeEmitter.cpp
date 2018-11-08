/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS bytecode generation.
 */

#include "frontend/BytecodeEmitter.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Maybe.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ReverseIterator.h"

#include <string.h>

#include "jsnum.h"
#include "jstypes.h"
#include "jsutil.h"

#include "ds/Nestable.h"
#include "frontend/BytecodeControlStructures.h"
#include "frontend/CallOrNewEmitter.h"
#include "frontend/CForEmitter.h"
#include "frontend/DoWhileEmitter.h"
#include "frontend/ElemOpEmitter.h"
#include "frontend/EmitterScope.h"
#include "frontend/ExpressionStatementEmitter.h"
#include "frontend/ForInEmitter.h"
#include "frontend/ForOfEmitter.h"
#include "frontend/ForOfLoopControl.h"
#include "frontend/IfEmitter.h"
#include "frontend/NameOpEmitter.h"
#include "frontend/ParseNode.h"
#include "frontend/Parser.h"
#include "frontend/PropOpEmitter.h"
#include "frontend/SwitchEmitter.h"
#include "frontend/TDZCheckCache.h"
#include "frontend/TryEmitter.h"
#include "frontend/WhileEmitter.h"
#include "js/CompileOptions.h"
#include "vm/BytecodeUtil.h"
#include "vm/Debugger.h"
#include "vm/GeneratorObject.h"
#include "vm/JSAtom.h"
#include "vm/JSContext.h"
#include "vm/JSFunction.h"
#include "vm/JSScript.h"
#include "vm/Stack.h"
#include "wasm/AsmJS.h"

#include "vm/JSObject-inl.h"

using namespace js;
using namespace js::frontend;

using mozilla::AssertedCast;
using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::NumberEqualsInt32;
using mozilla::NumberIsInt32;
using mozilla::PodCopy;
using mozilla::Some;
using mozilla::Unused;

static bool
ParseNodeRequiresSpecialLineNumberNotes(ParseNode* pn)
{
    // The few node types listed below are exceptions to the usual
    // location-source-note-emitting code in BytecodeEmitter::emitTree().
    // Single-line `while` loops and C-style `for` loops require careful
    // handling to avoid strange stepping behavior.
    // Functions usually shouldn't have location information (bug 1431202).

    ParseNodeKind kind = pn->getKind();
    return kind == ParseNodeKind::While ||
           kind == ParseNodeKind::For ||
           kind == ParseNodeKind::Function;
}

BytecodeEmitter::BytecodeEmitter(BytecodeEmitter* parent,
                                 SharedContext* sc, HandleScript script,
                                 Handle<LazyScript*> lazyScript,
                                 uint32_t lineNum, EmitterMode emitterMode)
  : sc(sc),
    cx(sc->context),
    parent(parent),
    script(cx, script),
    lazyScript(cx, lazyScript),
    prologue(cx, lineNum),
    main(cx, lineNum),
    current(&main),
    parser(nullptr),
    atomIndices(cx->frontendCollectionPool()),
    firstLine(lineNum),
    maxFixedSlots(0),
    maxStackDepth(0),
    stackDepth(0),
    emitLevel(0),
    bodyScopeIndex(UINT32_MAX),
    varEmitterScope(nullptr),
    innermostNestableControl(nullptr),
    innermostEmitterScope_(nullptr),
    innermostTDZCheckCache(nullptr),
#ifdef DEBUG
    unstableEmitterScope(false),
#endif
    numberList(cx),
    scopeList(cx),
    tryNoteList(cx),
    scopeNoteList(cx),
    resumeOffsetList(cx),
    numYields(0),
    typesetCount(0),
    hasSingletons(false),
    hasTryFinally(false),
    emittingRunOnceLambda(false),
    emitterMode(emitterMode),
    scriptStartOffsetSet(false),
    functionBodyEndPosSet(false)
{
    MOZ_ASSERT_IF(emitterMode == LazyFunction, lazyScript);
}

BytecodeEmitter::BytecodeEmitter(BytecodeEmitter* parent,
                                 BCEParserHandle* handle, SharedContext* sc,
                                 HandleScript script, Handle<LazyScript*> lazyScript,
                                 uint32_t lineNum, EmitterMode emitterMode)
    : BytecodeEmitter(parent, sc, script, lazyScript, lineNum, emitterMode)
{
    parser = handle;
}

BytecodeEmitter::BytecodeEmitter(BytecodeEmitter* parent,
                                 const EitherParser& parser, SharedContext* sc,
                                 HandleScript script, Handle<LazyScript*> lazyScript,
                                 uint32_t lineNum, EmitterMode emitterMode)
    : BytecodeEmitter(parent, sc, script, lazyScript, lineNum, emitterMode)
{
    ep_.emplace(parser);
    this->parser = ep_.ptr();
}

void
BytecodeEmitter::initFromBodyPosition(TokenPos bodyPosition)
{
    setScriptStartOffsetIfUnset(bodyPosition);
    setFunctionBodyEndPos(bodyPosition);
}

bool
BytecodeEmitter::init()
{
    return atomIndices.acquire(cx);
}

template <typename T>
T*
BytecodeEmitter::findInnermostNestableControl() const
{
    return NestableControl::findNearest<T>(innermostNestableControl);
}

template <typename T, typename Predicate /* (T*) -> bool */>
T*
BytecodeEmitter::findInnermostNestableControl(Predicate predicate) const
{
    return NestableControl::findNearest<T>(innermostNestableControl, predicate);
}

NameLocation
BytecodeEmitter::lookupName(JSAtom* name)
{
    return innermostEmitterScope()->lookup(this, name);
}

Maybe<NameLocation>
BytecodeEmitter::locationOfNameBoundInScope(JSAtom* name, EmitterScope* target)
{
    return innermostEmitterScope()->locationBoundInScope(name, target);
}

Maybe<NameLocation>
BytecodeEmitter::locationOfNameBoundInFunctionScope(JSAtom* name, EmitterScope* source)
{
    EmitterScope* funScope = source;
    while (!funScope->scope(this)->is<FunctionScope>()) {
        funScope = funScope->enclosingInFrame();
    }
    return source->locationBoundInScope(name, funScope);
}

bool
BytecodeEmitter::emitCheck(ptrdiff_t delta, ptrdiff_t* offset)
{
    *offset = code().length();

    if (!code().growBy(delta)) {
        ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

void
BytecodeEmitter::updateDepth(ptrdiff_t target)
{
    jsbytecode* pc = code(target);

    int nuses = StackUses(pc);
    int ndefs = StackDefs(pc);

    stackDepth -= nuses;
    MOZ_ASSERT(stackDepth >= 0);
    stackDepth += ndefs;

    if ((uint32_t)stackDepth > maxStackDepth) {
        maxStackDepth = stackDepth;
    }
}

#ifdef DEBUG
bool
BytecodeEmitter::checkStrictOrSloppy(JSOp op)
{
    if (IsCheckStrictOp(op) && !sc->strict()) {
        return false;
    }
    if (IsCheckSloppyOp(op) && sc->strict()) {
        return false;
    }
    return true;
}
#endif

bool
BytecodeEmitter::emit1(JSOp op)
{
    MOZ_ASSERT(checkStrictOrSloppy(op));

    ptrdiff_t offset;
    if (!emitCheck(1, &offset)) {
        return false;
    }

    jsbytecode* code = this->code(offset);
    code[0] = jsbytecode(op);
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::emit2(JSOp op, uint8_t op1)
{
    MOZ_ASSERT(checkStrictOrSloppy(op));

    ptrdiff_t offset;
    if (!emitCheck(2, &offset)) {
        return false;
    }

    jsbytecode* code = this->code(offset);
    code[0] = jsbytecode(op);
    code[1] = jsbytecode(op1);
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::emit3(JSOp op, jsbytecode op1, jsbytecode op2)
{
    MOZ_ASSERT(checkStrictOrSloppy(op));

    /* These should filter through emitVarOp. */
    MOZ_ASSERT(!IsArgOp(op));
    MOZ_ASSERT(!IsLocalOp(op));

    ptrdiff_t offset;
    if (!emitCheck(3, &offset)) {
        return false;
    }

    jsbytecode* code = this->code(offset);
    code[0] = jsbytecode(op);
    code[1] = op1;
    code[2] = op2;
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::emitN(JSOp op, size_t extra, ptrdiff_t* offset)
{
    MOZ_ASSERT(checkStrictOrSloppy(op));
    ptrdiff_t length = 1 + ptrdiff_t(extra);

    ptrdiff_t off;
    if (!emitCheck(length, &off)) {
        return false;
    }

    jsbytecode* code = this->code(off);
    code[0] = jsbytecode(op);
    /* The remaining |extra| bytes are set by the caller */

    /*
     * Don't updateDepth if op's use-count comes from the immediate
     * operand yet to be stored in the extra bytes after op.
     */
    if (CodeSpec[op].nuses >= 0) {
        updateDepth(off);
    }

    if (offset) {
        *offset = off;
    }
    return true;
}

bool
BytecodeEmitter::emitJumpTarget(JumpTarget* target)
{
    ptrdiff_t off = offset();

    // Alias consecutive jump targets.
    if (off == current->lastTarget.offset + ptrdiff_t(JSOP_JUMPTARGET_LENGTH)) {
        target->offset = current->lastTarget.offset;
        return true;
    }

    target->offset = off;
    current->lastTarget.offset = off;
    if (!emit1(JSOP_JUMPTARGET)) {
        return false;
    }
    return true;
}

bool
BytecodeEmitter::emitJumpNoFallthrough(JSOp op, JumpList* jump)
{
    ptrdiff_t offset;
    if (!emitCheck(5, &offset)) {
        return false;
    }

    jsbytecode* code = this->code(offset);
    code[0] = jsbytecode(op);
    MOZ_ASSERT(-1 <= jump->offset && jump->offset < offset);
    jump->push(this->code(0), offset);
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::emitJump(JSOp op, JumpList* jump)
{
    if (!emitJumpNoFallthrough(op, jump)) {
        return false;
    }
    if (BytecodeFallsThrough(op)) {
        JumpTarget fallthrough;
        if (!emitJumpTarget(&fallthrough)) {
            return false;
        }
    }
    return true;
}

bool
BytecodeEmitter::emitBackwardJump(JSOp op, JumpTarget target, JumpList* jump, JumpTarget* fallthrough)
{
    if (!emitJumpNoFallthrough(op, jump)) {
        return false;
    }
    patchJumpsToTarget(*jump, target);

    // Unconditionally create a fallthrough for closing iterators, and as a
    // target for break statements.
    if (!emitJumpTarget(fallthrough)) {
        return false;
    }
    return true;
}

void
BytecodeEmitter::patchJumpsToTarget(JumpList jump, JumpTarget target)
{
    MOZ_ASSERT(-1 <= jump.offset && jump.offset <= offset());
    MOZ_ASSERT(0 <= target.offset && target.offset <= offset());
    MOZ_ASSERT_IF(jump.offset != -1 && target.offset + 4 <= offset(),
                  BytecodeIsJumpTarget(JSOp(*code(target.offset))));
    jump.patchAll(code(0), target);
}

bool
BytecodeEmitter::emitJumpTargetAndPatch(JumpList jump)
{
    if (jump.offset == -1) {
        return true;
    }
    JumpTarget target;
    if (!emitJumpTarget(&target)) {
        return false;
    }
    patchJumpsToTarget(jump, target);
    return true;
}

bool
BytecodeEmitter::emitCall(JSOp op, uint16_t argc, const Maybe<uint32_t>& sourceCoordOffset)
{
    if (sourceCoordOffset.isSome()) {
        if (!updateSourceCoordNotes(*sourceCoordOffset)) {
            return false;
        }
    }
    return emit3(op, ARGC_LO(argc), ARGC_HI(argc));
}

bool
BytecodeEmitter::emitCall(JSOp op, uint16_t argc, ParseNode* pn)
{
    return emitCall(op, argc, pn ? Some(pn->pn_pos.begin) : Nothing());
}

bool
BytecodeEmitter::emitDupAt(unsigned slotFromTop)
{
    MOZ_ASSERT(slotFromTop < unsigned(stackDepth));

    if (slotFromTop == 0) {
        return emit1(JSOP_DUP);
    }

    if (slotFromTop >= JS_BIT(24)) {
        reportError(nullptr, JSMSG_TOO_MANY_LOCALS);
        return false;
    }

    ptrdiff_t off;
    if (!emitN(JSOP_DUPAT, 3, &off)) {
        return false;
    }

    jsbytecode* pc = code(off);
    SET_UINT24(pc, slotFromTop);
    return true;
}

bool
BytecodeEmitter::emitPopN(unsigned n)
{
    MOZ_ASSERT(n != 0);

    if (n == 1) {
        return emit1(JSOP_POP);
    }

    // 2 JSOP_POPs (2 bytes) are shorter than JSOP_POPN (3 bytes).
    if (n == 2) {
        return emit1(JSOP_POP) && emit1(JSOP_POP);
    }

    return emitUint16Operand(JSOP_POPN, n);
}

bool
BytecodeEmitter::emitCheckIsObj(CheckIsObjectKind kind)
{
    return emit2(JSOP_CHECKISOBJ, uint8_t(kind));
}

bool
BytecodeEmitter::emitCheckIsCallable(CheckIsCallableKind kind)
{
    return emit2(JSOP_CHECKISCALLABLE, uint8_t(kind));
}

static inline unsigned
LengthOfSetLine(unsigned line)
{
    return 1 /* SRC_SETLINE */ + (line > SN_4BYTE_OFFSET_MASK ? 4 : 1);
}

/* Updates line number notes, not column notes. */
bool
BytecodeEmitter::updateLineNumberNotes(uint32_t offset)
{
    ErrorReporter* er = &parser->errorReporter();
    bool onThisLine;
    if (!er->isOnThisLine(offset, currentLine(), &onThisLine)) {
        er->reportErrorNoOffset(JSMSG_OUT_OF_MEMORY);
        return false;
    }

    if (!onThisLine) {
        unsigned line = er->lineAt(offset);
        unsigned delta = line - currentLine();

        /*
         * Encode any change in the current source line number by using
         * either several SRC_NEWLINE notes or just one SRC_SETLINE note,
         * whichever consumes less space.
         *
         * NB: We handle backward line number deltas (possible with for
         * loops where the update part is emitted after the body, but its
         * line number is <= any line number in the body) here by letting
         * unsigned delta_ wrap to a very large number, which triggers a
         * SRC_SETLINE.
         */
        current->currentLine = line;
        current->lastColumn  = 0;
        if (delta >= LengthOfSetLine(line)) {
            if (!newSrcNote2(SRC_SETLINE, ptrdiff_t(line))) {
                return false;
            }
        } else {
            do {
                if (!newSrcNote(SRC_NEWLINE)) {
                    return false;
                }
            } while (--delta != 0);
        }
    }
    return true;
}

/* Updates the line number and column number information in the source notes. */
bool
BytecodeEmitter::updateSourceCoordNotes(uint32_t offset)
{
    if (!updateLineNumberNotes(offset)) {
        return false;
    }

    uint32_t columnIndex = parser->errorReporter().columnAt(offset);
    ptrdiff_t colspan = ptrdiff_t(columnIndex) - ptrdiff_t(current->lastColumn);
    if (colspan != 0) {
        // If the column span is so large that we can't store it, then just
        // discard this information. This can happen with minimized or otherwise
        // machine-generated code. Even gigantic column numbers are still
        // valuable if you have a source map to relate them to something real;
        // but it's better to fail soft here.
        if (!SN_REPRESENTABLE_COLSPAN(colspan)) {
            return true;
        }
        if (!newSrcNote2(SRC_COLSPAN, SN_COLSPAN_TO_OFFSET(colspan))) {
            return false;
        }
        current->lastColumn = columnIndex;
    }
    return true;
}

Maybe<uint32_t>
BytecodeEmitter::getOffsetForLoop(ParseNode* nextpn)
{
    if (!nextpn) {
        return Nothing();
    }

    // Try to give the JSOP_LOOPHEAD and JSOP_LOOPENTRY the same line number as
    // the next instruction. nextpn is often a block, in which case the next
    // instruction typically comes from the first statement inside.
    if (nextpn->is<LexicalScopeNode>()) {
        nextpn = nextpn->as<LexicalScopeNode>().scopeBody();
    }
    if (nextpn->isKind(ParseNodeKind::StatementList)) {
        if (ParseNode* firstStatement = nextpn->as<ListNode>().head()) {
            nextpn = firstStatement;
        }
    }

    return Some(nextpn->pn_pos.begin);
}

void
BytecodeEmitter::checkTypeSet(JSOp op)
{
    if (CodeSpec[op].format & JOF_TYPESET) {
        if (typesetCount < UINT16_MAX) {
            typesetCount++;
        }
    }
}

bool
BytecodeEmitter::emitUint16Operand(JSOp op, uint32_t operand)
{
    MOZ_ASSERT(operand <= UINT16_MAX);
    if (!emit3(op, UINT16_LO(operand), UINT16_HI(operand))) {
        return false;
    }
    checkTypeSet(op);
    return true;
}

bool
BytecodeEmitter::emitUint32Operand(JSOp op, uint32_t operand)
{
    ptrdiff_t off;
    if (!emitN(op, 4, &off)) {
        return false;
    }
    SET_UINT32(code(off), operand);
    checkTypeSet(op);
    return true;
}

namespace {

class NonLocalExitControl
{
  public:
    enum Kind
    {
        // IteratorClose is handled especially inside the exception unwinder.
        Throw,

        // A 'continue' statement does not call IteratorClose for the loop it
        // is continuing, i.e. excluding the target loop.
        Continue,

        // A 'break' or 'return' statement does call IteratorClose for the
        // loop it is breaking out of or returning from, i.e. including the
        // target loop.
        Break,
        Return
    };

  private:
    BytecodeEmitter* bce_;
    const uint32_t savedScopeNoteIndex_;
    const int savedDepth_;
    uint32_t openScopeNoteIndex_;
    Kind kind_;

    NonLocalExitControl(const NonLocalExitControl&) = delete;

    MOZ_MUST_USE bool leaveScope(EmitterScope* scope);

  public:
    NonLocalExitControl(BytecodeEmitter* bce, Kind kind)
      : bce_(bce),
        savedScopeNoteIndex_(bce->scopeNoteList.length()),
        savedDepth_(bce->stackDepth),
        openScopeNoteIndex_(bce->innermostEmitterScope()->noteIndex()),
        kind_(kind)
    { }

    ~NonLocalExitControl() {
        for (uint32_t n = savedScopeNoteIndex_; n < bce_->scopeNoteList.length(); n++) {
            bce_->scopeNoteList.recordEnd(n, bce_->offset(), bce_->inPrologue());
        }
        bce_->stackDepth = savedDepth_;
    }

    MOZ_MUST_USE bool prepareForNonLocalJump(NestableControl* target);

    MOZ_MUST_USE bool prepareForNonLocalJumpToOutermost() {
        return prepareForNonLocalJump(nullptr);
    }
};

bool
NonLocalExitControl::leaveScope(EmitterScope* es)
{
    if (!es->leave(bce_, /* nonLocal = */ true)) {
        return false;
    }

    // As we pop each scope due to the non-local jump, emit notes that
    // record the extent of the enclosing scope. These notes will have
    // their ends recorded in ~NonLocalExitControl().
    uint32_t enclosingScopeIndex = ScopeNote::NoScopeIndex;
    if (es->enclosingInFrame()) {
        enclosingScopeIndex = es->enclosingInFrame()->index();
    }
    if (!bce_->scopeNoteList.append(enclosingScopeIndex, bce_->offset(), bce_->inPrologue(),
                                    openScopeNoteIndex_))
    {
        return false;
    }
    openScopeNoteIndex_ = bce_->scopeNoteList.length() - 1;

    return true;
}

/*
 * Emit additional bytecode(s) for non-local jumps.
 */
bool
NonLocalExitControl::prepareForNonLocalJump(NestableControl* target)
{
    EmitterScope* es = bce_->innermostEmitterScope();
    int npops = 0;

    AutoCheckUnstableEmitterScope cues(bce_);

    // For 'continue', 'break', and 'return' statements, emit IteratorClose
    // bytecode inline. 'continue' statements do not call IteratorClose for
    // the loop they are continuing.
    bool emitIteratorClose = kind_ == Continue || kind_ == Break || kind_ == Return;
    bool emitIteratorCloseAtTarget = emitIteratorClose && kind_ != Continue;

    auto flushPops = [&npops](BytecodeEmitter* bce) {
        if (npops && !bce->emitPopN(npops)) {
            return false;
        }
        npops = 0;
        return true;
    };

    // Walk the nestable control stack and patch jumps.
    for (NestableControl* control = bce_->innermostNestableControl;
         control != target;
         control = control->enclosing())
    {
        // Walk the scope stack and leave the scopes we entered. Leaving a scope
        // may emit administrative ops like JSOP_POPLEXICALENV but never anything
        // that manipulates the stack.
        for (; es != control->emitterScope(); es = es->enclosingInFrame()) {
            if (!leaveScope(es)) {
                return false;
            }
        }

        switch (control->kind()) {
          case StatementKind::Finally: {
            TryFinallyControl& finallyControl = control->as<TryFinallyControl>();
            if (finallyControl.emittingSubroutine()) {
                /*
                 * There's a [exception or hole, retsub pc-index] pair and the
                 * possible return value on the stack that we need to pop.
                 */
                npops += 3;
            } else {
                if (!flushPops(bce_)) {
                    return false;
                }
                if (!bce_->emitGoSub(&finallyControl.gosubs)) { // ...
                    return false;
                }
            }
            break;
          }

          case StatementKind::ForOfLoop:
            if (emitIteratorClose) {
                if (!flushPops(bce_)) {
                    return false;
                }

                ForOfLoopControl& loopinfo = control->as<ForOfLoopControl>();
                if (!loopinfo.emitPrepareForNonLocalJumpFromScope(bce_, *es,
                                                                  /* isTarget = */ false))
                {                                         // ...
                    return false;
                }
            } else {
                // The iterator next method, the iterator, and the current
                // value are on the stack.
                npops += 3;
            }
            break;

          case StatementKind::ForInLoop:
            if (!flushPops(bce_)) {
                return false;
            }

            // The iterator and the current value are on the stack.
            if (!bce_->emit1(JSOP_POP)) {                 // ... ITER
                return false;
            }
            if (!bce_->emit1(JSOP_ENDITER)) {             // ...
                return false;
            }
            break;

          default:
            break;
        }
    }

    if (!flushPops(bce_)) {
        return false;
    }

    if (target && emitIteratorCloseAtTarget && target->is<ForOfLoopControl>()) {
        ForOfLoopControl& loopinfo = target->as<ForOfLoopControl>();
        if (!loopinfo.emitPrepareForNonLocalJumpFromScope(bce_, *es,
                                                          /* isTarget = */ true))
        {                                                 // ... UNDEF UNDEF UNDEF
            return false;
        }
    }

    EmitterScope* targetEmitterScope = target ? target->emitterScope() : bce_->varEmitterScope;
    for (; es != targetEmitterScope; es = es->enclosingInFrame()) {
        if (!leaveScope(es)) {
            return false;
        }
    }

    return true;
}

}  // anonymous namespace

bool
BytecodeEmitter::emitGoto(NestableControl* target, JumpList* jumplist, SrcNoteType noteType)
{
    NonLocalExitControl nle(this, noteType == SRC_CONTINUE
                                  ? NonLocalExitControl::Continue
                                  : NonLocalExitControl::Break);

    if (!nle.prepareForNonLocalJump(target)) {
        return false;
    }

    if (noteType != SRC_NULL) {
        if (!newSrcNote(noteType)) {
            return false;
        }
    }

    return emitJump(JSOP_GOTO, jumplist);
}

Scope*
BytecodeEmitter::innermostScope() const
{
    return innermostEmitterScope()->scope(this);
}

bool
BytecodeEmitter::emitIndex32(JSOp op, uint32_t index)
{
    MOZ_ASSERT(checkStrictOrSloppy(op));

    const size_t len = 1 + UINT32_INDEX_LEN;
    MOZ_ASSERT(len == size_t(CodeSpec[op].length));

    ptrdiff_t offset;
    if (!emitCheck(len, &offset)) {
        return false;
    }

    jsbytecode* code = this->code(offset);
    code[0] = jsbytecode(op);
    SET_UINT32_INDEX(code, index);
    checkTypeSet(op);
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::emitIndexOp(JSOp op, uint32_t index)
{
    MOZ_ASSERT(checkStrictOrSloppy(op));

    const size_t len = CodeSpec[op].length;
    MOZ_ASSERT(len >= 1 + UINT32_INDEX_LEN);

    ptrdiff_t offset;
    if (!emitCheck(len, &offset)) {
        return false;
    }

    jsbytecode* code = this->code(offset);
    code[0] = jsbytecode(op);
    SET_UINT32_INDEX(code, index);
    checkTypeSet(op);
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::emitAtomOp(JSAtom* atom, JSOp op)
{
    MOZ_ASSERT(atom);

    // .generator lookups should be emitted as JSOP_GETALIASEDVAR instead of
    // JSOP_GETNAME etc, to bypass |with| objects on the scope chain.
    // It's safe to emit .this lookups though because |with| objects skip
    // those.
    MOZ_ASSERT_IF(op == JSOP_GETNAME || op == JSOP_GETGNAME,
                  atom != cx->names().dotGenerator);

    if (op == JSOP_GETPROP && atom == cx->names().length) {
        /* Specialize length accesses for the interpreter. */
        op = JSOP_LENGTH;
    }

    uint32_t index;
    if (!makeAtomIndex(atom, &index)) {
        return false;
    }

    return emitAtomOp(index, op);
}

bool
BytecodeEmitter::emitAtomOp(uint32_t atomIndex, JSOp op)
{
    MOZ_ASSERT(JOF_OPTYPE(op) == JOF_ATOM);

    return emitIndexOp(op, atomIndex);
}

bool
BytecodeEmitter::emitInternedScopeOp(uint32_t index, JSOp op)
{
    MOZ_ASSERT(JOF_OPTYPE(op) == JOF_SCOPE);
    MOZ_ASSERT(index < scopeList.length());
    return emitIndex32(op, index);
}

bool
BytecodeEmitter::emitInternedObjectOp(uint32_t index, JSOp op)
{
    MOZ_ASSERT(JOF_OPTYPE(op) == JOF_OBJECT);
    MOZ_ASSERT(index < objectList.length);
    return emitIndex32(op, index);
}

bool
BytecodeEmitter::emitObjectOp(ObjectBox* objbox, JSOp op)
{
    return emitInternedObjectOp(objectList.add(objbox), op);
}

bool
BytecodeEmitter::emitObjectPairOp(ObjectBox* objbox1, ObjectBox* objbox2, JSOp op)
{
    uint32_t index = objectList.add(objbox1);
    objectList.add(objbox2);
    return emitInternedObjectOp(index, op);
}

bool
BytecodeEmitter::emitRegExp(uint32_t index)
{
    return emitIndex32(JSOP_REGEXP, index);
}

bool
BytecodeEmitter::emitLocalOp(JSOp op, uint32_t slot)
{
    MOZ_ASSERT(JOF_OPTYPE(op) != JOF_ENVCOORD);
    MOZ_ASSERT(IsLocalOp(op));

    ptrdiff_t off;
    if (!emitN(op, LOCALNO_LEN, &off)) {
        return false;
    }

    SET_LOCALNO(code(off), slot);
    return true;
}

bool
BytecodeEmitter::emitArgOp(JSOp op, uint16_t slot)
{
    MOZ_ASSERT(IsArgOp(op));
    ptrdiff_t off;
    if (!emitN(op, ARGNO_LEN, &off)) {
        return false;
    }

    SET_ARGNO(code(off), slot);
    return true;
}

bool
BytecodeEmitter::emitEnvCoordOp(JSOp op, EnvironmentCoordinate ec)
{
    MOZ_ASSERT(JOF_OPTYPE(op) == JOF_ENVCOORD);

    unsigned n = ENVCOORD_HOPS_LEN + ENVCOORD_SLOT_LEN;
    MOZ_ASSERT(int(n) + 1 /* op */ == CodeSpec[op].length);

    ptrdiff_t off;
    if (!emitN(op, n, &off)) {
        return false;
    }

    jsbytecode* pc = code(off);
    SET_ENVCOORD_HOPS(pc, ec.hops());
    pc += ENVCOORD_HOPS_LEN;
    SET_ENVCOORD_SLOT(pc, ec.slot());
    pc += ENVCOORD_SLOT_LEN;
    checkTypeSet(op);
    return true;
}

JSOp
BytecodeEmitter::strictifySetNameOp(JSOp op)
{
    switch (op) {
      case JSOP_SETNAME:
        if (sc->strict()) {
            op = JSOP_STRICTSETNAME;
        }
        break;
      case JSOP_SETGNAME:
        if (sc->strict()) {
            op = JSOP_STRICTSETGNAME;
        }
        break;
        default:;
    }
    return op;
}

bool
BytecodeEmitter::checkSideEffects(ParseNode* pn, bool* answer)
{
    if (!CheckRecursionLimit(cx)) {
        return false;
    }

 restart:

    switch (pn->getKind()) {
      // Trivial cases with no side effects.
      case ParseNodeKind::EmptyStatement:
      case ParseNodeKind::True:
      case ParseNodeKind::False:
      case ParseNodeKind::Null:
      case ParseNodeKind::RawUndefined:
      case ParseNodeKind::Elision:
      case ParseNodeKind::Generator:
        MOZ_ASSERT(pn->is<NullaryNode>());
        *answer = false;
        return true;

      case ParseNodeKind::ObjectPropertyName:
      case ParseNodeKind::PrivateName: // no side effects, unlike ParseNodeKind::Name
      case ParseNodeKind::String:
      case ParseNodeKind::TemplateString:
        MOZ_ASSERT(pn->is<NameNode>());
        *answer = false;
        return true;

      case ParseNodeKind::RegExp:
        MOZ_ASSERT(pn->is<RegExpLiteral>());
        *answer = false;
        return true;

      case ParseNodeKind::Number:
        MOZ_ASSERT(pn->is<NumericLiteral>());
        *answer = false;
        return true;

      // |this| can throw in derived class constructors, including nested arrow
      // functions or eval.
      case ParseNodeKind::This:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = sc->needsThisTDZChecks();
        return true;

      // Trivial binary nodes with more token pos holders.
      case ParseNodeKind::NewTarget:
      case ParseNodeKind::ImportMeta: {
        MOZ_ASSERT(pn->as<BinaryNode>().left()->isKind(ParseNodeKind::PosHolder));
        MOZ_ASSERT(pn->as<BinaryNode>().right()->isKind(ParseNodeKind::PosHolder));
        *answer = false;
        return true;
      }

      case ParseNodeKind::Break:
        MOZ_ASSERT(pn->is<BreakStatement>());
        *answer = true;
        return true;

      case ParseNodeKind::Continue:
        MOZ_ASSERT(pn->is<ContinueStatement>());
        *answer = true;
        return true;

      case ParseNodeKind::Debugger:
        MOZ_ASSERT(pn->is<DebuggerStatement>());
        *answer = true;
        return true;

      // Watch out for getters!
      case ParseNodeKind::Dot:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      // Unary cases with side effects only if the child has them.
      case ParseNodeKind::TypeOfExpr:
      case ParseNodeKind::Void:
      case ParseNodeKind::Not:
        return checkSideEffects(pn->as<UnaryNode>().kid(), answer);

      // Even if the name expression is effect-free, performing ToPropertyKey on
      // it might not be effect-free:
      //
      //   RegExp.prototype.toString = () => { throw 42; };
      //   ({ [/regex/]: 0 }); // ToPropertyKey(/regex/) throws 42
      //
      //   function Q() {
      //     ({ [new.target]: 0 });
      //   }
      //   Q.toString = () => { throw 17; };
      //   new Q; // new.target will be Q, ToPropertyKey(Q) throws 17
      case ParseNodeKind::ComputedName:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // Looking up or evaluating the associated name could throw.
      case ParseNodeKind::TypeOfName:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // This unary case has side effects on the enclosing object, sure.  But
      // that's not the question this function answers: it's whether the
      // operation may have a side effect on something *other* than the result
      // of the overall operation in which it's embedded.  The answer to that
      // is no, because an object literal having a mutated prototype only
      // produces a value, without affecting anything else.
      case ParseNodeKind::MutateProto:
        return checkSideEffects(pn->as<UnaryNode>().kid(), answer);

      // Unary cases with obvious side effects.
      case ParseNodeKind::PreIncrement:
      case ParseNodeKind::PostIncrement:
      case ParseNodeKind::PreDecrement:
      case ParseNodeKind::PostDecrement:
      case ParseNodeKind::Throw:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // These might invoke valueOf/toString, even with a subexpression without
      // side effects!  Consider |+{ valueOf: null, toString: null }|.
      case ParseNodeKind::BitNot:
      case ParseNodeKind::Pos:
      case ParseNodeKind::Neg:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // This invokes the (user-controllable) iterator protocol.
      case ParseNodeKind::Spread:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      case ParseNodeKind::InitialYield:
      case ParseNodeKind::YieldStar:
      case ParseNodeKind::Yield:
      case ParseNodeKind::Await:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // Deletion generally has side effects, even if isolated cases have none.
      case ParseNodeKind::DeleteName:
      case ParseNodeKind::DeleteProp:
      case ParseNodeKind::DeleteElem:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      // Deletion of a non-Reference expression has side effects only through
      // evaluating the expression.
      case ParseNodeKind::DeleteExpr: {
        ParseNode* expr = pn->as<UnaryNode>().kid();
        return checkSideEffects(expr, answer);
      }

      case ParseNodeKind::ExpressionStatement:
        return checkSideEffects(pn->as<UnaryNode>().kid(), answer);

      // Binary cases with obvious side effects.
      case ParseNodeKind::Assign:
      case ParseNodeKind::AddAssign:
      case ParseNodeKind::SubAssign:
      case ParseNodeKind::BitOrAssign:
      case ParseNodeKind::BitXorAssign:
      case ParseNodeKind::BitAndAssign:
      case ParseNodeKind::LshAssign:
      case ParseNodeKind::RshAssign:
      case ParseNodeKind::UrshAssign:
      case ParseNodeKind::MulAssign:
      case ParseNodeKind::DivAssign:
      case ParseNodeKind::ModAssign:
      case ParseNodeKind::PowAssign:
        MOZ_ASSERT(pn->is<AssignmentNode>());
        *answer = true;
        return true;

      case ParseNodeKind::SetThis:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      case ParseNodeKind::StatementList:
      // Strict equality operations and logical operators are well-behaved and
      // perform no conversions.
      case ParseNodeKind::Or:
      case ParseNodeKind::And:
      case ParseNodeKind::StrictEq:
      case ParseNodeKind::StrictNe:
      // Any subexpression of a comma expression could be effectful.
      case ParseNodeKind::Comma:
        MOZ_ASSERT(!pn->as<ListNode>().empty());
        MOZ_FALLTHROUGH;
      // Subcomponents of a literal may be effectful.
      case ParseNodeKind::Array:
      case ParseNodeKind::Object:
        for (ParseNode* item : pn->as<ListNode>().contents()) {
            if (!checkSideEffects(item, answer)) {
                return false;
            }
            if (*answer) {
                return true;
            }
        }
        return true;

      // Most other binary operations (parsed as lists in SpiderMonkey) may
      // perform conversions triggering side effects.  Math operations perform
      // ToNumber and may fail invoking invalid user-defined toString/valueOf:
      // |5 < { toString: null }|.  |instanceof| throws if provided a
      // non-object constructor: |null instanceof null|.  |in| throws if given
      // a non-object RHS: |5 in null|.
      case ParseNodeKind::BitOr:
      case ParseNodeKind::BitXor:
      case ParseNodeKind::BitAnd:
      case ParseNodeKind::Eq:
      case ParseNodeKind::Ne:
      case ParseNodeKind::Lt:
      case ParseNodeKind::Le:
      case ParseNodeKind::Gt:
      case ParseNodeKind::Ge:
      case ParseNodeKind::InstanceOf:
      case ParseNodeKind::In:
      case ParseNodeKind::Lsh:
      case ParseNodeKind::Rsh:
      case ParseNodeKind::Ursh:
      case ParseNodeKind::Add:
      case ParseNodeKind::Sub:
      case ParseNodeKind::Star:
      case ParseNodeKind::Div:
      case ParseNodeKind::Mod:
      case ParseNodeKind::Pow:
        MOZ_ASSERT(pn->as<ListNode>().count() >= 2);
        *answer = true;
        return true;

      case ParseNodeKind::Colon:
      case ParseNodeKind::Case: {
        BinaryNode* node = &pn->as<BinaryNode>();
        if (!checkSideEffects(node->left(), answer)) {
            return false;
        }
        if (*answer) {
            return true;
        }
        return checkSideEffects(node->right(), answer);
      }

      // More getters.
      case ParseNodeKind::Elem:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      // These affect visible names in this code, or in other code.
      case ParseNodeKind::Import:
      case ParseNodeKind::ExportFrom:
      case ParseNodeKind::ExportDefault:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      // Likewise.
      case ParseNodeKind::Export:
        MOZ_ASSERT(pn->is<UnaryNode>());
        *answer = true;
        return true;

      case ParseNodeKind::CallImport:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      // Every part of a loop might be effect-free, but looping infinitely *is*
      // an effect.  (Language lawyer trivia: C++ says threads can be assumed
      // to exit or have side effects, C++14 [intro.multithread]p27, so a C++
      // implementation's equivalent of the below could set |*answer = false;|
      // if all loop sub-nodes set |*answer = false|!)
      case ParseNodeKind::DoWhile:
      case ParseNodeKind::While:
      case ParseNodeKind::For:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      // Declarations affect the name set of the relevant scope.
      case ParseNodeKind::Var:
      case ParseNodeKind::Const:
      case ParseNodeKind::Let:
        MOZ_ASSERT(pn->is<ListNode>());
        *answer = true;
        return true;

      case ParseNodeKind::If:
      case ParseNodeKind::Conditional:
      {
        TernaryNode* node = &pn->as<TernaryNode>();
        if (!checkSideEffects(node->kid1(), answer)) {
            return false;
        }
        if (*answer) {
            return true;
        }
        if (!checkSideEffects(node->kid2(), answer)) {
            return false;
        }
        if (*answer) {
            return true;
        }
        if ((pn = node->kid3())) {
            goto restart;
        }
        return true;
      }

      // Function calls can invoke non-local code.
      case ParseNodeKind::New:
      case ParseNodeKind::Call:
      case ParseNodeKind::TaggedTemplate:
      case ParseNodeKind::SuperCall:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      // Function arg lists can contain arbitrary expressions. Technically
      // this only causes side-effects if one of the arguments does, but since
      // the call being made will always trigger side-effects, it isn't needed.
      case ParseNodeKind::Arguments:
        MOZ_ASSERT(pn->is<ListNode>());
        *answer = true;
        return true;

      case ParseNodeKind::Pipeline:
        MOZ_ASSERT(pn->as<ListNode>().count() >= 2);
        *answer = true;
        return true;

      // Classes typically introduce names.  Even if no name is introduced,
      // the heritage and/or class body (through computed property names)
      // usually have effects.
      case ParseNodeKind::Class:
        MOZ_ASSERT(pn->is<ClassNode>());
        *answer = true;
        return true;

      // |with| calls |ToObject| on its expression and so throws if that value
      // is null/undefined.
      case ParseNodeKind::With:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      case ParseNodeKind::Return:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      case ParseNodeKind::Name:
        MOZ_ASSERT(pn->is<NameNode>());
        *answer = true;
        return true;

      // Shorthands could trigger getters: the |x| in the object literal in
      // |with ({ get x() { throw 42; } }) ({ x });|, for example, triggers
      // one.  (Of course, it isn't necessary to use |with| for a shorthand to
      // trigger a getter.)
      case ParseNodeKind::Shorthand:
        MOZ_ASSERT(pn->is<BinaryNode>());
        *answer = true;
        return true;

      case ParseNodeKind::Function:
        MOZ_ASSERT(pn->is<CodeNode>());
        /*
         * A named function, contrary to ES3, is no longer effectful, because
         * we bind its name lexically (using JSOP_CALLEE) instead of creating
         * an Object instance and binding a readonly, permanent property in it
         * (the object and binding can be detected and hijacked or captured).
         * This is a bug fix to ES3; it is fixed in ES3.1 drafts.
         */
        *answer = false;
        return true;

      case ParseNodeKind::Module:
        *answer = false;
        return true;

      case ParseNodeKind::Try:
      {
        TryNode* tryNode = &pn->as<TryNode>();
        if (!checkSideEffects(tryNode->body(), answer)) {
            return false;
        }
        if (*answer) {
            return true;
        }
        if (LexicalScopeNode* catchScope = tryNode->catchScope()) {
            if (!checkSideEffects(catchScope, answer)) {
                return false;
            }
            if (*answer) {
                return true;
            }
        }
        if (ParseNode* finallyBlock = tryNode->finallyBlock()) {
            if (!checkSideEffects(finallyBlock, answer)) {
                return false;
            }
        }
        return true;
      }

      case ParseNodeKind::Catch: {
        BinaryNode* catchClause = &pn->as<BinaryNode>();
        if (ParseNode* name = catchClause->left()) {
            if (!checkSideEffects(name, answer)) {
                return false;
            }
            if (*answer) {
                return true;
            }
        }
        return checkSideEffects(catchClause->right(), answer);
      }

      case ParseNodeKind::Switch: {
        SwitchStatement* switchStmt = &pn->as<SwitchStatement>();
        if (!checkSideEffects(&switchStmt->discriminant(), answer)) {
            return false;
        }
        return *answer || checkSideEffects(&switchStmt->lexicalForCaseList(), answer);
      }

      case ParseNodeKind::Label:
        return checkSideEffects(pn->as<LabeledStatement>().statement(), answer);

      case ParseNodeKind::LexicalScope:
        return checkSideEffects(pn->as<LexicalScopeNode>().scopeBody(), answer);

      // We could methodically check every interpolated expression, but it's
      // probably not worth the trouble.  Treat template strings as effect-free
      // only if they don't contain any substitutions.
      case ParseNodeKind::TemplateStringList: {
        ListNode* list = &pn->as<ListNode>();
        MOZ_ASSERT(!list->empty());
        MOZ_ASSERT((list->count() % 2) == 1,
                   "template strings must alternate template and substitution "
                   "parts");
        *answer = list->count() > 1;
        return true;
      }

      // This should be unreachable but is left as-is for now.
      case ParseNodeKind::ParamsBody:
        *answer = true;
        return true;

      case ParseNodeKind::ForIn:           // by ParseNodeKind::For
      case ParseNodeKind::ForOf:           // by ParseNodeKind::For
      case ParseNodeKind::ForHead:         // by ParseNodeKind::For
      case ParseNodeKind::ClassMethod:     // by ParseNodeKind::Class
      case ParseNodeKind::ClassField:      // by ParseNodeKind::Class
      case ParseNodeKind::ClassNames:      // by ParseNodeKind::Class
      case ParseNodeKind::ClassMemberList: // by ParseNodeKind::Class
      case ParseNodeKind::ImportSpecList:  // by ParseNodeKind::Import
      case ParseNodeKind::ImportSpec:      // by ParseNodeKind::Import
      case ParseNodeKind::ExportBatchSpec: // by ParseNodeKind::Export
      case ParseNodeKind::ExportSpecList:  // by ParseNodeKind::Export
      case ParseNodeKind::ExportSpec:      // by ParseNodeKind::Export
      case ParseNodeKind::CallSiteObj:     // by ParseNodeKind::TaggedTemplate
      case ParseNodeKind::PosHolder:       // by ParseNodeKind::NewTarget
      case ParseNodeKind::SuperBase:       // by ParseNodeKind::Elem and others
      case ParseNodeKind::PropertyName:    // by ParseNodeKind::Dot
        MOZ_CRASH("handled by parent nodes");

      case ParseNodeKind::Limit: // invalid sentinel value
        MOZ_CRASH("invalid node kind");
    }

    MOZ_CRASH("invalid, unenumerated ParseNodeKind value encountered in "
              "BytecodeEmitter::checkSideEffects");
}

bool
BytecodeEmitter::isInLoop()
{
    return findInnermostNestableControl<LoopControl>();
}

bool
BytecodeEmitter::checkSingletonContext()
{
    if (!script->treatAsRunOnce() || sc->isFunctionBox() || isInLoop()) {
        return false;
    }
    hasSingletons = true;
    return true;
}

bool
BytecodeEmitter::checkRunOnceContext()
{
    return checkSingletonContext() || (!isInLoop() && isRunOnceLambda());
}

bool
BytecodeEmitter::needsImplicitThis()
{
    // Short-circuit if there is an enclosing 'with' scope.
    if (sc->inWith()) {
        return true;
    }

    // Otherwise see if the current point is under a 'with'.
    for (EmitterScope* es = innermostEmitterScope(); es; es = es->enclosingInFrame()) {
        if (es->scope(this)->kind() == ScopeKind::With) {
            return true;
        }
    }

    return false;
}

void
BytecodeEmitter::tellDebuggerAboutCompiledScript(JSContext* cx)
{
    // Note: when parsing off thread the resulting scripts need to be handed to
    // the debugger after rejoining to the main thread.
    if (cx->helperThread()) {
        return;
    }

    // Lazy scripts are never top level (despite always being invoked with a
    // nullptr parent), and so the hook should never be fired.
    if (emitterMode != LazyFunction && !parent) {
        Debugger::onNewScript(cx, script);
    }
}

void
BytecodeEmitter::reportError(ParseNode* pn, unsigned errorNumber, ...)
{
    MOZ_ASSERT_IF(!pn, this->scriptStartOffsetSet);
    uint32_t offset = pn ? pn->pn_pos.begin : this->scriptStartOffset;

    va_list args;
    va_start(args, errorNumber);

    parser->errorReporter().errorAtVA(offset, errorNumber, &args);

    va_end(args);
}

void
BytecodeEmitter::reportError(const Maybe<uint32_t>& maybeOffset, unsigned errorNumber, ...)
{
    MOZ_ASSERT_IF(!maybeOffset, this->scriptStartOffsetSet);
    uint32_t offset = maybeOffset ? *maybeOffset : this->scriptStartOffset;

    va_list args;
    va_start(args, errorNumber);

    parser->errorReporter().errorAtVA(offset, errorNumber, &args);

    va_end(args);
}

bool
BytecodeEmitter::reportExtraWarning(ParseNode* pn, unsigned errorNumber, ...)
{
    MOZ_ASSERT_IF(!pn, this->scriptStartOffsetSet);
    uint32_t offset = pn ? pn->pn_pos.begin : this->scriptStartOffset;

    va_list args;
    va_start(args, errorNumber);

    bool result = parser->errorReporter().reportExtraWarningErrorNumberVA(nullptr, offset, errorNumber, &args);

    va_end(args);
    return result;
}

bool
BytecodeEmitter::reportExtraWarning(const Maybe<uint32_t>& maybeOffset,
                                    unsigned errorNumber, ...)
{
    MOZ_ASSERT_IF(!maybeOffset, this->scriptStartOffsetSet);
    uint32_t offset = maybeOffset ? *maybeOffset : this->scriptStartOffset;

    va_list args;
    va_start(args, errorNumber);

    bool result = parser->errorReporter().reportExtraWarningErrorNumberVA(nullptr, offset, errorNumber, &args);

    va_end(args);
    return result;
}

bool
BytecodeEmitter::emitNewInit()
{
    const size_t len = 1 + UINT32_INDEX_LEN;
    ptrdiff_t offset;
    if (!emitCheck(len, &offset)) {
        return false;
    }

    jsbytecode* code = this->code(offset);
    code[0] = JSOP_NEWINIT;
    code[1] = 0;
    code[2] = 0;
    code[3] = 0;
    code[4] = 0;
    checkTypeSet(JSOP_NEWINIT);
    updateDepth(offset);
    return true;
}

bool
BytecodeEmitter::iteratorResultShape(unsigned* shape)
{
    // No need to do any guessing for the object kind, since we know exactly how
    // many properties we plan to have.
    gc::AllocKind kind = gc::GetGCObjectKind(2);
    RootedPlainObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx, kind, TenuredObject));
    if (!obj) {
        return false;
    }

    Rooted<jsid> value_id(cx, NameToId(cx->names().value));
    Rooted<jsid> done_id(cx, NameToId(cx->names().done));
    if (!NativeDefineDataProperty(cx, obj, value_id, UndefinedHandleValue, JSPROP_ENUMERATE)) {
        return false;
    }
    if (!NativeDefineDataProperty(cx, obj, done_id, UndefinedHandleValue, JSPROP_ENUMERATE)) {
        return false;
    }

    ObjectBox* objbox = parser->newObjectBox(obj);
    if (!objbox) {
        return false;
    }

    *shape = objectList.add(objbox);

    return true;
}

bool
BytecodeEmitter::emitPrepareIteratorResult()
{
    unsigned shape;
    if (!iteratorResultShape(&shape)) {
        return false;
    }
    return emitIndex32(JSOP_NEWOBJECT, shape);
}

bool
BytecodeEmitter::emitFinishIteratorResult(bool done)
{
    uint32_t value_id;
    if (!makeAtomIndex(cx->names().value, &value_id)) {
        return false;
    }
    uint32_t done_id;
    if (!makeAtomIndex(cx->names().done, &done_id)) {
        return false;
    }

    if (!emitIndex32(JSOP_INITPROP, value_id)) {
        return false;
    }
    if (!emit1(done ? JSOP_TRUE : JSOP_FALSE)) {
        return false;
    }
    if (!emitIndex32(JSOP_INITPROP, done_id)) {
        return false;
    }
    return true;
}

bool
BytecodeEmitter::emitGetNameAtLocation(JSAtom* name, const NameLocation& loc)
{
    NameOpEmitter noe(this, name, loc, NameOpEmitter::Kind::Get);
    if (!noe.emitGet()) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitGetName(NameNode* name)
{
    return emitGetName(name->name());
}

bool
BytecodeEmitter::emitTDZCheckIfNeeded(JSAtom* name, const NameLocation& loc)
{
    // Dynamic accesses have TDZ checks built into their VM code and should
    // never emit explicit TDZ checks.
    MOZ_ASSERT(loc.hasKnownSlot());
    MOZ_ASSERT(loc.isLexical());

    Maybe<MaybeCheckTDZ> check = innermostTDZCheckCache->needsTDZCheck(this, name);
    if (!check) {
        return false;
    }

    // We've already emitted a check in this basic block.
    if (*check == DontCheckTDZ) {
        return true;
    }

    if (loc.kind() == NameLocation::Kind::FrameSlot) {
        if (!emitLocalOp(JSOP_CHECKLEXICAL, loc.frameSlot())) {
            return false;
        }
    } else {
        if (!emitEnvCoordOp(JSOP_CHECKALIASEDLEXICAL, loc.environmentCoordinate())) {
            return false;
        }
    }

    return innermostTDZCheckCache->noteTDZCheck(this, name, DontCheckTDZ);
}

bool
BytecodeEmitter::emitPropLHS(PropertyAccess* prop)
{
    MOZ_ASSERT(!prop->isSuper());

    ParseNode* expr = &prop->expression();

    if (!expr->is<PropertyAccess>() || expr->as<PropertyAccess>().isSuper()) {
        // The non-optimized case.
        return emitTree(expr);
    }

    // If the object operand is also a dotted property reference, reverse the
    // list linked via expression() temporarily so we can iterate over it from
    // the bottom up (reversing again as we go), to avoid excessive recursion.
    PropertyAccess* pndot = &expr->as<PropertyAccess>();
    ParseNode* pnup = nullptr;
    ParseNode* pndown;
    for (;;) {
        // Reverse pndot->expression() to point up, not down.
        pndown = &pndot->expression();
        pndot->setExpression(pnup);
        if (!pndown->is<PropertyAccess>() || pndown->as<PropertyAccess>().isSuper()) {
            break;
        }
        pnup = pndot;
        pndot = &pndown->as<PropertyAccess>();
    }

    // pndown is a primary expression, not a dotted property reference.
    if (!emitTree(pndown)) {
        return false;
    }

    while (true) {
        // TODO(khyperia): Implement private field access.

        // Walk back up the list, emitting annotated name ops.
        if (!emitAtomOp(pndot->key().atom(), JSOP_GETPROP)) {
            return false;
        }

        // Reverse the pndot->expression() link again.
        pnup = pndot->maybeExpression();
        pndot->setExpression(pndown);
        pndown = pndot;
        if (!pnup) {
            break;
        }
        pndot = &pnup->as<PropertyAccess>();
    }
    return true;
}

bool
BytecodeEmitter::emitPropIncDec(UnaryNode* incDec)
{
    PropertyAccess* prop = &incDec->kid()->as<PropertyAccess>();
    // TODO(khyperia): Implement private field access.
    bool isSuper = prop->isSuper();
    ParseNodeKind kind = incDec->getKind();
    PropOpEmitter poe(this,
                      kind == ParseNodeKind::PostIncrement ? PropOpEmitter::Kind::PostIncrement
                      : kind == ParseNodeKind::PreIncrement ? PropOpEmitter::Kind::PreIncrement
                      : kind == ParseNodeKind::PostDecrement ? PropOpEmitter::Kind::PostDecrement
                      : PropOpEmitter::Kind::PreDecrement,
                      isSuper
                      ? PropOpEmitter::ObjKind::Super
                      : PropOpEmitter::ObjKind::Other);
    if (!poe.prepareForObj()) {
        return false;
    }
    if (isSuper) {
        UnaryNode* base = &prop->expression().as<UnaryNode>();
        if (!emitGetThisForSuperBase(base)) {       // THIS
            return false;
        }
    } else {
        if (!emitPropLHS(prop)) {
            return false;                           // OBJ
        }
    }
    if (!poe.emitIncDec(prop->key().atom())) {      // RESULT
        return false;
    }

    return true;
}


bool
BytecodeEmitter::emitNameIncDec(UnaryNode* incDec)
{
    MOZ_ASSERT(incDec->kid()->isKind(ParseNodeKind::Name));

    ParseNodeKind kind = incDec->getKind();
    NameNode* name = &incDec->kid()->as<NameNode>();
    NameOpEmitter noe(this, name->atom(),
                      kind == ParseNodeKind::PostIncrement ? NameOpEmitter::Kind::PostIncrement
                      : kind == ParseNodeKind::PreIncrement ? NameOpEmitter::Kind::PreIncrement
                      : kind == ParseNodeKind::PostDecrement ? NameOpEmitter::Kind::PostDecrement
                      : NameOpEmitter::Kind::PreDecrement);
    if (!noe.emitIncDec()) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitElemOpBase(JSOp op)
{
    if (!emit1(op)) {
        return false;
    }

    checkTypeSet(op);
    return true;
}

bool
BytecodeEmitter::emitElemObjAndKey(PropertyByValue* elem, bool isSuper, ElemOpEmitter& eoe)
{
    if (isSuper) {
        if (!eoe.prepareForObj()) {                       //
            return false;
        }
        UnaryNode* base = &elem->expression().as<UnaryNode>();
        if (!emitGetThisForSuperBase(base)) {             // THIS
            return false;
        }
        if (!eoe.prepareForKey()) {                       // THIS
            return false;
        }
        if (!emitTree(&elem->key())) {                    // THIS KEY
            return false;
        }

        return true;
    }

    if (!eoe.prepareForObj()) {                           //
        return false;
    }
    if (!emitTree(&elem->expression())) {                 // OBJ
        return false;
    }
    if (!eoe.prepareForKey()) {                           // OBJ? OBJ
        return false;
    }
    if (!emitTree(&elem->key())) {                        // OBJ? OBJ KEY
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitElemIncDec(UnaryNode* incDec)
{
    PropertyByValue* elemExpr = &incDec->kid()->as<PropertyByValue>();
    bool isSuper = elemExpr->isSuper();
    ParseNodeKind kind = incDec->getKind();
    ElemOpEmitter eoe(this,
                      kind == ParseNodeKind::PostIncrement ? ElemOpEmitter::Kind::PostIncrement
                      : kind == ParseNodeKind::PreIncrement ? ElemOpEmitter::Kind::PreIncrement
                      : kind == ParseNodeKind::PostDecrement ? ElemOpEmitter::Kind::PostDecrement
                      : ElemOpEmitter::Kind::PreDecrement,
                      isSuper
                      ? ElemOpEmitter::ObjKind::Super
                      : ElemOpEmitter::ObjKind::Other);
    if (!emitElemObjAndKey(elemExpr, isSuper, eoe)) {     // [Super]
        //                                                // THIS KEY
        //                                                // [Other]
        //                                                // OBJ KEY
        return false;
    }
    if (!eoe.emitIncDec()) {                              // RESULT
         return false;
    }

    return true;
}

bool
BytecodeEmitter::emitCallIncDec(UnaryNode* incDec)
{
    MOZ_ASSERT(incDec->isKind(ParseNodeKind::PreIncrement) ||
               incDec->isKind(ParseNodeKind::PostIncrement) ||
               incDec->isKind(ParseNodeKind::PreDecrement) ||
               incDec->isKind(ParseNodeKind::PostDecrement));

    ParseNode* call = incDec->kid();
    MOZ_ASSERT(call->isKind(ParseNodeKind::Call));
    if (!emitTree(call)) {                              // CALLRESULT
        return false;
    }
    if (!emit1(JSOP_POS)) {                             // N
        return false;
    }

    // The increment/decrement has no side effects, so proceed to throw for
    // invalid assignment target.
    return emitUint16Operand(JSOP_THROWMSG, JSMSG_BAD_LEFTSIDE_OF_ASS);
}

bool
BytecodeEmitter::emitNumberOp(double dval)
{
    int32_t ival;
    if (NumberIsInt32(dval, &ival)) {
        if (ival == 0) {
            return emit1(JSOP_ZERO);
        }
        if (ival == 1) {
            return emit1(JSOP_ONE);
        }
        if ((int)(int8_t)ival == ival) {
            return emit2(JSOP_INT8, uint8_t(int8_t(ival)));
        }

        uint32_t u = uint32_t(ival);
        if (u < JS_BIT(16)) {
            if (!emitUint16Operand(JSOP_UINT16, u)) {
                return false;
            }
        } else if (u < JS_BIT(24)) {
            ptrdiff_t off;
            if (!emitN(JSOP_UINT24, 3, &off)) {
                return false;
            }
            SET_UINT24(code(off), u);
        } else {
            ptrdiff_t off;
            if (!emitN(JSOP_INT32, 4, &off)) {
                return false;
            }
            SET_INT32(code(off), ival);
        }
        return true;
    }

    if (!numberList.append(dval)) {
        return false;
    }

    return emitIndex32(JSOP_DOUBLE, numberList.length() - 1);
}

/*
 * Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047.
 * LLVM is deciding to inline this function which uses a lot of stack space
 * into emitTree which is recursive and uses relatively little stack space.
 */
MOZ_NEVER_INLINE bool
BytecodeEmitter::emitSwitch(SwitchStatement* switchStmt)
{
    LexicalScopeNode& lexical = switchStmt->lexicalForCaseList();
    MOZ_ASSERT(lexical.isKind(ParseNodeKind::LexicalScope));
    ListNode* cases = &lexical.scopeBody()->as<ListNode>();
    MOZ_ASSERT(cases->isKind(ParseNodeKind::StatementList));

    SwitchEmitter se(this);
    if (!se.emitDiscriminant(Some(switchStmt->pn_pos.begin))) {
        return false;
    }
    if (!emitTree(&switchStmt->discriminant())) {
        return false;
    }

    // Enter the scope before pushing the switch BreakableControl since all
    // breaks are under this scope.

    if (!lexical.isEmptyScope()) {
        if (!se.emitLexical(lexical.scopeBindings())) {
            return false;
        }

        // A switch statement may contain hoisted functions inside its
        // cases. The PNX_FUNCDEFS flag is propagated from the STATEMENTLIST
        // bodies of the cases to the case list.
        if (cases->hasTopLevelFunctionDeclarations()) {
            for (ParseNode* item : cases->contents()) {
                CaseClause* caseClause = &item->as<CaseClause>();
                ListNode* statements = caseClause->statementList();
                if (statements->hasTopLevelFunctionDeclarations()) {
                    if (!emitHoistedFunctionsInList(statements)) {
                        return false;
                    }
                }
            }
        }
    } else {
        MOZ_ASSERT(!cases->hasTopLevelFunctionDeclarations());
    }

    SwitchEmitter::TableGenerator tableGen(this);
    uint32_t caseCount = cases->count() - (switchStmt->hasDefault() ? 1 : 0);
    if (caseCount == 0) {
        tableGen.finish(0);
    } else {
        for (ParseNode* item : cases->contents()) {
            CaseClause* caseClause = &item->as<CaseClause>();
            if (caseClause->isDefault()) {
                continue;
            }

            ParseNode* caseValue = caseClause->caseExpression();

            if (caseValue->getKind() != ParseNodeKind::Number) {
                tableGen.setInvalid();
                break;
            }

            int32_t i;
            if (!NumberEqualsInt32(caseValue->as<NumericLiteral>().value(), &i)) {
                tableGen.setInvalid();
                break;
            }

            if (!tableGen.addNumber(i)) {
                return false;
            }
        }

        tableGen.finish(caseCount);
    }

    if (!se.validateCaseCount(caseCount)) {
        return false;
    }

    bool isTableSwitch = tableGen.isValid();
    if (isTableSwitch) {
        if (!se.emitTable(tableGen)) {
            return false;
        }
    } else {
        if (!se.emitCond()) {
            return false;
        }

        // Emit code for evaluating cases and jumping to case statements.
        for (ParseNode* item : cases->contents()) {
            CaseClause* caseClause = &item->as<CaseClause>();
            if (caseClause->isDefault()) {
                continue;
            }

            if (!se.prepareForCaseValue()) {
                return false;
            }

            ParseNode* caseValue = caseClause->caseExpression();
            // If the expression is a literal, suppress line number emission so
            // that debugging works more naturally.
            if (!emitTree(caseValue, ValueUsage::WantValue,
                          caseValue->isLiteral() ? SUPPRESS_LINENOTE : EMIT_LINENOTE))
            {
                return false;
            }

            if (!se.emitCaseJump()) {
                return false;
            }
        }
    }

    // Emit code for each case's statements.
    for (ParseNode* item : cases->contents()) {
        CaseClause* caseClause = &item->as<CaseClause>();
        if (caseClause->isDefault()) {
            if (!se.emitDefaultBody()) {
                return false;
            }
        } else {
            if (isTableSwitch) {
                ParseNode* caseValue = caseClause->caseExpression();
                MOZ_ASSERT(caseValue->isKind(ParseNodeKind::Number));

                NumericLiteral* literal = &caseValue->as<NumericLiteral>();
#ifdef DEBUG
                // Use NumberEqualsInt32 here because switches compare using
                // strict equality, which will equate -0 and +0.  In contrast
                // NumberIsInt32 would return false for -0.
                int32_t v;
                MOZ_ASSERT(mozilla::NumberEqualsInt32(literal->value(), &v));
#endif
                int32_t i = int32_t(literal->value());

                if (!se.emitCaseBody(i, tableGen)) {
                    return false;
                }
            } else {
                if (!se.emitCaseBody()) {
                    return false;
                }
            }
        }

        if (!emitTree(caseClause->statementList())) {
            return false;
        }
    }

    if (!se.emitEnd()) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::isRunOnceLambda()
{
    // The run once lambda flags set by the parser are approximate, and we look
    // at properties of the function itself before deciding to emit a function
    // as a run once lambda.

    if (!(parent && parent->emittingRunOnceLambda) &&
        (emitterMode != LazyFunction || !lazyScript->treatAsRunOnce()))
    {
        return false;
    }

    FunctionBox* funbox = sc->asFunctionBox();
    return !funbox->argumentsHasLocalBinding() &&
           !funbox->isGenerator() &&
           !funbox->isAsync() &&
           !funbox->function()->explicitName();
}

bool
BytecodeEmitter::allocateResumeIndex(ptrdiff_t offset, uint32_t* resumeIndex)
{
    static constexpr uint32_t MaxResumeIndex = JS_BITMASK(24);

    static_assert(MaxResumeIndex < uint32_t(GeneratorObject::RESUME_INDEX_CLOSING),
                  "resumeIndex should not include magic GeneratorObject resumeIndex values");

    *resumeIndex = resumeOffsetList.length();
    if (*resumeIndex > MaxResumeIndex) {
        reportError(nullptr, JSMSG_TOO_MANY_RESUME_INDEXES);
        return false;
    }

    return resumeOffsetList.append(offset);
}

bool
BytecodeEmitter::allocateResumeIndexRange(mozilla::Span<ptrdiff_t> offsets,
                                          uint32_t* firstResumeIndex)
{
    *firstResumeIndex = 0;

    for (size_t i = 0, len = offsets.size(); i < len; i++) {
        uint32_t resumeIndex;
        if (!allocateResumeIndex(offsets[i], &resumeIndex)) {
            return false;
        }
        if (i == 0) {
            *firstResumeIndex = resumeIndex;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitYieldOp(JSOp op)
{
    if (op == JSOP_FINALYIELDRVAL) {
        return emit1(JSOP_FINALYIELDRVAL);
    }

    MOZ_ASSERT(op == JSOP_INITIALYIELD || op == JSOP_YIELD || op == JSOP_AWAIT);

    ptrdiff_t off;
    if (!emitN(op, 3, &off)) {
        return false;
    }

    if (op == JSOP_INITIALYIELD || op == JSOP_YIELD) {
        numYields++;
    }

    uint32_t resumeIndex;
    if (!allocateResumeIndex(offset(), &resumeIndex)) {
        return false;
    }

    SET_RESUMEINDEX(code(off), resumeIndex);

    return emit1(JSOP_DEBUGAFTERYIELD);
}

bool
BytecodeEmitter::emitSetThis(BinaryNode* setThisNode)
{
    // ParseNodeKind::SetThis is used to update |this| after a super() call
    // in a derived class constructor.

    MOZ_ASSERT(setThisNode->isKind(ParseNodeKind::SetThis));
    MOZ_ASSERT(setThisNode->left()->isKind(ParseNodeKind::Name));

    RootedAtom name(cx, setThisNode->left()->as<NameNode>().name());

    // The 'this' binding is not lexical, but due to super() semantics this
    // initialization needs to be treated as a lexical one.
    NameLocation loc = lookupName(name);
    NameLocation lexicalLoc;
    if (loc.kind() == NameLocation::Kind::FrameSlot) {
        lexicalLoc = NameLocation::FrameSlot(BindingKind::Let, loc.frameSlot());
    } else if (loc.kind() == NameLocation::Kind::EnvironmentCoordinate) {
        EnvironmentCoordinate coord = loc.environmentCoordinate();
        uint8_t hops = AssertedCast<uint8_t>(coord.hops());
        lexicalLoc = NameLocation::EnvironmentCoordinate(BindingKind::Let, hops, coord.slot());
    } else {
        MOZ_ASSERT(loc.kind() == NameLocation::Kind::Dynamic);
        lexicalLoc = loc;
    }

    NameOpEmitter noe(this, name, lexicalLoc, NameOpEmitter::Kind::Initialize);
    if (!noe.prepareForRhs()) {                           //
        return false;
    }

    // Emit the new |this| value.
    if (!emitTree(setThisNode->right()))                  // NEWTHIS
        return false;

    // Get the original |this| and throw if we already initialized
    // it. Do *not* use the NameLocation argument, as that's the special
    // lexical location below to deal with super() semantics.
    if (!emitGetName(name)) {                             // NEWTHIS THIS
        return false;
    }
    if (!emit1(JSOP_CHECKTHISREINIT)) {                   // NEWTHIS THIS
        return false;
    }
    if (!emit1(JSOP_POP)) {                               // NEWTHIS
        return false;
    }
    if (!noe.emitAssignment()) {                          // NEWTHIS
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitScript(ParseNode* body)
{
    AutoFrontendTraceLog traceLog(cx, TraceLogger_BytecodeEmission, parser->errorReporter(), body);

    setScriptStartOffsetIfUnset(body->pn_pos);

    TDZCheckCache tdzCache(this);
    EmitterScope emitterScope(this);
    if (sc->isGlobalContext()) {
        switchToPrologue();
        if (!emitterScope.enterGlobal(this, sc->asGlobalContext())) {
            return false;
        }
        switchToMain();
    } else if (sc->isEvalContext()) {
        switchToPrologue();
        if (!emitterScope.enterEval(this, sc->asEvalContext())) {
            return false;
        }
        switchToMain();
    } else {
        MOZ_ASSERT(sc->isModuleContext());
        if (!emitterScope.enterModule(this, sc->asModuleContext())) {
            return false;
        }
    }

    setFunctionBodyEndPos(body->pn_pos);

    if (sc->isEvalContext() && !sc->strict() &&
        body->is<LexicalScopeNode>() && !body->as<LexicalScopeNode>().isEmptyScope())
    {
        // Sloppy eval scripts may need to emit DEFFUNs in the prologue. If there is
        // an immediately enclosed lexical scope, we need to enter the lexical
        // scope in the prologue for the DEFFUNs to pick up the right
        // environment chain.
        EmitterScope lexicalEmitterScope(this);
        LexicalScopeNode* scope = &body->as<LexicalScopeNode>();

        switchToPrologue();
        if (!lexicalEmitterScope.enterLexical(this, ScopeKind::Lexical, scope->scopeBindings())) {
            return false;
        }
        switchToMain();

        if (!emitLexicalScopeBody(scope->scopeBody())) {
            return false;
        }

        if (!lexicalEmitterScope.leave(this)) {
            return false;
        }
    } else {
        if (!emitTree(body)) {
            return false;
        }
    }

    if (!updateSourceCoordNotes(body->pn_pos.end)) {
        return false;
    }

    if (!emit1(JSOP_RETRVAL)) {
        return false;
    }

    if (!emitterScope.leave(this)) {
        return false;
    }

    if (!NameFunctions(cx, body)) {
        return false;
    }

    if (!JSScript::fullyInitFromEmitter(cx, script, this)) {
        return false;
    }

    tellDebuggerAboutCompiledScript(cx);

    return true;
}

bool
BytecodeEmitter::emitFunctionScript(CodeNode* funNode, TopLevelFunction isTopLevel)
{
    MOZ_ASSERT(funNode->isKind(ParseNodeKind::Function));
    ParseNode* body = funNode->body();
    MOZ_ASSERT(body->isKind(ParseNodeKind::ParamsBody));
    FunctionBox* funbox = sc->asFunctionBox();
    AutoFrontendTraceLog traceLog(cx, TraceLogger_BytecodeEmission, parser->errorReporter(), funbox);

    setScriptStartOffsetIfUnset(body->pn_pos);

    // The ordering of these EmitterScopes is important. The named lambda
    // scope needs to enclose the function scope needs to enclose the extra
    // var scope.

    Maybe<EmitterScope> namedLambdaEmitterScope;
    if (funbox->namedLambdaBindings()) {
        namedLambdaEmitterScope.emplace(this);
        if (!namedLambdaEmitterScope->enterNamedLambda(this, funbox)) {
            return false;
        }
    }

    /*
     * Emit a prologue for run-once scripts which will deoptimize JIT code
     * if the script ends up running multiple times via foo.caller related
     * shenanigans.
     *
     * Also mark the script so that initializers created within it may be
     * given more precise types.
     */
    if (isRunOnceLambda()) {
        script->setTreatAsRunOnce();
        MOZ_ASSERT(!script->hasRunOnce());

        switchToPrologue();
        if (!emit1(JSOP_RUNONCE)) {
            return false;
        }
        switchToMain();
    }

    setFunctionBodyEndPos(body->pn_pos);
    if (!emitTree(body)) {
        return false;
    }

    if (!updateSourceCoordNotes(body->pn_pos.end)) {
        return false;
    }

    // Always end the script with a JSOP_RETRVAL. Some other parts of the
    // codebase depend on this opcode,
    // e.g. InterpreterRegs::setToEndOfScript.
    if (!emit1(JSOP_RETRVAL)) {
        return false;
    }

    if (namedLambdaEmitterScope) {
        if (!namedLambdaEmitterScope->leave(this)) {
            return false;
        }
        namedLambdaEmitterScope.reset();
    }

    if (isTopLevel == TopLevelFunction::Yes) {
        if (!NameFunctions(cx, funNode)) {
            return false;
        }
    }

    if (!JSScript::fullyInitFromEmitter(cx, script, this)) {
        return false;
    }

    tellDebuggerAboutCompiledScript(cx);

    return true;
}

bool
BytecodeEmitter::emitDestructuringLHSRef(ParseNode* target, size_t* emitted)
{
    *emitted = 0;

    if (target->isKind(ParseNodeKind::Spread)) {
        target = target->as<UnaryNode>().kid();
    } else if (target->isKind(ParseNodeKind::Assign)) {
        target = target->as<AssignmentNode>().left();
    }

    // No need to recur into ParseNodeKind::Array and
    // ParseNodeKind::Object subpatterns here, since
    // emitSetOrInitializeDestructuring does the recursion when
    // setting or initializing value.  Getting reference doesn't recur.
    if (target->isKind(ParseNodeKind::Name) ||
        target->isKind(ParseNodeKind::Array) ||
        target->isKind(ParseNodeKind::Object))
    {
        return true;
    }

#ifdef DEBUG
    int depth = stackDepth;
#endif

    switch (target->getKind()) {
      case ParseNodeKind::Dot: {
        PropertyAccess* prop = &target->as<PropertyAccess>();
        bool isSuper = prop->isSuper();
        PropOpEmitter poe(this,
                          PropOpEmitter::Kind::SimpleAssignment,
                          isSuper
                          ? PropOpEmitter::ObjKind::Super
                          : PropOpEmitter::ObjKind::Other);
        if (!poe.prepareForObj()) {
            return false;
        }
        if (isSuper) {
            UnaryNode* base = &prop->expression().as<UnaryNode>();
            if (!emitGetThisForSuperBase(base)) {         // THIS SUPERBASE
                return false;
            }
            // SUPERBASE is pushed onto THIS in poe.prepareForRhs below.
            *emitted = 2;
        } else {
            if (!emitTree(&prop->expression())) {         // OBJ
                return false;
            }
            *emitted = 1;
        }
        if (!poe.prepareForRhs()) {                       // [Super]
            //                                            // THIS SUPERBASE
            //                                            // [Other]
            //                                            // OBJ
            return false;
        }
        break;
      }

      case ParseNodeKind::Elem: {
        PropertyByValue* elem = &target->as<PropertyByValue>();
        bool isSuper = elem->isSuper();
        ElemOpEmitter eoe(this,
                          ElemOpEmitter::Kind::SimpleAssignment,
                          isSuper
                          ? ElemOpEmitter::ObjKind::Super
                          : ElemOpEmitter::ObjKind::Other);
        if (!emitElemObjAndKey(elem, isSuper, eoe)) {     // [Super]
            //                                            // THIS KEY
            //                                            // [Other]
            //                                            // OBJ KEY
            return false;
        }
        if (isSuper) {
            // SUPERBASE is pushed onto KEY in eoe.prepareForRhs below.
            *emitted = 3;
        } else {
            *emitted = 2;
        }
        if (!eoe.prepareForRhs()) {                       // [Super]
            //                                            // THIS KEY SUPERBASE
            //                                            // [Other]
            //                                            // OBJ KEY
            return false;
        }
        break;
      }

      case ParseNodeKind::Call:
        MOZ_ASSERT_UNREACHABLE("Parser::reportIfNotValidSimpleAssignmentTarget "
                               "rejects function calls as assignment "
                               "targets in destructuring assignments");
        break;

      default:
        MOZ_CRASH("emitDestructuringLHSRef: bad lhs kind");
    }

    MOZ_ASSERT(stackDepth == depth + int(*emitted));

    return true;
}

bool
BytecodeEmitter::emitSetOrInitializeDestructuring(ParseNode* target, DestructuringFlavor flav)
{
    // Now emit the lvalue opcode sequence. If the lvalue is a nested
    // destructuring initialiser-form, call ourselves to handle it, then pop
    // the matched value. Otherwise emit an lvalue bytecode sequence followed
    // by an assignment op.
    if (target->isKind(ParseNodeKind::Spread)) {
        target = target->as<UnaryNode>().kid();
    } else if (target->isKind(ParseNodeKind::Assign)) {
        target = target->as<AssignmentNode>().left();
    }
    if (target->isKind(ParseNodeKind::Array) || target->isKind(ParseNodeKind::Object)) {
        if (!emitDestructuringOps(&target->as<ListNode>(), flav)) {
            return false;
        }
        // Per its post-condition, emitDestructuringOps has left the
        // to-be-destructured value on top of the stack.
        if (!emit1(JSOP_POP)) {
            return false;
        }
    } else {
        switch (target->getKind()) {
          case ParseNodeKind::Name: {
            RootedAtom name(cx, target->as<NameNode>().name());
            NameLocation loc;
            NameOpEmitter::Kind kind;
            switch (flav) {
              case DestructuringDeclaration:
                loc = lookupName(name);
                kind = NameOpEmitter::Kind::Initialize;
                break;
              case DestructuringFormalParameterInVarScope: {
                // If there's an parameter expression var scope, the
                // destructuring declaration needs to initialize the name in
                // the function scope. The innermost scope is the var scope,
                // and its enclosing scope is the function scope.
                EmitterScope* funScope = innermostEmitterScope()->enclosingInFrame();
                loc = *locationOfNameBoundInScope(name, funScope);
                kind = NameOpEmitter::Kind::Initialize;
                break;
              }

              case DestructuringAssignment:
                loc = lookupName(name);
                kind = NameOpEmitter::Kind::SimpleAssignment;
                break;
            }

            NameOpEmitter noe(this, name, loc, kind);
            if (!noe.prepareForRhs()) {                   // V ENV?
                return false;
            }
            if (noe.emittedBindOp()) {
                // This is like ordinary assignment, but with one difference.
                //
                // In `a = b`, we first determine a binding for `a` (using
                // JSOP_BINDNAME or JSOP_BINDGNAME), then we evaluate `b`, then
                // a JSOP_SETNAME instruction.
                //
                // In `[a] = [b]`, per spec, `b` is evaluated first, then we
                // determine a binding for `a`. Then we need to do assignment--
                // but the operands are on the stack in the wrong order for
                // JSOP_SETPROP, so we have to add a JSOP_SWAP.
                //
                // In the cases where we are emitting a name op, emit a swap
                // because of this.
                if (!emit1(JSOP_SWAP)) {                  // ENV V
                    return false;
                }
            } else {
                // In cases of emitting a frame slot or environment slot,
                // nothing needs be done.
            }
            if (!noe.emitAssignment()) {                  // V
                return false;
            }

            break;
          }

          case ParseNodeKind::Dot: {
            // The reference is already pushed by emitDestructuringLHSRef.
            //                                            // [Super]
            //                                            // THIS SUPERBASE VAL
            //                                            // [Other]
            //                                            // OBJ VAL
            PropertyAccess* prop = &target->as<PropertyAccess>();
            // TODO(khyperia): Implement private field access.
            bool isSuper = prop->isSuper();
            PropOpEmitter poe(this,
                              PropOpEmitter::Kind::SimpleAssignment,
                              isSuper
                              ? PropOpEmitter::ObjKind::Super
                              : PropOpEmitter::ObjKind::Other);
            if (!poe.skipObjAndRhs()) {
                return false;
            }
            if (!poe.emitAssignment(prop->key().atom())) {
                return false;                             // VAL
            }
            break;
          }

          case ParseNodeKind::Elem: {
            // The reference is already pushed by emitDestructuringLHSRef.
            //                                            // [Super]
            //                                            // THIS KEY SUPERBASE VAL
            //                                            // [Other]
            //                                            // OBJ KEY VAL
            PropertyByValue* elem = &target->as<PropertyByValue>();
            bool isSuper = elem->isSuper();
            ElemOpEmitter eoe(this,
                              ElemOpEmitter::Kind::SimpleAssignment,
                              isSuper
                              ? ElemOpEmitter::ObjKind::Super
                              : ElemOpEmitter::ObjKind::Other);
            if (!eoe.skipObjAndKeyAndRhs()) {
                return false;
            }
            if (!eoe.emitAssignment()) {                  // VAL
                return false;
            }
            break;
          }

          case ParseNodeKind::Call:
            MOZ_ASSERT_UNREACHABLE("Parser::reportIfNotValidSimpleAssignmentTarget "
                                   "rejects function calls as assignment "
                                   "targets in destructuring assignments");
            break;

          default:
            MOZ_CRASH("emitSetOrInitializeDestructuring: bad lhs kind");
        }

        // Pop the assigned value.
        if (!emit1(JSOP_POP)) {                           // !STACK EMPTY!
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitIteratorNext(const Maybe<uint32_t>& callSourceCoordOffset,
                                  IteratorKind iterKind /* = IteratorKind::Sync */,
                                  bool allowSelfHosted /* = false */)
{
    MOZ_ASSERT(allowSelfHosted || emitterMode != BytecodeEmitter::SelfHosting,
               ".next() iteration is prohibited in self-hosted code because it "
               "can run user-modifiable iteration code");

    MOZ_ASSERT(this->stackDepth >= 2);                    // ... NEXT ITER

    if (!emitCall(JSOP_CALL, 0, callSourceCoordOffset)) { // ... RESULT
        return false;
    }

    if (iterKind == IteratorKind::Async) {
        if (!emitAwaitInInnermostScope()) {               // ... RESULT
            return false;
        }
    }

    if (!emitCheckIsObj(CheckIsObjectKind::IteratorNext)) { // ... RESULT
        return false;
    }
    checkTypeSet(JSOP_CALL);
    return true;
}

bool
BytecodeEmitter::emitPushNotUndefinedOrNull()
{
    MOZ_ASSERT(this->stackDepth > 0);                     // V

    if (!emit1(JSOP_DUP)) {                               // V V
        return false;
    }
    if (!emit1(JSOP_UNDEFINED)) {                         // V V UNDEFINED
        return false;
    }
    if (!emit1(JSOP_STRICTNE)) {                          // V ?NEQL
        return false;
    }

    JumpList undefinedOrNullJump;
    if (!emitJump(JSOP_AND, &undefinedOrNullJump)) {      // V ?NEQL
        return false;
    }

    if (!emit1(JSOP_POP)) {                               // V
        return false;
    }
    if (!emit1(JSOP_DUP)) {                               // V V
        return false;
    }
    if (!emit1(JSOP_NULL)) {                              // V V NULL
        return false;
    }
    if (!emit1(JSOP_STRICTNE)) {                          // V ?NEQL
        return false;
    }

    if (!emitJumpTargetAndPatch(undefinedOrNullJump)) {   // V NOT-UNDEF-OR-NULL
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitIteratorCloseInScope(EmitterScope& currentScope,
                                          IteratorKind iterKind /* = IteratorKind::Sync */,
                                          CompletionKind completionKind /* = CompletionKind::Normal */,
                                          bool allowSelfHosted /* = false */)
{
    MOZ_ASSERT(allowSelfHosted || emitterMode != BytecodeEmitter::SelfHosting,
               ".close() on iterators is prohibited in self-hosted code because it "
               "can run user-modifiable iteration code");

    // Generate inline logic corresponding to IteratorClose (ES 7.4.6).
    //
    // Callers need to ensure that the iterator object is at the top of the
    // stack.

    if (!emit1(JSOP_DUP)) {                               // ... ITER ITER
        return false;
    }

    // Step 3.
    //
    // Get the "return" method.
    if (!emitAtomOp(cx->names().return_, JSOP_CALLPROP)) {  // ... ITER RET
        return false;
    }

    // Step 4.
    //
    // Do nothing if "return" is undefined or null.
    InternalIfEmitter ifReturnMethodIsDefined(this);
    if (!emitPushNotUndefinedOrNull()) {                  // ... ITER RET NOT-UNDEF-OR-NULL
        return false;
    }

    if (!ifReturnMethodIsDefined.emitThenElse()) {        // ... ITER RET
        return false;
    }

    if (completionKind == CompletionKind::Throw) {
        // 7.4.6 IteratorClose ( iterator, completion )
        //   ...
        //   3. Let return be ? GetMethod(iterator, "return").
        //   4. If return is undefined, return Completion(completion).
        //   5. Let innerResult be Call(return, iterator, « »).
        //   6. If completion.[[Type]] is throw, return Completion(completion).
        //   7. If innerResult.[[Type]] is throw, return
        //      Completion(innerResult).
        //
        // For CompletionKind::Normal case, JSOP_CALL for step 5 checks if RET
        // is callable, and throws if not.  Since step 6 doesn't match and
        // error handling in step 3 and step 7 can be merged.
        //
        // For CompletionKind::Throw case, an error thrown by JSOP_CALL for
        // step 5 is ignored by try-catch.  So we should check if RET is
        // callable here, outside of try-catch, and the throw immediately if
        // not.
        CheckIsCallableKind kind = CheckIsCallableKind::IteratorReturn;
        if (!emitCheckIsCallable(kind)) {                 // ... ITER RET
            return false;
        }
    }

    // Steps 5, 8.
    //
    // Call "return" if it is not undefined or null, and check that it returns
    // an Object.
    if (!emit1(JSOP_SWAP)) {                              // ... RET ITER
        return false;
    }

    Maybe<TryEmitter> tryCatch;

    if (completionKind == CompletionKind::Throw) {
        tryCatch.emplace(this, TryEmitter::Kind::TryCatch, TryEmitter::ControlKind::NonSyntactic);

        // Mutate stack to balance stack for try-catch.
        if (!emit1(JSOP_UNDEFINED)) {                     // ... RET ITER UNDEF
            return false;
        }
        if (!tryCatch->emitTry()) {                       // ... RET ITER UNDEF
            return false;
        }
        if (!emitDupAt(2)) {                              // ... RET ITER UNDEF RET
            return false;
        }
        if (!emitDupAt(2)) {                              // ... RET ITER UNDEF RET ITER
            return false;
        }
    }

    if (!emitCall(JSOP_CALL, 0)) {                        // ... ... RESULT
        return false;
    }
    checkTypeSet(JSOP_CALL);

    if (iterKind == IteratorKind::Async) {
        if (completionKind != CompletionKind::Throw) {
            // Await clobbers rval, so save the current rval.
            if (!emit1(JSOP_GETRVAL)) {                   // ... ... RESULT RVAL
                return false;
            }
            if (!emit1(JSOP_SWAP)) {                      // ... ... RVAL RESULT
                return false;
            }
        }
        if (!emitAwaitInScope(currentScope)) {            // ... ... RVAL? RESULT
            return false;
        }
    }

    if (completionKind == CompletionKind::Throw) {
        if (!emit1(JSOP_SWAP)) {                          // ... RET ITER RESULT UNDEF
            return false;
        }
        if (!emit1(JSOP_POP)) {                           // ... RET ITER RESULT
            return false;
        }

        if (!tryCatch->emitCatch()) {                     // ... RET ITER RESULT
            return false;
        }

        // Just ignore the exception thrown by call and await.
        if (!emit1(JSOP_EXCEPTION)) {                     // ... RET ITER RESULT EXC
            return false;
        }
        if (!emit1(JSOP_POP)) {                           // ... RET ITER RESULT
            return false;
        }

        if (!tryCatch->emitEnd()) {                       // ... RET ITER RESULT
            return false;
        }

        // Restore stack.
        if (!emit2(JSOP_UNPICK, 2)) {                     // ... RESULT RET ITER
            return false;
        }
        if (!emitPopN(2)) {                               // ... RESULT
            return false;
        }
    } else {
        if (!emitCheckIsObj(CheckIsObjectKind::IteratorReturn)) { // ... RVAL? RESULT
            return false;
        }

        if (iterKind == IteratorKind::Async) {
            if (!emit1(JSOP_SWAP)) {                      // ... RESULT RVAL
                return false;
            }
            if (!emit1(JSOP_SETRVAL)) {                   // ... RESULT
                return false;
            }
        }
    }

    if (!ifReturnMethodIsDefined.emitElse()) {            // ... ITER RET
        return false;
    }

    if (!emit1(JSOP_POP)) {                               // ... ITER
        return false;
    }

    if (!ifReturnMethodIsDefined.emitEnd()) {
        return false;
    }

    return emit1(JSOP_POP);                               // ...
}

template <typename InnerEmitter>
bool
BytecodeEmitter::wrapWithDestructuringIteratorCloseTryNote(int32_t iterDepth, InnerEmitter emitter)
{
    MOZ_ASSERT(this->stackDepth >= iterDepth);

    // Pad a nop at the beginning of the bytecode covered by the trynote so
    // that when unwinding environments, we may unwind to the scope
    // corresponding to the pc *before* the start, in case the first bytecode
    // emitted by |emitter| is the start of an inner scope. See comment above
    // UnwindEnvironmentToTryPc.
    if (!emit1(JSOP_TRY_DESTRUCTURING_ITERCLOSE)) {
        return false;
    }

    ptrdiff_t start = offset();
    if (!emitter(this)) {
        return false;
    }
    ptrdiff_t end = offset();
    if (start != end) {
        return addTryNote(JSTRY_DESTRUCTURING_ITERCLOSE, iterDepth, start, end);
    }
    return true;
}

bool
BytecodeEmitter::emitDefault(ParseNode* defaultExpr, ParseNode* pattern)
{
    IfEmitter ifUndefined(this);
    if (!ifUndefined.emitIf(Nothing())) {
        return false;
    }

    if (!emit1(JSOP_DUP)) {                               // VALUE VALUE
        return false;
    }
    if (!emit1(JSOP_UNDEFINED)) {                         // VALUE VALUE UNDEFINED
        return false;
    }
    if (!emit1(JSOP_STRICTEQ)) {                          // VALUE EQL?
        return false;
    }

    if (!ifUndefined.emitThen()) {                        // VALUE
        return false;
    }

    if (!emit1(JSOP_POP)) {                               //
        return false;
    }
    if (!emitInitializer(defaultExpr, pattern)) {         // DEFAULTVALUE
        return false;
    }

    if (!ifUndefined.emitEnd()) {                         // VALUE/DEFAULTVALUE
        return false;
    }
    return true;
}

bool
BytecodeEmitter::setOrEmitSetFunName(ParseNode* maybeFun, HandleAtom name)
{
    MOZ_ASSERT(maybeFun->isDirectRHSAnonFunction());

    if (maybeFun->isKind(ParseNodeKind::Function)) {
        // Function doesn't have 'name' property at this point.
        // Set function's name at compile time.
        JSFunction* fun = maybeFun->as<CodeNode>().funbox()->function();

        // The inferred name may already be set if this function is an
        // interpreted lazy function and we OOM'ed after we set the inferred
        // name the first time.
        if (fun->hasInferredName()) {
            MOZ_ASSERT(fun->isInterpretedLazy());
            MOZ_ASSERT(fun->inferredName() == name);

            return true;
        }

        fun->setInferredName(name);
        return true;
    }

    MOZ_ASSERT(maybeFun->isKind(ParseNodeKind::Class));

    uint32_t nameIndex;
    if (!makeAtomIndex(name, &nameIndex)) {
        return false;
    }
    if (!emitIndexOp(JSOP_STRING, nameIndex)) { // FUN NAME
        return false;
    }
    uint8_t kind = uint8_t(FunctionPrefixKind::None);
    if (!emit2(JSOP_SETFUNNAME, kind)) {        // FUN
        return false;
    }
    return true;
}

bool
BytecodeEmitter::emitInitializer(ParseNode* initializer, ParseNode* pattern)
{
    if (!emitTree(initializer)) {
        return false;
    }

    if (initializer->isDirectRHSAnonFunction()) {
        MOZ_ASSERT(!pattern->isInParens());
        RootedAtom name(cx, pattern->as<NameNode>().name());
        if (!setOrEmitSetFunName(initializer, name)) {
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitDestructuringOpsArray(ListNode* pattern, DestructuringFlavor flav)
{
    MOZ_ASSERT(pattern->isKind(ParseNodeKind::Array));
    MOZ_ASSERT(this->stackDepth != 0);

    // Here's pseudo code for |let [a, b, , c=y, ...d] = x;|
    //
    // Lines that are annotated "covered by trynote" mean that upon throwing
    // an exception, IteratorClose is called on iter only if done is false.
    //
    //   let x, y;
    //   let a, b, c, d;
    //   let iter, next, lref, result, done, value; // stack values
    //
    //   iter = x[Symbol.iterator]();
    //   next = iter.next;
    //
    //   // ==== emitted by loop for a ====
    //   lref = GetReference(a);              // covered by trynote
    //
    //   result = Call(next, iter);
    //   done = result.done;
    //
    //   if (done)
    //     value = undefined;
    //   else
    //     value = result.value;
    //
    //   SetOrInitialize(lref, value);        // covered by trynote
    //
    //   // ==== emitted by loop for b ====
    //   lref = GetReference(b);              // covered by trynote
    //
    //   if (done) {
    //     value = undefined;
    //   } else {
    //     result = Call(next, iter);
    //     done = result.done;
    //     if (done)
    //       value = undefined;
    //     else
    //       value = result.value;
    //   }
    //
    //   SetOrInitialize(lref, value);        // covered by trynote
    //
    //   // ==== emitted by loop for elision ====
    //   if (done) {
    //     value = undefined;
    //   } else {
    //     result = Call(next, iter);
    //     done = result.done;
    //     if (done)
    //       value = undefined;
    //     else
    //       value = result.value;
    //   }
    //
    //   // ==== emitted by loop for c ====
    //   lref = GetReference(c);              // covered by trynote
    //
    //   if (done) {
    //     value = undefined;
    //   } else {
    //     result = Call(next, iter);
    //     done = result.done;
    //     if (done)
    //       value = undefined;
    //     else
    //       value = result.value;
    //   }
    //
    //   if (value === undefined)
    //     value = y;                         // covered by trynote
    //
    //   SetOrInitialize(lref, value);        // covered by trynote
    //
    //   // ==== emitted by loop for d ====
    //   lref = GetReference(d);              // covered by trynote
    //
    //   if (done)
    //     value = [];
    //   else
    //     value = [...iter];
    //
    //   SetOrInitialize(lref, value);        // covered by trynote
    //
    //   // === emitted after loop ===
    //   if (!done)
    //      IteratorClose(iter);

    // Use an iterator to destructure the RHS, instead of index lookup. We
    // must leave the *original* value on the stack.
    if (!emit1(JSOP_DUP)) {                                       // ... OBJ OBJ
        return false;
    }
    if (!emitIterator()) {                                        // ... OBJ NEXT ITER
        return false;
    }

    // For an empty pattern [], call IteratorClose unconditionally. Nothing
    // else needs to be done.
    if (!pattern->head()) {
        if (!emit1(JSOP_SWAP)) {                                  // ... OBJ ITER NEXT
            return false;
        }
        if (!emit1(JSOP_POP)) {                                   // ... OBJ ITER
            return false;
        }

        return emitIteratorCloseInInnermostScope();               // ... OBJ
    }

    // Push an initial FALSE value for DONE.
    if (!emit1(JSOP_FALSE)) {                                     // ... OBJ NEXT ITER FALSE
        return false;
    }

    // JSTRY_DESTRUCTURING_ITERCLOSE expects the iterator and the done value
    // to be the second to top and the top of the stack, respectively.
    // IteratorClose is called upon exception only if done is false.
    int32_t tryNoteDepth = stackDepth;

    for (ParseNode* member : pattern->contents()) {
        bool isFirst = member == pattern->head();
        DebugOnly<bool> hasNext = !!member->pn_next;

        size_t emitted = 0;

        // Spec requires LHS reference to be evaluated first.
        ParseNode* lhsPattern = member;
        if (lhsPattern->isKind(ParseNodeKind::Assign)) {
            lhsPattern = lhsPattern->as<AssignmentNode>().left();
        }

        bool isElision = lhsPattern->isKind(ParseNodeKind::Elision);
        if (!isElision) {
            auto emitLHSRef = [lhsPattern, &emitted](BytecodeEmitter* bce) {
                return bce->emitDestructuringLHSRef(lhsPattern, &emitted); // ... OBJ NEXT ITER DONE *LREF
            };
            if (!wrapWithDestructuringIteratorCloseTryNote(tryNoteDepth, emitLHSRef)) {
                return false;
            }
        }

        // Pick the DONE value to the top of the stack.
        if (emitted) {
            if (!emit2(JSOP_PICK, emitted)) {                     // ... OBJ NEXT ITER *LREF DONE
                return false;
            }
        }

        if (isFirst) {
            // If this element is the first, DONE is always FALSE, so pop it.
            //
            // Non-first elements should emit if-else depending on the
            // member pattern, below.
            if (!emit1(JSOP_POP)) {                               // ... OBJ NEXT ITER *LREF
                return false;
            }
        }

        if (member->isKind(ParseNodeKind::Spread)) {
            InternalIfEmitter ifThenElse(this);
            if (!isFirst) {
                // If spread is not the first element of the pattern,
                // iterator can already be completed.
                                                                  // ... OBJ NEXT ITER *LREF DONE
                if (!ifThenElse.emitThenElse()) {                 // ... OBJ NEXT ITER *LREF
                    return false;
                }

                if (!emitUint32Operand(JSOP_NEWARRAY, 0)) {       // ... OBJ NEXT ITER *LREF ARRAY
                    return false;
                }
                if (!ifThenElse.emitElse()) {                     // ... OBJ NEXT ITER *LREF
                    return false;
                }
            }

            // If iterator is not completed, create a new array with the rest
            // of the iterator.
            if (!emitDupAt(emitted + 1)) {                        // ... OBJ NEXT ITER *LREF NEXT
                return false;
            }
            if (!emitDupAt(emitted + 1)) {                        // ... OBJ NEXT ITER *LREF NEXT ITER
                return false;
            }
            if (!emitUint32Operand(JSOP_NEWARRAY, 0)) {           // ... OBJ NEXT ITER *LREF NEXT ITER ARRAY
                return false;
            }
            if (!emitNumberOp(0)) {                               // ... OBJ NEXT ITER *LREF NEXT ITER ARRAY INDEX
                return false;
            }
            if (!emitSpread()) {                                  // ... OBJ NEXT ITER *LREF ARRAY INDEX
                return false;
            }
            if (!emit1(JSOP_POP)) {                               // ... OBJ NEXT ITER *LREF ARRAY
                return false;
            }

            if (!isFirst) {
                if (!ifThenElse.emitEnd()) {
                    return false;
                }
                MOZ_ASSERT(ifThenElse.pushed() == 1);
            }

            // At this point the iterator is done. Unpick a TRUE value for DONE above ITER.
            if (!emit1(JSOP_TRUE)) {                              // ... OBJ NEXT ITER *LREF ARRAY TRUE
                return false;
            }
            if (!emit2(JSOP_UNPICK, emitted + 1)) {               // ... OBJ NEXT ITER TRUE *LREF ARRAY
                return false;
            }

            auto emitAssignment = [member, flav](BytecodeEmitter* bce) {
                return bce->emitSetOrInitializeDestructuring(member, flav); // ... OBJ NEXT ITER TRUE
            };
            if (!wrapWithDestructuringIteratorCloseTryNote(tryNoteDepth, emitAssignment)) {
                return false;
            }

            MOZ_ASSERT(!hasNext);
            break;
        }

        ParseNode* pndefault = nullptr;
        if (member->isKind(ParseNodeKind::Assign)) {
            pndefault = member->as<AssignmentNode>().right();
        }

        MOZ_ASSERT(!member->isKind(ParseNodeKind::Spread));

        InternalIfEmitter ifAlreadyDone(this);
        if (!isFirst) {
                                                                  // ... OBJ NEXT ITER *LREF DONE
            if (!ifAlreadyDone.emitThenElse()) {                  // ... OBJ NEXT ITER *LREF
                return false;
            }

            if (!emit1(JSOP_UNDEFINED)) {                         // ... OBJ NEXT ITER *LREF UNDEF
                return false;
            }
            if (!emit1(JSOP_NOP_DESTRUCTURING)) {                 // ... OBJ NEXT ITER *LREF UNDEF
                return false;
            }

            // The iterator is done. Unpick a TRUE value for DONE above ITER.
            if (!emit1(JSOP_TRUE)) {                              // ... OBJ NEXT ITER *LREF UNDEF TRUE
                return false;
            }
            if (!emit2(JSOP_UNPICK, emitted + 1)) {               // ... OBJ NEXT ITER TRUE *LREF UNDEF
                return false;
            }

            if (!ifAlreadyDone.emitElse()) {                      // ... OBJ NEXT ITER *LREF
                return false;
            }
        }

        if (!emitDupAt(emitted + 1)) {                            // ... OBJ NEXT ITER *LREF NEXT
            return false;
        }
        if (!emitDupAt(emitted + 1)) {                            // ... OBJ NEXT ITER *LREF NEXT ITER
            return false;
        }
        if (!emitIteratorNext(Some(pattern->pn_pos.begin))) {     // ... OBJ NEXT ITER *LREF RESULT
            return false;
        }
        if (!emit1(JSOP_DUP)) {                                   // ... OBJ NEXT ITER *LREF RESULT RESULT
            return false;
        }
        if (!emitAtomOp(cx->names().done, JSOP_GETPROP)) {        // ... OBJ NEXT ITER *LREF RESULT DONE
            return false;
        }

        if (!emit1(JSOP_DUP)) {                                   // ... OBJ NEXT ITER *LREF RESULT DONE DONE
            return false;
        }
        if (!emit2(JSOP_UNPICK, emitted + 2)) {                   // ... OBJ NEXT ITER DONE *LREF RESULT DONE
            return false;
        }

        InternalIfEmitter ifDone(this);
        if (!ifDone.emitThenElse()) {                             // ... OBJ NEXT ITER DONE *LREF RESULT
            return false;
        }

        if (!emit1(JSOP_POP)) {                                   // ... OBJ NEXT ITER DONE *LREF
            return false;
        }
        if (!emit1(JSOP_UNDEFINED)) {                             // ... OBJ NEXT ITER DONE *LREF UNDEF
            return false;
        }
        if (!emit1(JSOP_NOP_DESTRUCTURING)) {                     // ... OBJ NEXT ITER DONE *LREF UNDEF
            return false;
        }

        if (!ifDone.emitElse()) {                                 // ... OBJ NEXT ITER DONE *LREF RESULT
            return false;
        }

        if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) {       // ... OBJ NEXT ITER DONE *LREF VALUE
            return false;
        }

        if (!ifDone.emitEnd()) {
            return false;
        }
        MOZ_ASSERT(ifDone.pushed() == 0);

        if (!isFirst) {
            if (!ifAlreadyDone.emitEnd()) {
                return false;
            }
            MOZ_ASSERT(ifAlreadyDone.pushed() == 2);
        }

        if (pndefault) {
            auto emitDefault = [pndefault, lhsPattern](BytecodeEmitter* bce) {
                return bce->emitDefault(pndefault, lhsPattern);    // ... OBJ NEXT ITER DONE *LREF VALUE
            };

            if (!wrapWithDestructuringIteratorCloseTryNote(tryNoteDepth, emitDefault)) {
                return false;
            }
        }

        if (!isElision) {
            auto emitAssignment = [lhsPattern, flav](BytecodeEmitter* bce) {
                return bce->emitSetOrInitializeDestructuring(lhsPattern, flav); // ... OBJ NEXT ITER DONE
            };

            if (!wrapWithDestructuringIteratorCloseTryNote(tryNoteDepth, emitAssignment)) {
                return false;
            }
        } else {
            if (!emit1(JSOP_POP)) {                               // ... OBJ NEXT ITER DONE
                return false;
            }
        }
    }

    // The last DONE value is on top of the stack. If not DONE, call
    // IteratorClose.
                                                                  // ... OBJ NEXT ITER DONE
    InternalIfEmitter ifDone(this);
    if (!ifDone.emitThenElse()) {                                 // ... OBJ NEXT ITER
        return false;
    }
    if (!emitPopN(2)) {                                           // ... OBJ
        return false;
    }
    if (!ifDone.emitElse()) {                                     // ... OBJ NEXT ITER
        return false;
    }
    if (!emit1(JSOP_SWAP)) {                                      // ... OBJ ITER NEXT
        return false;
    }
    if (!emit1(JSOP_POP)) {                                       // ... OBJ ITER
        return false;
    }
    if (!emitIteratorCloseInInnermostScope()) {                   // ... OBJ
        return false;
    }
    if (!ifDone.emitEnd()) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitComputedPropertyName(UnaryNode* computedPropName)
{
    MOZ_ASSERT(computedPropName->isKind(ParseNodeKind::ComputedName));
    return emitTree(computedPropName->kid()) && emit1(JSOP_TOID);
}

bool
BytecodeEmitter::emitDestructuringOpsObject(ListNode* pattern, DestructuringFlavor flav)
{
    MOZ_ASSERT(pattern->isKind(ParseNodeKind::Object));

    MOZ_ASSERT(this->stackDepth > 0);                             // ... RHS

    if (!emit1(JSOP_CHECKOBJCOERCIBLE)) {                         // ... RHS
        return false;
    }

    bool needsRestPropertyExcludedSet = pattern->count() > 1 &&
                                        pattern->last()->isKind(ParseNodeKind::Spread);
    if (needsRestPropertyExcludedSet) {
        if (!emitDestructuringObjRestExclusionSet(pattern)) {     // ... RHS SET
            return false;
        }

        if (!emit1(JSOP_SWAP)) {                                  // ... SET RHS
            return false;
        }
    }

    for (ParseNode* member : pattern->contents()) {
        ParseNode* subpattern;
        if (member->isKind(ParseNodeKind::MutateProto) ||
            member->isKind(ParseNodeKind::Spread))
        {
            subpattern = member->as<UnaryNode>().kid();
        } else {
            MOZ_ASSERT(member->isKind(ParseNodeKind::Colon) ||
                       member->isKind(ParseNodeKind::Shorthand));
            subpattern = member->as<BinaryNode>().right();
        }

        ParseNode* lhs = subpattern;
        MOZ_ASSERT_IF(member->isKind(ParseNodeKind::Spread),
                      !lhs->isKind(ParseNodeKind::Assign));
        if (lhs->isKind(ParseNodeKind::Assign)) {
            lhs = lhs->as<AssignmentNode>().left();
        }

        size_t emitted;
        if (!emitDestructuringLHSRef(lhs, &emitted)) {            // ... *SET RHS *LREF
            return false;
        }

        // Duplicate the value being destructured to use as a reference base.
        if (!emitDupAt(emitted)) {                                // ... *SET RHS *LREF RHS
            return false;
        }

        if (member->isKind(ParseNodeKind::Spread)) {
            if (!updateSourceCoordNotes(member->pn_pos.begin)) {
                return false;
            }

            if (!emitNewInit()) {                                 // ... *SET RHS *LREF RHS TARGET
                return false;
            }
            if (!emit1(JSOP_DUP)) {                               // ... *SET RHS *LREF RHS TARGET TARGET
                return false;
            }
            if (!emit2(JSOP_PICK, 2)) {                           // ... *SET RHS *LREF TARGET TARGET RHS
                return false;
            }

            if (needsRestPropertyExcludedSet) {
                if (!emit2(JSOP_PICK, emitted + 4)) {             // ... RHS *LREF TARGET TARGET RHS SET
                    return false;
                }
            }

            CopyOption option = needsRestPropertyExcludedSet
                                ? CopyOption::Filtered
                                : CopyOption::Unfiltered;
            if (!emitCopyDataProperties(option)) {                // ... RHS *LREF TARGET
                return false;
            }

            // Destructure TARGET per this member's lhs.
            if (!emitSetOrInitializeDestructuring(lhs, flav)) {   // ... RHS
                return false;
            }

            MOZ_ASSERT(member == pattern->last(), "Rest property is always last");
            break;
        }

        // Now push the property name currently being matched, which is the
        // current property name "label" on the left of a colon in the object
        // initialiser.
        bool needsGetElem = true;

        if (member->isKind(ParseNodeKind::MutateProto)) {
            if (!emitAtomOp(cx->names().proto, JSOP_GETPROP)) {   // ... *SET RHS *LREF PROP
                return false;
            }
            needsGetElem = false;
        } else {
            MOZ_ASSERT(member->isKind(ParseNodeKind::Colon) ||
                       member->isKind(ParseNodeKind::Shorthand));

            ParseNode* key = member->as<BinaryNode>().left();
            if (key->isKind(ParseNodeKind::Number)) {
                if (!emitNumberOp(key->as<NumericLiteral>().value())) {
                    return false;                                 // ... *SET RHS *LREF RHS KEY
                }
            } else if (key->isKind(ParseNodeKind::ObjectPropertyName) ||
                       key->isKind(ParseNodeKind::String))
            {
                if (!emitAtomOp(key->as<NameNode>().atom(), JSOP_GETPROP)) {
                    return false;                                 // ... *SET RHS *LREF PROP
                }
                needsGetElem = false;
            } else {
                if (!emitComputedPropertyName(&key->as<UnaryNode>())) {
                    return false;                                 // ... *SET RHS *LREF RHS KEY
                }

                // Add the computed property key to the exclusion set.
                if (needsRestPropertyExcludedSet) {
                    if (!emitDupAt(emitted + 3)) {                // ... SET RHS *LREF RHS KEY SET
                        return false;
                    }
                    if (!emitDupAt(1)) {                          // ... SET RHS *LREF RHS KEY SET KEY
                        return false;
                    }
                    if (!emit1(JSOP_UNDEFINED)) {                 // ... SET RHS *LREF RHS KEY SET KEY UNDEFINED
                        return false;
                    }
                    if (!emit1(JSOP_INITELEM)) {                  // ... SET RHS *LREF RHS KEY SET
                        return false;
                    }
                    if (!emit1(JSOP_POP)) {                       // ... SET RHS *LREF RHS KEY
                        return false;
                    }
                }
            }
        }

        // Get the property value if not done already.
        if (needsGetElem && !emitElemOpBase(JSOP_GETELEM)) {      // ... *SET RHS *LREF PROP
            return false;
        }

        if (subpattern->isKind(ParseNodeKind::Assign)) {
            if (!emitDefault(subpattern->as<AssignmentNode>().right(), lhs)) {
                return false;                                     // ... *SET RHS *LREF VALUE
            }
        }

        // Destructure PROP per this member's lhs.
        if (!emitSetOrInitializeDestructuring(subpattern, flav)) {  // ... *SET RHS
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitDestructuringObjRestExclusionSet(ListNode* pattern)
{
    MOZ_ASSERT(pattern->isKind(ParseNodeKind::Object));
    MOZ_ASSERT(pattern->last()->isKind(ParseNodeKind::Spread));

    ptrdiff_t offset = this->offset();
    if (!emitNewInit()) {
        return false;
    }

    // Try to construct the shape of the object as we go, so we can emit a
    // JSOP_NEWOBJECT with the final shape instead.
    // In the case of computed property names and indices, we cannot fix the
    // shape at bytecode compile time. When the shape cannot be determined,
    // |obj| is nulled out.

    // No need to do any guessing for the object kind, since we know the upper
    // bound of how many properties we plan to have.
    gc::AllocKind kind = gc::GetGCObjectKind(pattern->count() - 1);
    RootedPlainObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx, kind, TenuredObject));
    if (!obj) {
        return false;
    }

    RootedAtom pnatom(cx);
    for (ParseNode* member : pattern->contents()) {
        if (member->isKind(ParseNodeKind::Spread)) {
            break;
        }

        bool isIndex = false;
        if (member->isKind(ParseNodeKind::MutateProto)) {
            pnatom.set(cx->names().proto);
        } else {
            ParseNode* key = member->as<BinaryNode>().left();
            if (key->isKind(ParseNodeKind::Number)) {
                if (!emitNumberOp(key->as<NumericLiteral>().value())) {
                    return false;
                }
                isIndex = true;
            } else if (key->isKind(ParseNodeKind::ObjectPropertyName) ||
                       key->isKind(ParseNodeKind::String))
            {
                pnatom.set(key->as<NameNode>().atom());
            } else {
                // Otherwise this is a computed property name which needs to
                // be added dynamically.
                obj.set(nullptr);
                continue;
            }
        }

        // Initialize elements with |undefined|.
        if (!emit1(JSOP_UNDEFINED)) {
            return false;
        }

        if (isIndex) {
            obj.set(nullptr);
            if (!emit1(JSOP_INITELEM)) {
                return false;
            }
        } else {
            uint32_t index;
            if (!makeAtomIndex(pnatom, &index)) {
                return false;
            }

            if (obj) {
                MOZ_ASSERT(!obj->inDictionaryMode());
                Rooted<jsid> id(cx, AtomToId(pnatom));
                if (!NativeDefineDataProperty(cx, obj, id, UndefinedHandleValue, JSPROP_ENUMERATE)) {
                    return false;
                }
                if (obj->inDictionaryMode()) {
                    obj.set(nullptr);
                }
            }

            if (!emitIndex32(JSOP_INITPROP, index)) {
                return false;
            }
        }
    }

    if (obj) {
        // The object survived and has a predictable shape: update the
        // original bytecode.
        if (!replaceNewInitWithNewObject(obj, offset)) {
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitDestructuringOps(ListNode* pattern, DestructuringFlavor flav)
{
    if (pattern->isKind(ParseNodeKind::Array)) {
        return emitDestructuringOpsArray(pattern, flav);
    }
    return emitDestructuringOpsObject(pattern, flav);
}

bool
BytecodeEmitter::emitTemplateString(ListNode* templateString)
{
    bool pushedString = false;

    for (ParseNode* item : templateString->contents()) {
        bool isString = (item->getKind() == ParseNodeKind::String ||
                         item->getKind() == ParseNodeKind::TemplateString);

        // Skip empty strings. These are very common: a template string like
        // `${a}${b}` has three empty strings and without this optimization
        // we'd emit four JSOP_ADD operations instead of just one.
        if (isString && item->as<NameNode>().atom()->empty()) {
            continue;
        }

        if (!isString) {
            // We update source notes before emitting the expression
            if (!updateSourceCoordNotes(item->pn_pos.begin)) {
                return false;
            }
        }

        if (!emitTree(item)) {
            return false;
        }

        if (!isString) {
            // We need to convert the expression to a string
            if (!emit1(JSOP_TOSTRING)) {
                return false;
            }
        }

        if (pushedString) {
            // We've pushed two strings onto the stack. Add them together, leaving just one.
            if (!emit1(JSOP_ADD)) {
                return false;
            }
        } else {
            pushedString = true;
        }
    }

    if (!pushedString) {
        // All strings were empty, this can happen for something like `${""}`.
        // Just push an empty string.
        if (!emitAtomOp(cx->names().empty, JSOP_STRING)) {
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitDeclarationList(ListNode* declList)
{
    MOZ_ASSERT(declList->isOp(JSOP_NOP));

    for (ParseNode* decl : declList->contents()) {
        if (!updateSourceCoordNotes(decl->pn_pos.begin)) {
            return false;
        }

        if (decl->isKind(ParseNodeKind::Assign)) {
            MOZ_ASSERT(decl->isOp(JSOP_NOP));

            AssignmentNode* assignNode = &decl->as<AssignmentNode>();
            ListNode* pattern = &assignNode->left()->as<ListNode>();
            MOZ_ASSERT(pattern->isKind(ParseNodeKind::Array) ||
                       pattern->isKind(ParseNodeKind::Object));

            if (!emitTree(assignNode->right())) {
                return false;
            }

            if (!emitDestructuringOps(pattern, DestructuringDeclaration)) {
                return false;
            }

            if (!emit1(JSOP_POP)) {
                return false;
            }
        } else {
            NameNode* name = &decl->as<NameNode>();
            if (!emitSingleDeclaration(declList, name, name->initializer())) {
                return false;
            }
        }
    }
    return true;
}

bool
BytecodeEmitter::emitSingleDeclaration(ListNode* declList, NameNode* decl,
                                       ParseNode* initializer)
{
    MOZ_ASSERT(decl->isKind(ParseNodeKind::Name));

    // Nothing to do for initializer-less 'var' declarations, as there's no TDZ.
    if (!initializer && declList->isKind(ParseNodeKind::Var)) {
        return true;
    }

    NameOpEmitter noe(this, decl->name(), NameOpEmitter::Kind::Initialize);
    if (!noe.prepareForRhs()) {                           // ENV?
        return false;
    }
    if (!initializer) {
        // Lexical declarations are initialized to undefined without an
        // initializer.
        MOZ_ASSERT(declList->isKind(ParseNodeKind::Let),
                   "var declarations without initializers handled above, "
                   "and const declarations must have initializers");
        if (!emit1(JSOP_UNDEFINED)) {                     // ENV? UNDEF
            return false;
        }
    } else {
        MOZ_ASSERT(initializer);
        if (!emitInitializer(initializer, decl)) {        // ENV? V
            return false;
        }
    }
    if (!noe.emitAssignment()) {                          // V
         return false;
    }
    if (!emit1(JSOP_POP)) {                               //
        return false;
    }

    return true;
}

static bool
EmitAssignmentRhs(BytecodeEmitter* bce, ParseNode* rhs, uint8_t offset)
{
    // If there is a RHS tree, emit the tree.
    if (rhs) {
        return bce->emitTree(rhs);
    }

    // Otherwise the RHS value to assign is already on the stack, i.e., the
    // next enumeration value in a for-in or for-of loop. Depending on how
    // many other values have been pushed on the stack, we need to get the
    // already-pushed RHS value.
    if (offset != 1 && !bce->emit2(JSOP_PICK, offset - 1)) {
        return false;
    }

    return true;
}

static inline JSOp
CompoundAssignmentParseNodeKindToJSOp(ParseNodeKind pnk)
{
    switch (pnk) {
      case ParseNodeKind::Assign:       return JSOP_NOP;
      case ParseNodeKind::AddAssign:    return JSOP_ADD;
      case ParseNodeKind::SubAssign:    return JSOP_SUB;
      case ParseNodeKind::BitOrAssign:  return JSOP_BITOR;
      case ParseNodeKind::BitXorAssign: return JSOP_BITXOR;
      case ParseNodeKind::BitAndAssign: return JSOP_BITAND;
      case ParseNodeKind::LshAssign:    return JSOP_LSH;
      case ParseNodeKind::RshAssign:    return JSOP_RSH;
      case ParseNodeKind::UrshAssign:   return JSOP_URSH;
      case ParseNodeKind::MulAssign:    return JSOP_MUL;
      case ParseNodeKind::DivAssign:    return JSOP_DIV;
      case ParseNodeKind::ModAssign:    return JSOP_MOD;
      case ParseNodeKind::PowAssign:    return JSOP_POW;
      default: MOZ_CRASH("unexpected compound assignment op");
    }
}

bool
BytecodeEmitter::emitAssignment(ParseNode* lhs, JSOp compoundOp, ParseNode* rhs)
{
    bool isCompound = compoundOp != JSOP_NOP;

    // Name assignments are handled separately because choosing ops and when
    // to emit BINDNAME is involved and should avoid duplication.
    if (lhs->isKind(ParseNodeKind::Name)) {
        NameNode* nameNode = &lhs->as<NameNode>();
        RootedAtom name(cx, nameNode->name());
        NameOpEmitter noe(this,
                          name,
                          isCompound
                          ? NameOpEmitter::Kind::CompoundAssignment
                          : NameOpEmitter::Kind::SimpleAssignment);
        if (!noe.prepareForRhs()) {                       // ENV? VAL?
            return false;
        }

        // Emit the RHS. If we emitted a BIND[G]NAME, then the scope is on
        // the top of the stack and we need to pick the right RHS value.
        uint8_t offset = noe.emittedBindOp() ? 2 : 1;
        if (!EmitAssignmentRhs(this, rhs, offset)) {      // ENV? VAL? RHS
            return false;
        }
        if (rhs && rhs->isDirectRHSAnonFunction()) {
            MOZ_ASSERT(!nameNode->isInParens());
            MOZ_ASSERT(!isCompound);
            if (!setOrEmitSetFunName(rhs, name)) {         // ENV? VAL? RHS
                return false;
            }
        }

        // Emit the compound assignment op if there is one.
        if (isCompound) {
            if (!emit1(compoundOp)) {                     // ENV? VAL
                return false;
            }
        }
        if (!noe.emitAssignment()) {                      // VAL
            return false;
        }

        return true;
    }

    Maybe<PropOpEmitter> poe;
    Maybe<ElemOpEmitter> eoe;

    // Deal with non-name assignments.
    uint8_t offset = 1;

    switch (lhs->getKind()) {
      case ParseNodeKind::Dot: {
        PropertyAccess* prop = &lhs->as<PropertyAccess>();
        bool isSuper = prop->isSuper();
        poe.emplace(this,
                    isCompound
                    ? PropOpEmitter::Kind::CompoundAssignment
                    : PropOpEmitter::Kind::SimpleAssignment,
                    isSuper
                    ? PropOpEmitter::ObjKind::Super
                    : PropOpEmitter::ObjKind::Other);
        if (!poe->prepareForObj()) {
            return false;
        }
        if (isSuper) {
            UnaryNode* base = &prop->expression().as<UnaryNode>();
            if (!emitGetThisForSuperBase(base)) {         // THIS SUPERBASE
                return false;
            }
            // SUPERBASE is pushed onto THIS later in poe->emitGet below.
            offset += 2;
        } else {
            if (!emitTree(&prop->expression())) {         // OBJ
                return false;
            }
            offset += 1;
        }
        break;
      }
      case ParseNodeKind::Elem: {
        PropertyByValue* elem = &lhs->as<PropertyByValue>();
        bool isSuper = elem->isSuper();
        eoe.emplace(this,
                    isCompound
                    ? ElemOpEmitter::Kind::CompoundAssignment
                    : ElemOpEmitter::Kind::SimpleAssignment,
                    isSuper
                    ? ElemOpEmitter::ObjKind::Super
                    : ElemOpEmitter::ObjKind::Other);
        if (!emitElemObjAndKey(elem, isSuper, *eoe)) {    // [Super]
            //                                            // THIS KEY
            //                                            // [Other]
            //                                            // OBJ KEY
            return false;
        }
        if (isSuper) {
            // SUPERBASE is pushed onto KEY in eoe->emitGet below.
            offset += 3;
        } else {
            offset += 2;
        }
        break;
      }
      case ParseNodeKind::Array:
      case ParseNodeKind::Object:
        break;
      case ParseNodeKind::Call:
        if (!emitTree(lhs)) {
            return false;
        }

        // Assignment to function calls is forbidden, but we have to make the
        // call first.  Now we can throw.
        if (!emitUint16Operand(JSOP_THROWMSG, JSMSG_BAD_LEFTSIDE_OF_ASS)) {
            return false;
        }

        // Rebalance the stack to placate stack-depth assertions.
        if (!emit1(JSOP_POP)) {
            return false;
        }
        break;
      default:
        MOZ_ASSERT(0);
    }

    if (isCompound) {
        MOZ_ASSERT(rhs);
        switch (lhs->getKind()) {
          case ParseNodeKind::Dot: {
            PropertyAccess* prop = &lhs->as<PropertyAccess>();
            // TODO(khyperia): Implement private field access.
            if (!poe->emitGet(prop->key().atom())) {      // [Super]
                //                                        // THIS SUPERBASE PROP
                //                                        // [Other]
                //                                        // OBJ PROP
                return false;
            }
            break;
          }
          case ParseNodeKind::Elem: {
            if (!eoe->emitGet()) {                        // KEY THIS OBJ ELEM
                return false;
            }
            break;
          }
          case ParseNodeKind::Call:
            // We just emitted a JSOP_THROWMSG and popped the call's return
            // value.  Push a random value to make sure the stack depth is
            // correct.
            if (!emit1(JSOP_NULL)) {
                return false;
            }
            break;
          default:;
        }
    }

    switch (lhs->getKind()) {
      case ParseNodeKind::Dot:
        if (!poe->prepareForRhs()) {                      // [Simple,Super]
            //                                            // THIS SUPERBASE
            //                                            // [Simple,Other]
            //                                            // OBJ
            //                                            // [Compound,Super]
            //                                            // THIS SUPERBASE PROP
            //                                            // [Compound,Other]
            //                                            // OBJ PROP
            return false;
        }
        break;
      case ParseNodeKind::Elem:
        if (!eoe->prepareForRhs()) {                      // [Simple,Super]
            //                                            // THIS KEY SUPERBASE
            //                                            // [Simple,Other]
            //                                            // OBJ KEY
            //                                            // [Compound,Super]
            //                                            // THIS KEY SUPERBASE ELEM
            //                                            // [Compound,Other]
            //                                            // OBJ KEY ELEM
            return false;
        }
        break;
      default:
        break;
    }

    if (!EmitAssignmentRhs(this, rhs, offset)) {          // ... VAL? RHS
        return false;
    }

    /* If += etc., emit the binary operator with a source note. */
    if (isCompound) {
        if (!newSrcNote(SRC_ASSIGNOP)) {
            return false;
        }
        if (!emit1(compoundOp)) {                         // ... VAL
            return false;
        }
    }

    /* Finally, emit the specialized assignment bytecode. */
    switch (lhs->getKind()) {
      case ParseNodeKind::Dot: {
        PropertyAccess* prop = &lhs->as<PropertyAccess>();
        // TODO(khyperia): Implement private field access.
        if (!poe->emitAssignment(prop->key().atom())) {   // VAL
            return false;
        }

        poe.reset();
        break;
      }
      case ParseNodeKind::Call:
        // We threw above, so nothing to do here.
        break;
      case ParseNodeKind::Elem: {
        if (!eoe->emitAssignment()) {                     // VAL
            return false;
        }

        eoe.reset();
        break;
      }
      case ParseNodeKind::Array:
      case ParseNodeKind::Object:
        if (!emitDestructuringOps(&lhs->as<ListNode>(), DestructuringAssignment)) {
            return false;
        }
        break;
      default:
        MOZ_ASSERT(0);
    }
    return true;
}

bool
ParseNode::getConstantValue(JSContext* cx, AllowConstantObjects allowObjects,
                            MutableHandleValue vp, Value* compare, size_t ncompare,
                            NewObjectKind newKind)
{
    MOZ_ASSERT(newKind == TenuredObject || newKind == SingletonObject);

    switch (getKind()) {
      case ParseNodeKind::Number:
        vp.setNumber(as<NumericLiteral>().value());
        return true;
      case ParseNodeKind::TemplateString:
      case ParseNodeKind::String:
        vp.setString(as<NameNode>().atom());
        return true;
      case ParseNodeKind::True:
        vp.setBoolean(true);
        return true;
      case ParseNodeKind::False:
        vp.setBoolean(false);
        return true;
      case ParseNodeKind::Null:
        vp.setNull();
        return true;
      case ParseNodeKind::RawUndefined:
        vp.setUndefined();
        return true;
      case ParseNodeKind::CallSiteObj:
      case ParseNodeKind::Array: {
        unsigned count;
        ParseNode* pn;

        if (allowObjects == DontAllowObjects) {
            vp.setMagic(JS_GENERIC_MAGIC);
            return true;
        }

        ObjectGroup::NewArrayKind arrayKind = ObjectGroup::NewArrayKind::Normal;
        if (allowObjects == ForCopyOnWriteArray) {
            arrayKind = ObjectGroup::NewArrayKind::CopyOnWrite;
            allowObjects = DontAllowObjects;
        }

        if (getKind() == ParseNodeKind::CallSiteObj) {
            count = as<CallSiteNode>().count() - 1;
            pn = as<CallSiteNode>().head()->pn_next;
        } else {
            MOZ_ASSERT(!as<ListNode>().hasNonConstInitializer());
            count = as<ListNode>().count();
            pn = as<ListNode>().head();
        }

        AutoValueVector values(cx);
        if (!values.appendN(MagicValue(JS_ELEMENTS_HOLE), count)) {
            return false;
        }
        size_t idx;
        for (idx = 0; pn; idx++, pn = pn->pn_next) {
            if (!pn->getConstantValue(cx, allowObjects, values[idx], values.begin(), idx)) {
                return false;
            }
            if (values[idx].isMagic(JS_GENERIC_MAGIC)) {
                vp.setMagic(JS_GENERIC_MAGIC);
                return true;
            }
        }
        MOZ_ASSERT(idx == count);

        ArrayObject* obj = ObjectGroup::newArrayObject(cx, values.begin(), values.length(),
                                                       newKind, arrayKind);
        if (!obj) {
            return false;
        }

        if (!CombineArrayElementTypes(cx, obj, compare, ncompare)) {
            return false;
        }

        vp.setObject(*obj);
        return true;
      }
      case ParseNodeKind::Object: {
        MOZ_ASSERT(!as<ListNode>().hasNonConstInitializer());

        if (allowObjects == DontAllowObjects) {
            vp.setMagic(JS_GENERIC_MAGIC);
            return true;
        }
        MOZ_ASSERT(allowObjects == AllowObjects);

        Rooted<IdValueVector> properties(cx, IdValueVector(cx));

        RootedValue value(cx), idvalue(cx);
        for (ParseNode* item : as<ListNode>().contents()) {
            // MutateProto and Spread, both are unary, cannot appear here.
            BinaryNode* prop = &item->as<BinaryNode>();
            if (!prop->right()->getConstantValue(cx, allowObjects, &value)) {
                return false;
            }
            if (value.isMagic(JS_GENERIC_MAGIC)) {
                vp.setMagic(JS_GENERIC_MAGIC);
                return true;
            }

            ParseNode* key = prop->left();
            if (key->isKind(ParseNodeKind::Number)) {
                idvalue = NumberValue(key->as<NumericLiteral>().value());
            } else {
                MOZ_ASSERT(key->isKind(ParseNodeKind::ObjectPropertyName) ||
                           key->isKind(ParseNodeKind::String));
                MOZ_ASSERT(key->as<NameNode>().atom() != cx->names().proto);
                idvalue = StringValue(key->as<NameNode>().atom());
            }

            RootedId id(cx);
            if (!ValueToId<CanGC>(cx, idvalue, &id)) {
                return false;
            }

            if (!properties.append(IdValuePair(id, value))) {
                return false;
            }
        }

        JSObject* obj = ObjectGroup::newPlainObject(cx, properties.begin(), properties.length(),
                                                    newKind);
        if (!obj) {
            return false;
        }

        if (!CombinePlainObjectPropertyTypes(cx, obj, compare, ncompare)) {
            return false;
        }

        vp.setObject(*obj);
        return true;
      }
      default:
        MOZ_CRASH("Unexpected node");
    }
    return false;
}

bool
BytecodeEmitter::emitSingletonInitialiser(ListNode* objOrArray)
{
    NewObjectKind newKind =
        (objOrArray->getKind() == ParseNodeKind::Object) ? SingletonObject : TenuredObject;

    RootedValue value(cx);
    if (!objOrArray->getConstantValue(cx, ParseNode::AllowObjects, &value, nullptr, 0, newKind)) {
        return false;
    }

    MOZ_ASSERT_IF(newKind == SingletonObject, value.toObject().isSingleton());

    ObjectBox* objbox = parser->newObjectBox(&value.toObject());
    if (!objbox) {
        return false;
    }

    return emitObjectOp(objbox, JSOP_OBJECT);
}

bool
BytecodeEmitter::emitCallSiteObject(CallSiteNode* callSiteObj)
{
    RootedValue value(cx);
    if (!callSiteObj->getConstantValue(cx, ParseNode::AllowObjects, &value)) {
        return false;
    }

    MOZ_ASSERT(value.isObject());

    ObjectBox* objbox1 = parser->newObjectBox(&value.toObject());
    if (!objbox1) {
        return false;
    }

    if (!callSiteObj->getRawArrayValue(cx, &value)) {
        return false;
    }

    MOZ_ASSERT(value.isObject());

    ObjectBox* objbox2 = parser->newObjectBox(&value.toObject());
    if (!objbox2) {
        return false;
    }

    return emitObjectPairOp(objbox1, objbox2, JSOP_CALLSITEOBJ);
}

namespace {

class EmitLevelManager
{
    BytecodeEmitter* bce;
  public:
    explicit EmitLevelManager(BytecodeEmitter* bce) : bce(bce) { bce->emitLevel++; }
    ~EmitLevelManager() { bce->emitLevel--; }
};

} /* anonymous namespace */

bool
BytecodeEmitter::emitCatch(BinaryNode* catchClause)
{
    // We must be nested under a try-finally statement.
    MOZ_ASSERT(innermostNestableControl->is<TryFinallyControl>());

    /* Pick up the pending exception and bind it to the catch variable. */
    if (!emit1(JSOP_EXCEPTION)) {
        return false;
    }

    ParseNode* param = catchClause->left();
    if (!param) {
        // Catch parameter was omitted; just discard the exception.
        if (!emit1(JSOP_POP)) {
            return false;
        }
    } else {
        switch (param->getKind()) {
          case ParseNodeKind::Array:
          case ParseNodeKind::Object:
            if (!emitDestructuringOps(&param->as<ListNode>(), DestructuringDeclaration)) {
                return false;
            }
            if (!emit1(JSOP_POP)) {
                return false;
            }
            break;

          case ParseNodeKind::Name:
            if (!emitLexicalInitialization(&param->as<NameNode>())) {
                return false;
            }
            if (!emit1(JSOP_POP)) {
                return false;
            }
            break;

          default:
            MOZ_ASSERT(0);
        }
    }

    /* Emit the catch body. */
    return emitTree(catchClause->right());
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See the
// comment on EmitSwitch.
MOZ_NEVER_INLINE bool
BytecodeEmitter::emitTry(TryNode* tryNode)
{
    LexicalScopeNode* catchScope = tryNode->catchScope();
    ParseNode* finallyNode = tryNode->finallyBlock();

    TryEmitter::Kind kind;
    if (catchScope) {
        if (finallyNode) {
            kind = TryEmitter::Kind::TryCatchFinally;
        } else {
            kind = TryEmitter::Kind::TryCatch;
        }
    } else {
        MOZ_ASSERT(finallyNode);
        kind = TryEmitter::Kind::TryFinally;
    }
    TryEmitter tryCatch(this, kind, TryEmitter::ControlKind::Syntactic);

    if (!tryCatch.emitTry()) {
        return false;
    }

    if (!emitTree(tryNode->body())) {
        return false;
    }

    // If this try has a catch block, emit it.
    if (catchScope) {
        // The emitted code for a catch block looks like:
        //
        // [pushlexicalenv]             only if any local aliased
        // exception
        // setlocal 0; pop              assign or possibly destructure exception
        // < catch block contents >
        // debugleaveblock
        // [poplexicalenv]              only if any local aliased
        // if there is a finally block:
        //   gosub <finally>
        //   goto <after finally>
        if (!tryCatch.emitCatch()) {
            return false;
        }

        // Emit the lexical scope and catch body.
        if (!emitTree(catchScope)) {
            return false;
        }
    }

    // Emit the finally handler, if there is one.
    if (finallyNode) {
        if (!tryCatch.emitFinally(Some(finallyNode->pn_pos.begin))) {
            return false;
        }

        if (!emitTree(finallyNode)) {
            return false;
        }
    }

    if (!tryCatch.emitEnd()) {
        return false;
    }

    return true;
}

MOZ_MUST_USE bool
BytecodeEmitter::emitGoSub(JumpList* jump)
{
    if (!emit1(JSOP_FALSE)) {
        return false;
    }

    ptrdiff_t off;
    if (!emitN(JSOP_RESUMEINDEX, 3, &off)) {
        return false;
    }

    if (!emitJump(JSOP_GOSUB, jump)) {
        return false;
    }

    uint32_t resumeIndex;
    if (!allocateResumeIndex(offset(), &resumeIndex)) {
        return false;
    }

    SET_RESUMEINDEX(code(off), resumeIndex);
    return true;
}

bool
BytecodeEmitter::emitIf(TernaryNode* ifNode)
{
    IfEmitter ifThenElse(this);

    if (!ifThenElse.emitIf(Some(ifNode->pn_pos.begin))) {
        return false;
    }

  if_again:
    /* Emit code for the condition before pushing stmtInfo. */
    if (!emitTree(ifNode->kid1())) {
        return false;
    }

    ParseNode* elseNode = ifNode->kid3();
    if (elseNode) {
        if (!ifThenElse.emitThenElse()) {
            return false;
        }
    } else {
        if (!ifThenElse.emitThen()) {
            return false;
        }
    }

    /* Emit code for the then part. */
    if (!emitTree(ifNode->kid2())) {
        return false;
    }

    if (elseNode) {
        if (elseNode->isKind(ParseNodeKind::If)) {
            ifNode = &elseNode->as<TernaryNode>();

            if (!ifThenElse.emitElseIf(Some(ifNode->pn_pos.begin))) {
                return false;
            }

            goto if_again;
        }

        if (!ifThenElse.emitElse()) {
            return false;
        }

        /* Emit code for the else part. */
        if (!emitTree(elseNode)) {
            return false;
        }
    }

    if (!ifThenElse.emitEnd()) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitHoistedFunctionsInList(ListNode* stmtList)
{
    MOZ_ASSERT(stmtList->hasTopLevelFunctionDeclarations());

    for (ParseNode* stmt : stmtList->contents()) {
        ParseNode* maybeFun = stmt;

        if (!sc->strict()) {
            while (maybeFun->isKind(ParseNodeKind::Label)) {
                maybeFun = maybeFun->as<LabeledStatement>().statement();
            }
        }

        if (maybeFun->isKind(ParseNodeKind::Function) &&
            maybeFun->as<CodeNode>().functionIsHoisted())
        {
            if (!emitTree(maybeFun)) {
                return false;
            }
        }
    }

    return true;
}

bool
BytecodeEmitter::emitLexicalScopeBody(ParseNode* body, EmitLineNumberNote emitLineNote)
{
    if (body->isKind(ParseNodeKind::StatementList) &&
        body->as<ListNode>().hasTopLevelFunctionDeclarations())
    {
        // This block contains function statements whose definitions are
        // hoisted to the top of the block. Emit these as a separate pass
        // before the rest of the block.
        if (!emitHoistedFunctionsInList(&body->as<ListNode>())) {
            return false;
        }
    }

    // Line notes were updated by emitLexicalScope.
    return emitTree(body, ValueUsage::WantValue, emitLineNote);
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See
// the comment on emitSwitch.
MOZ_NEVER_INLINE bool
BytecodeEmitter::emitLexicalScope(LexicalScopeNode* lexicalScope)
{
    TDZCheckCache tdzCache(this);

    ParseNode* body = lexicalScope->scopeBody();
    if (lexicalScope->isEmptyScope()) {
        return emitLexicalScopeBody(body);
    }

    // We are about to emit some bytecode for what the spec calls "declaration
    // instantiation". Assign these instructions to the opening `{` of the
    // block. (Using the location of each declaration we're instantiating is
    // too weird when stepping in the debugger.)
    if (!ParseNodeRequiresSpecialLineNumberNotes(body)) {
        if (!updateSourceCoordNotes(lexicalScope->pn_pos.begin)) {
            return false;
        }
    }

    EmitterScope emitterScope(this);
    ScopeKind kind;
    if (body->isKind(ParseNodeKind::Catch)) {
        BinaryNode* catchNode = &body->as<BinaryNode>();
        kind = (!catchNode->left() || catchNode->left()->isKind(ParseNodeKind::Name))
               ? ScopeKind::SimpleCatch
               : ScopeKind::Catch;
    } else {
        kind = ScopeKind::Lexical;
    }

    if (!emitterScope.enterLexical(this, kind, lexicalScope->scopeBindings())) {
        return false;
    }

    if (body->isKind(ParseNodeKind::For)) {
        // for loops need to emit {FRESHEN,RECREATE}LEXICALENV if there are
        // lexical declarations in the head. Signal this by passing a
        // non-nullptr lexical scope.
        if (!emitFor(&body->as<ForNode>(), &emitterScope)) {
            return false;
        }
    } else {
        if (!emitLexicalScopeBody(body, SUPPRESS_LINENOTE)) {
            return false;
        }
    }

    return emitterScope.leave(this);
}

bool
BytecodeEmitter::emitWith(BinaryNode* withNode)
{
    // Ensure that the column of the 'with' is set properly.
    if (!updateSourceCoordNotes(withNode->pn_pos.begin)) {
        return false;
    }

    if (!emitTree(withNode->left())) {
        return false;
    }

    EmitterScope emitterScope(this);
    if (!emitterScope.enterWith(this)) {
        return false;
    }

    if (!emitTree(withNode->right())) {
        return false;
    }

    return emitterScope.leave(this);
}

bool
BytecodeEmitter::emitCopyDataProperties(CopyOption option)
{
    DebugOnly<int32_t> depth = this->stackDepth;

    uint32_t argc;
    if (option == CopyOption::Filtered) {
        MOZ_ASSERT(depth > 2);                 // TARGET SOURCE SET
        argc = 3;

        if (!emitAtomOp(cx->names().CopyDataProperties,
                        JSOP_GETINTRINSIC))    // TARGET SOURCE SET COPYDATAPROPERTIES
        {
            return false;
        }
    } else {
        MOZ_ASSERT(depth > 1);                 // TARGET SOURCE
        argc = 2;

        if (!emitAtomOp(cx->names().CopyDataPropertiesUnfiltered,
                        JSOP_GETINTRINSIC))    // TARGET SOURCE COPYDATAPROPERTIES
        {
            return false;
        }
    }

    if (!emit1(JSOP_UNDEFINED)) {              // TARGET SOURCE *SET COPYDATAPROPERTIES UNDEFINED
        return false;
    }
    if (!emit2(JSOP_PICK, argc + 1)) {         // SOURCE *SET COPYDATAPROPERTIES UNDEFINED TARGET
        return false;
    }
    if (!emit2(JSOP_PICK, argc + 1)) {         // *SET COPYDATAPROPERTIES UNDEFINED TARGET SOURCE
        return false;
    }
    if (option == CopyOption::Filtered) {
        if (!emit2(JSOP_PICK, argc + 1)) {     // COPYDATAPROPERTIES UNDEFINED TARGET SOURCE SET
            return false;
        }
    }
    if (!emitCall(JSOP_CALL_IGNORES_RV, argc)) { // IGNORED
        return false;
    }
    checkTypeSet(JSOP_CALL_IGNORES_RV);

    if (!emit1(JSOP_POP)) {                    // -
        return false;
    }

    MOZ_ASSERT(depth - int(argc) == this->stackDepth);
    return true;
}

bool
BytecodeEmitter::emitIterator()
{
    // Convert iterable to iterator.
    if (!emit1(JSOP_DUP)) {                                       // OBJ OBJ
        return false;
    }
    if (!emit2(JSOP_SYMBOL, uint8_t(JS::SymbolCode::iterator))) { // OBJ OBJ @@ITERATOR
        return false;
    }
    if (!emitElemOpBase(JSOP_CALLELEM)) {                         // OBJ ITERFN
        return false;
    }
    if (!emit1(JSOP_SWAP)) {                                      // ITERFN OBJ
        return false;
    }
    if (!emitCall(JSOP_CALLITER, 0)) {                            // ITER
        return false;
    }
    checkTypeSet(JSOP_CALLITER);
    if (!emitCheckIsObj(CheckIsObjectKind::GetIterator)) {        // ITER
        return false;
    }
    if (!emit1(JSOP_DUP)) {                                       // ITER ITER
        return false;
    }
    if (!emitAtomOp(cx->names().next, JSOP_GETPROP)) {            // ITER NEXT
        return false;
    }
    if (!emit1(JSOP_SWAP)) {                                      // NEXT ITER
        return false;
    }
    return true;
}

bool
BytecodeEmitter::emitAsyncIterator()
{
    // Convert iterable to iterator.
    if (!emit1(JSOP_DUP)) {                                       // OBJ OBJ
        return false;
    }
    if (!emit2(JSOP_SYMBOL, uint8_t(JS::SymbolCode::asyncIterator))) { // OBJ OBJ @@ASYNCITERATOR
        return false;
    }
    if (!emitElemOpBase(JSOP_CALLELEM)) {                         // OBJ ITERFN
        return false;
    }

    InternalIfEmitter ifAsyncIterIsUndefined(this);
    if (!emitPushNotUndefinedOrNull()) {                          // OBJ ITERFN !UNDEF-OR-NULL
        return false;
    }
    if (!emit1(JSOP_NOT)) {                                       // OBJ ITERFN UNDEF-OR-NULL
        return false;
    }
    if (!ifAsyncIterIsUndefined.emitThenElse()) {                 // OBJ ITERFN
        return false;
    }

    if (!emit1(JSOP_POP)) {                                       // OBJ
        return false;
    }
    if (!emit1(JSOP_DUP)) {                                       // OBJ OBJ
        return false;
    }
    if (!emit2(JSOP_SYMBOL, uint8_t(JS::SymbolCode::iterator))) { // OBJ OBJ @@ITERATOR
        return false;
    }
    if (!emitElemOpBase(JSOP_CALLELEM)) {                         // OBJ ITERFN
        return false;
    }
    if (!emit1(JSOP_SWAP)) {                                      // ITERFN OBJ
        return false;
    }
    if (!emitCall(JSOP_CALLITER, 0)) {                            // ITER
        return false;
    }
    checkTypeSet(JSOP_CALLITER);
    if (!emitCheckIsObj(CheckIsObjectKind::GetIterator)) {        // ITER
        return false;
    }

    if (!emit1(JSOP_DUP)) {                                       // ITER ITER
        return false;
    }
    if (!emitAtomOp(cx->names().next, JSOP_GETPROP)) {            // ITER SYNCNEXT
        return false;
    }

    if (!emit1(JSOP_TOASYNCITER)) {                               // ITER
        return false;
    }

    if (!ifAsyncIterIsUndefined.emitElse()) {                     // OBJ ITERFN
        return false;
    }

    if (!emit1(JSOP_SWAP)) {                                      // ITERFN OBJ
        return false;
    }
    if (!emitCall(JSOP_CALLITER, 0)) {                            // ITER
        return false;
    }
    checkTypeSet(JSOP_CALLITER);
    if (!emitCheckIsObj(CheckIsObjectKind::GetIterator)) {        // ITER
        return false;
    }

    if (!ifAsyncIterIsUndefined.emitEnd()) {                      // ITER
        return false;
    }

    if (!emit1(JSOP_DUP)) {                                       // ITER ITER
        return false;
    }
    if (!emitAtomOp(cx->names().next, JSOP_GETPROP)) {            // ITER NEXT
        return false;
    }
    if (!emit1(JSOP_SWAP)) {                                      // NEXT ITER
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitSpread(bool allowSelfHosted)
{
    LoopControl loopInfo(this, StatementKind::Spread);

    // Jump down to the loop condition to minimize overhead assuming at least
    // one iteration, as the other loop forms do.  Annotate so IonMonkey can
    // find the loop-closing jump.
    unsigned noteIndex;
    if (!newSrcNote(SRC_FOR_OF, &noteIndex)) {
        return false;
    }

    // Jump down to the loop condition to minimize overhead, assuming at least
    // one iteration.  (This is also what we do for loops; whether this
    // assumption holds for spreads is an unanswered question.)
    if (!loopInfo.emitEntryJump(this)) {                  // NEXT ITER ARR I (during the goto)
        return false;
    }

    if (!loopInfo.emitLoopHead(this, Nothing())) {        // NEXT ITER ARR I
        return false;
    }

    // When we enter the goto above, we have NEXT ITER ARR I on the stack. But
    // when we reach this point on the loop backedge (if spreading produces at
    // least one value), we've additionally pushed a RESULT iteration value.
    // Increment manually to reflect this.
    this->stackDepth++;

    {
#ifdef DEBUG
        auto loopDepth = this->stackDepth;
#endif

        // Emit code to assign result.value to the iteration variable.
        if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) { // NEXT ITER ARR I VALUE
            return false;
        }
        if (!emit1(JSOP_INITELEM_INC)) {                  // NEXT ITER ARR (I+1)
            return false;
        }

        MOZ_ASSERT(this->stackDepth == loopDepth - 1);

        // Spread operations can't contain |continue|, so don't bother setting loop
        // and enclosing "update" offsets, as we do with for-loops.

        // COME FROM the beginning of the loop to here.
        if (!loopInfo.emitLoopEntry(this, Nothing())) {   // NEXT ITER ARR I
            return false;
        }

        if (!emitDupAt(3)) {                              // NEXT ITER ARR I NEXT
            return false;
        }
        if (!emitDupAt(3)) {                              // NEXT ITER ARR I NEXT ITER
            return false;
        }
        if (!emitIteratorNext(Nothing(), IteratorKind::Sync, allowSelfHosted)) {
            return false;                                 // ITER ARR I RESULT
        }
        if (!emit1(JSOP_DUP)) {                           // NEXT ITER ARR I RESULT RESULT
            return false;
        }
        if (!emitAtomOp(cx->names().done, JSOP_GETPROP)) {  // NEXT ITER ARR I RESULT DONE
            return false;
        }

        if (!loopInfo.emitLoopEnd(this, JSOP_IFEQ)) {     // NEXT ITER ARR I RESULT
            return false;
        }

        MOZ_ASSERT(this->stackDepth == loopDepth);
    }

    // Let Ion know where the closing jump of this loop is.
    if (!setSrcNoteOffset(noteIndex, SrcNote::ForOf::BackJumpOffset,
                          loopInfo.loopEndOffsetFromEntryJump()))
    {
        return false;
    }

    // No breaks or continues should occur in spreads.
    MOZ_ASSERT(loopInfo.breaks.offset == -1);
    MOZ_ASSERT(loopInfo.continues.offset == -1);

    if (!addTryNote(JSTRY_FOR_OF, stackDepth, loopInfo.headOffset(),
                    loopInfo.breakTargetOffset()))
    {
        return false;
    }

    if (!emit2(JSOP_PICK, 4)) {                           // ITER ARR FINAL_INDEX RESULT NEXT
        return false;
    }
    if (!emit2(JSOP_PICK, 4)) {                           // ARR FINAL_INDEX RESULT NEXT ITER
        return false;
    }

    return emitPopN(3);                                   // ARR FINAL_INDEX
}

bool
BytecodeEmitter::emitInitializeForInOrOfTarget(TernaryNode* forHead)
{
    MOZ_ASSERT(forHead->isKind(ParseNodeKind::ForIn) ||
               forHead->isKind(ParseNodeKind::ForOf));

    MOZ_ASSERT(this->stackDepth >= 1,
               "must have a per-iteration value for initializing");

    ParseNode* target = forHead->kid1();
    MOZ_ASSERT(!forHead->kid2());

    // If the for-in/of loop didn't have a variable declaration, per-loop
    // initialization is just assigning the iteration value to a target
    // expression.
    if (!parser->astGenerator().isDeclarationList(target)) {
        return emitAssignment(target, JSOP_NOP, nullptr); // ... ITERVAL
    }

    // Otherwise, per-loop initialization is (possibly) declaration
    // initialization.  If the declaration is a lexical declaration, it must be
    // initialized.  If the declaration is a variable declaration, an
    // assignment to that name (which does *not* necessarily assign to the
    // variable!) must be generated.

    if (!updateSourceCoordNotes(target->pn_pos.begin)) {
        return false;
    }

    MOZ_ASSERT(target->isForLoopDeclaration());
    target = parser->astGenerator().singleBindingFromDeclaration(&target->as<ListNode>());

    if (target->isKind(ParseNodeKind::Name)) {
        NameNode* nameNode = &target->as<NameNode>();
        NameOpEmitter noe(this, nameNode->name(), NameOpEmitter::Kind::Initialize);
        if (!noe.prepareForRhs()) {
            return false;
        }
        if (noe.emittedBindOp()) {
            // Per-iteration initialization in for-in/of loops computes the
            // iteration value *before* initializing.  Thus the initializing
            // value may be buried under a bind-specific value on the stack.
            // Swap it to the top of the stack.
            MOZ_ASSERT(stackDepth >= 2);
            if (!emit1(JSOP_SWAP)) {
                return false;
            }
        } else {
             // In cases of emitting a frame slot or environment slot,
             // nothing needs be done.
            MOZ_ASSERT(stackDepth >= 1);
        }
        if (!noe.emitAssignment()) {
            return false;
        }

        // The caller handles removing the iteration value from the stack.
        return true;
    }

    MOZ_ASSERT(!target->isKind(ParseNodeKind::Assign),
               "for-in/of loop destructuring declarations can't have initializers");

    MOZ_ASSERT(target->isKind(ParseNodeKind::Array) ||
               target->isKind(ParseNodeKind::Object));
    return emitDestructuringOps(&target->as<ListNode>(), DestructuringDeclaration);
}

bool
BytecodeEmitter::emitForOf(ForNode* forOfLoop, const EmitterScope* headLexicalEmitterScope)
{
    MOZ_ASSERT(forOfLoop->isKind(ParseNodeKind::For));

    TernaryNode* forOfHead = forOfLoop->head();
    MOZ_ASSERT(forOfHead->isKind(ParseNodeKind::ForOf));

    unsigned iflags = forOfLoop->iflags();
    IteratorKind iterKind = (iflags & JSITER_FORAWAITOF)
                            ? IteratorKind::Async
                            : IteratorKind::Sync;
    MOZ_ASSERT_IF(iterKind == IteratorKind::Async, sc->asFunctionBox());
    MOZ_ASSERT_IF(iterKind == IteratorKind::Async, sc->asFunctionBox()->isAsync());

    ParseNode* forHeadExpr = forOfHead->kid3();

    // Certain builtins (e.g. Array.from) are implemented in self-hosting
    // as for-of loops.
    bool allowSelfHostedIter = false;
    if (emitterMode == BytecodeEmitter::SelfHosting &&
        forHeadExpr->isKind(ParseNodeKind::Call) &&
        forHeadExpr->as<BinaryNode>().left()->isName(cx->names().allowContentIter)) {
        allowSelfHostedIter = true;
    }

    ForOfEmitter forOf(this, headLexicalEmitterScope, allowSelfHostedIter, iterKind);

    if (!forOf.emitIterated()) {                          //
        return false;
    }

    if (!emitTree(forHeadExpr)) {                         // ITERABLE
        return false;
    }

    if (headLexicalEmitterScope) {
        DebugOnly<ParseNode*> forOfTarget = forOfHead->kid1();
        MOZ_ASSERT(forOfTarget->isKind(ParseNodeKind::Let) ||
                   forOfTarget->isKind(ParseNodeKind::Const));
    }

    if (!forOf.emitInitialize(Some(forOfHead->pn_pos.begin))) {
        return false;                                     // NEXT ITER VALUE
    }

    if (!emitInitializeForInOrOfTarget(forOfHead)) {      // NEXT ITER VALUE
        return false;
    }

    if (!forOf.emitBody()) {                              // NEXT ITER UNDEF
        return false;
    }

    // Perform the loop body.
    ParseNode* forBody = forOfLoop->body();
    if (!emitTree(forBody)) {                             // NEXT ITER UNDEF
        return false;
    }

    if (!forOf.emitEnd(Some(forHeadExpr->pn_pos.begin))) {  //
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitForIn(ForNode* forInLoop, const EmitterScope* headLexicalEmitterScope)
{
    MOZ_ASSERT(forInLoop->isOp(JSOP_ITER));

    TernaryNode* forInHead = forInLoop->head();
    MOZ_ASSERT(forInHead->isKind(ParseNodeKind::ForIn));

    ForInEmitter forIn(this, headLexicalEmitterScope);

    // Annex B: Evaluate the var-initializer expression if present.
    // |for (var i = initializer in expr) { ... }|
    ParseNode* forInTarget = forInHead->kid1();
    if (parser->astGenerator().isDeclarationList(forInTarget)) {
        ParseNode* decl =
            parser->astGenerator().singleBindingFromDeclaration(&forInTarget->as<ListNode>());
        if (decl->isKind(ParseNodeKind::Name)) {
            if (ParseNode* initializer = decl->as<NameNode>().initializer()) {
                MOZ_ASSERT(forInTarget->isKind(ParseNodeKind::Var),
                           "for-in initializers are only permitted for |var| declarations");

                if (!updateSourceCoordNotes(decl->pn_pos.begin)) {
                    return false;
                }

                NameNode* nameNode = &decl->as<NameNode>();
                NameOpEmitter noe(this, nameNode->name(), NameOpEmitter::Kind::Initialize);
                if (!noe.prepareForRhs()) {
                    return false;
                }
                if (!emitInitializer(initializer, decl)) {
                    return false;
                }
                if (!noe.emitAssignment()) {
                    return false;
                }

                // Pop the initializer.
                if (!emit1(JSOP_POP)) {
                    return false;
                }
            }
        }
    }

    if (!forIn.emitIterated()) {                          //
        return false;
    }

    // Evaluate the expression being iterated.
    ParseNode* expr = forInHead->kid3();
    if (!emitTree(expr)) {                                // EXPR
        return false;
    }

    MOZ_ASSERT(forInLoop->iflags() == 0);

    MOZ_ASSERT_IF(headLexicalEmitterScope,
                  forInTarget->isKind(ParseNodeKind::Let) ||
                  forInTarget->isKind(ParseNodeKind::Const));

    if (!forIn.emitInitialize()) {                        // ITER ITERVAL
        return false;
    }

    if (!emitInitializeForInOrOfTarget(forInHead)) {      // ITER ITERVAL
        return false;
    }

    if (!forIn.emitBody()) {                              // ITER ITERVAL
        return false;
    }

    // Perform the loop body.
    ParseNode* forBody = forInLoop->body();
    if (!emitTree(forBody)) {                             // ITER ITERVAL
        return false;
    }

    if (!forIn.emitEnd(Some(forInHead->pn_pos.begin))) {  //
        return false;
    }

    return true;
}

/* C-style `for (init; cond; update) ...` loop. */
bool
BytecodeEmitter::emitCStyleFor(ForNode* forNode, const EmitterScope* headLexicalEmitterScope)
{
    TernaryNode* forHead = forNode->head();
    ParseNode* forBody = forNode->body();
    ParseNode* init = forHead->kid1();
    ParseNode* cond = forHead->kid2();
    ParseNode* update = forHead->kid3();
    bool isLet = init && init->isKind(ParseNodeKind::Let);

    CForEmitter cfor(this, isLet ? headLexicalEmitterScope : nullptr);

    if (!cfor.emitInit(init ? Some(init->pn_pos.begin) : Nothing())) {
        return false;                                     //
    }

    // If the head of this for-loop declared any lexical variables, the parser
    // wrapped this ParseNodeKind::For node in a ParseNodeKind::LexicalScope
    // representing the implicit scope of those variables. By the time we get
    // here, we have already entered that scope. So far, so good.
    if (init) {
        // Emit the `init` clause, whether it's an expression or a variable
        // declaration. (The loop variables were hoisted into an enclosing
        // scope, but we still need to emit code for the initializers.)
        if (init->isForLoopDeclaration()) {
            if (!emitTree(init)) {                        //
                return false;
            }
        } else {
            // 'init' is an expression, not a declaration. emitTree left its
            // value on the stack.
            if (!emitTree(init, ValueUsage::IgnoreValue)) { // VAL
                return false;
            }
            if (!emit1(JSOP_POP)) {                       //
                return false;
            }
        }
    }

    if (!cfor.emitBody(cond ? CForEmitter::Cond::Present : CForEmitter::Cond::Missing,
                       getOffsetForLoop(forBody)))        //
    {
        return false;
    }

    if (!emitTree(forBody)) {                             //
        return false;
    }

    if (!cfor.emitUpdate(update ? CForEmitter::Update::Present : CForEmitter::Update::Missing,
                         update ? Some(update->pn_pos.begin) : Nothing()))
    {                                                     //
        return false;
    }

    // Check for update code to do before the condition (if any).
    if (update) {
        if (!emitTree(update, ValueUsage::IgnoreValue)) { // VAL
            return false;
        }
    }

    if (!cfor.emitCond(Some(forNode->pn_pos.begin),
                       cond ? Some(cond->pn_pos.begin) : Nothing(),
                       Some(forNode->pn_pos.end)))        //
    {
        return false;
    }

    if (cond) {
        if (!emitTree(cond)) {                            // VAL
            return false;
        }
    }

    if (!cfor.emitEnd()) {                                //
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitFor(ForNode* forNode, const EmitterScope* headLexicalEmitterScope)
{
    if (forNode->head()->isKind(ParseNodeKind::ForHead)) {
        return emitCStyleFor(forNode, headLexicalEmitterScope);
    }

    if (!updateLineNumberNotes(forNode->pn_pos.begin)) {
        return false;
    }

    if (forNode->head()->isKind(ParseNodeKind::ForIn)) {
        return emitForIn(forNode, headLexicalEmitterScope);
    }

    MOZ_ASSERT(forNode->head()->isKind(ParseNodeKind::ForOf));
    return emitForOf(forNode, headLexicalEmitterScope);
}

MOZ_NEVER_INLINE bool
BytecodeEmitter::emitFunction(CodeNode* funNode, bool needsProto)
{
    FunctionBox* funbox = funNode->funbox();
    RootedFunction fun(cx, funbox->function());
    RootedAtom name(cx, fun->explicitName());
    MOZ_ASSERT_IF(fun->isInterpretedLazy(), fun->lazyScript());

    // Set the |wasEmitted| flag in the funbox once the function has been
    // emitted. Function definitions that need hoisting to the top of the
    // function will be seen by emitFunction in two places.
    if (funbox->wasEmitted) {
        // Annex B block-scoped functions are hoisted like any other
        // block-scoped function to the top of their scope. When their
        // definitions are seen for the second time, we need to emit the
        // assignment that assigns the function to the outer 'var' binding.
        if (funbox->isAnnexB) {
            // Get the location of the 'var' binding in the body scope. The
            // name must be found, else there is a bug in the Annex B handling
            // in Parser.
            //
            // In sloppy eval contexts, this location is dynamic.
            Maybe<NameLocation> lhsLoc = locationOfNameBoundInScope(name, varEmitterScope);

            // If there are parameter expressions, the var name could be a
            // parameter.
            if (!lhsLoc && sc->isFunctionBox() && sc->asFunctionBox()->hasExtraBodyVarScope()) {
                lhsLoc = locationOfNameBoundInScope(name, varEmitterScope->enclosingInFrame());
            }

            if (!lhsLoc) {
                lhsLoc = Some(NameLocation::DynamicAnnexBVar());
            } else {
                MOZ_ASSERT(lhsLoc->bindingKind() == BindingKind::Var ||
                           lhsLoc->bindingKind() == BindingKind::FormalParameter ||
                           (lhsLoc->bindingKind() == BindingKind::Let &&
                            sc->asFunctionBox()->hasParameterExprs));
            }

            NameOpEmitter noe(this, name, *lhsLoc, NameOpEmitter::Kind::SimpleAssignment);
            if (!noe.prepareForRhs()) {
                return false;
            }
            if (!emitGetName(name)) {
                return false;
            }
            if (!noe.emitAssignment()) {
                return false;
            }
            if (!emit1(JSOP_POP)) {
                return false;
            }
        }

        MOZ_ASSERT_IF(fun->hasScript(), fun->nonLazyScript());
        MOZ_ASSERT(funNode->functionIsHoisted());
        return true;
    }

    funbox->wasEmitted = true;

    // Mark as singletons any function which will only be executed once, or
    // which is inner to a lambda we only expect to run once. In the latter
    // case, if the lambda runs multiple times then CloneFunctionObject will
    // make a deep clone of its contents.
    if (fun->isInterpreted()) {
        bool singleton = checkRunOnceContext();
        if (!JSFunction::setTypeForScriptedFunction(cx, fun, singleton)) {
            return false;
        }

        SharedContext* outersc = sc;
        if (fun->isInterpretedLazy()) {
            funbox->setEnclosingScopeForInnerLazyFunction(innermostScope());
            if (emittingRunOnceLambda) {
                fun->lazyScript()->setTreatAsRunOnce();
            }
        } else {
            MOZ_ASSERT_IF(outersc->strict(), funbox->strictScript);

            // Inherit most things (principals, version, etc) from the
            // parent.  Use default values for the rest.
            Rooted<JSScript*> parent(cx, script);
            MOZ_ASSERT(parent->mutedErrors() == parser->options().mutedErrors());
            const JS::TransitiveCompileOptions& transitiveOptions = parser->options();
            JS::CompileOptions options(cx, transitiveOptions);

            Rooted<JSObject*> sourceObject(cx, script->sourceObject());
            Rooted<JSScript*> script(cx, JSScript::Create(cx, options, sourceObject,
                                                          funbox->bufStart, funbox->bufEnd,
                                                          funbox->toStringStart,
                                                          funbox->toStringEnd));
            if (!script) {
                return false;
            }

            EmitterMode nestedMode = emitterMode;
            if (nestedMode == BytecodeEmitter::LazyFunction) {
                MOZ_ASSERT(lazyScript->isBinAST());
                nestedMode = BytecodeEmitter::Normal;
            }

            BytecodeEmitter bce2(this, parser, funbox, script, /* lazyScript = */ nullptr,
                                 funNode->pn_pos, nestedMode);
            if (!bce2.init()) {
                return false;
            }

            /* We measured the max scope depth when we parsed the function. */
            if (!bce2.emitFunctionScript(funNode, TopLevelFunction::No)) {
                return false;
            }

            if (funbox->isLikelyConstructorWrapper()) {
                script->setLikelyConstructorWrapper();
            }
        }

        if (outersc->isFunctionBox()) {
            outersc->asFunctionBox()->setHasInnerFunctions();
        }
    } else {
        MOZ_ASSERT(IsAsmJSModule(fun));
    }

    // Make the function object a literal in the outer script's pool.
    unsigned index = objectList.add(funNode->funbox());

    // Non-hoisted functions simply emit their respective op.
    if (!funNode->functionIsHoisted()) {
        // JSOP_LAMBDA_ARROW is always preceded by a new.target
        MOZ_ASSERT(fun->isArrow() == (funNode->getOp() == JSOP_LAMBDA_ARROW));
        if (funbox->isAsync()) {
            MOZ_ASSERT(!needsProto);
            return emitAsyncWrapper(index, funbox->needsHomeObject(), fun->isArrow(),
                                    fun->isGenerator());
        }

        if (fun->isArrow()) {
            if (sc->allowNewTarget()) {
                if (!emit1(JSOP_NEWTARGET)) {
                    return false;
                }
            } else {
                if (!emit1(JSOP_NULL)) {
                    return false;
                }
            }
        }

        if (needsProto) {
            MOZ_ASSERT(funNode->getOp() == JSOP_LAMBDA);
            funNode->setOp(JSOP_FUNWITHPROTO);
        }

        if (funNode->getOp() == JSOP_DEFFUN) {
            if (!emitIndex32(JSOP_LAMBDA, index)) {
                return false;
            }
            return emit1(JSOP_DEFFUN);
        }

        // This is a FunctionExpression, ArrowFunctionExpression, or class
        // constructor. Emit the single instruction (without location info).
        return emitIndex32(funNode->getOp(), index);
    }

    MOZ_ASSERT(!needsProto);

    bool topLevelFunction;
    if (sc->isFunctionBox() || (sc->isEvalContext() && sc->strict())) {
        // No nested functions inside other functions are top-level.
        topLevelFunction = false;
    } else {
        // In sloppy eval scripts, top-level functions in are accessed
        // dynamically. In global and module scripts, top-level functions are
        // those bound in the var scope.
        NameLocation loc = lookupName(name);
        topLevelFunction = loc.kind() == NameLocation::Kind::Dynamic ||
                           loc.bindingKind() == BindingKind::Var;
    }

    if (topLevelFunction) {
        if (sc->isModuleContext()) {
            // For modules, we record the function and instantiate the binding
            // during ModuleInstantiate(), before the script is run.

            RootedModuleObject module(cx, sc->asModuleContext()->module());
            if (!module->noteFunctionDeclaration(cx, name, fun)) {
                return false;
            }
        } else {
            MOZ_ASSERT(sc->isGlobalContext() || sc->isEvalContext());
            MOZ_ASSERT(funNode->getOp() == JSOP_NOP);
            switchToPrologue();
            if (funbox->isAsync()) {
                if (!emitAsyncWrapper(index, fun->isMethod(), fun->isArrow(),
                                      fun->isGenerator()))
                {
                    return false;
                }
            } else {
                if (!emitIndex32(JSOP_LAMBDA, index)) {
                    return false;
                }
            }
            if (!emit1(JSOP_DEFFUN)) {
                return false;
            }
            switchToMain();
        }
    } else {
        // For functions nested within functions and blocks, make a lambda and
        // initialize the binding name of the function in the current scope.

        NameOpEmitter noe(this, name, NameOpEmitter::Kind::Initialize);
        if (!noe.prepareForRhs()) {
            return false;
        }
        if (funbox->isAsync()) {
            if (!emitAsyncWrapper(index, /* needsHomeObject = */ false,
                                  /* isArrow = */ false, funbox->isGenerator()))
            {
                return false;
            }
        } else {
            if (!emitIndexOp(JSOP_LAMBDA, index)) {
                return false;
            }
        }
        if (!noe.emitAssignment()) {
            return false;
        }
        if (!emit1(JSOP_POP)) {
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitAsyncWrapperLambda(unsigned index, bool isArrow) {
    if (isArrow) {
        if (sc->allowNewTarget()) {
            if (!emit1(JSOP_NEWTARGET)) {
                return false;
            }
        } else {
            if (!emit1(JSOP_NULL)) {
                return false;
            }
        }
        if (!emitIndex32(JSOP_LAMBDA_ARROW, index)) {
            return false;
        }
    } else {
        if (!emitIndex32(JSOP_LAMBDA, index)) {
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitAsyncWrapper(unsigned index, bool needsHomeObject, bool isArrow,
                                  bool isGenerator)
{
    // needsHomeObject can be true for propertyList for extended class.
    // In that case push both unwrapped and wrapped function, in order to
    // initialize home object of unwrapped function, and set wrapped function
    // as a property.
    //
    //   lambda       // unwrapped
    //   dup          // unwrapped unwrapped
    //   toasync      // unwrapped wrapped
    //
    // Emitted code is surrounded by the following code.
    //
    //                    // classObj classCtor classProto
    //   (emitted code)   // classObj classCtor classProto unwrapped wrapped
    //   swap             // classObj classCtor classProto wrapped unwrapped
    //   inithomeobject 1 // classObj classCtor classProto wrapped unwrapped
    //                    //   initialize the home object of unwrapped
    //                    //   with classProto here
    //   pop              // classObj classCtor classProto wrapped
    //   inithiddenprop   // classObj classCtor classProto wrapped
    //                    //   initialize the property of the classProto
    //                    //   with wrapped function here
    //   pop              // classObj classCtor classProto
    //
    // needsHomeObject is false for other cases, push wrapped function only.
    if (!emitAsyncWrapperLambda(index, isArrow)) {
        return false;
    }
    if (needsHomeObject) {
        if (!emit1(JSOP_DUP)) {
            return false;
        }
    }
    if (isGenerator) {
        if (!emit1(JSOP_TOASYNCGEN)) {
            return false;
        }
    } else {
        if (!emit1(JSOP_TOASYNC)) {
            return false;
        }
    }
    return true;
}

bool
BytecodeEmitter::emitDo(BinaryNode* doNode)
{
    ParseNode* bodyNode = doNode->left();

    DoWhileEmitter doWhile(this);
    if (!doWhile.emitBody(Some(doNode->pn_pos.begin), getOffsetForLoop(bodyNode))) {
        return false;
    }

    if (!emitTree(bodyNode)) {
        return false;
    }

    if (!doWhile.emitCond()) {
        return false;
    }

    ParseNode* condNode = doNode->right();
    if (!emitTree(condNode)) {
        return false;
    }

    if (!doWhile.emitEnd()) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitWhile(BinaryNode* whileNode)
{
    ParseNode* bodyNode = whileNode->right();

    WhileEmitter wh(this);
    if (!wh.emitBody(Some(whileNode->pn_pos.begin),
                     getOffsetForLoop(bodyNode), Some(whileNode->pn_pos.end)))
    {
        return false;
    }

    if (!emitTree(bodyNode)) {
        return false;
    }

    ParseNode* condNode = whileNode->left();
    if (!wh.emitCond(getOffsetForLoop(condNode))) {
        return false;
    }

    if (!emitTree(condNode)) {
        return false;
    }

    if (!wh.emitEnd()) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitBreak(PropertyName* label)
{
    BreakableControl* target;
    SrcNoteType noteType;
    if (label) {
        // Any statement with the matching label may be the break target.
        auto hasSameLabel = [label](LabelControl* labelControl) {
            return labelControl->label() == label;
        };
        target = findInnermostNestableControl<LabelControl>(hasSameLabel);
        noteType = SRC_BREAK2LABEL;
    } else {
        auto isNotLabel = [](BreakableControl* control) {
            return !control->is<LabelControl>();
        };
        target = findInnermostNestableControl<BreakableControl>(isNotLabel);
        noteType = (target->kind() == StatementKind::Switch) ? SRC_SWITCHBREAK : SRC_BREAK;
    }

    return emitGoto(target, &target->breaks, noteType);
}

bool
BytecodeEmitter::emitContinue(PropertyName* label)
{
    LoopControl* target = nullptr;
    if (label) {
        // Find the loop statement enclosed by the matching label.
        NestableControl* control = innermostNestableControl;
        while (!control->is<LabelControl>() || control->as<LabelControl>().label() != label) {
            if (control->is<LoopControl>()) {
                target = &control->as<LoopControl>();
            }
            control = control->enclosing();
        }
    } else {
        target = findInnermostNestableControl<LoopControl>();
    }
    return emitGoto(target, &target->continues, SRC_CONTINUE);
}

bool
BytecodeEmitter::emitGetFunctionThis(NameNode* thisName)
{
    MOZ_ASSERT(sc->thisBinding() == ThisBinding::Function);
    MOZ_ASSERT(thisName->isName(cx->names().dotThis));

    return emitGetFunctionThis(Some(thisName->pn_pos.begin));
}

bool
BytecodeEmitter::emitGetFunctionThis(const mozilla::Maybe<uint32_t>& offset)
{
    if (offset) {
        if (!updateLineNumberNotes(*offset)) {
            return false;
        }
    }

    if (!emitGetName(cx->names().dotThis)) {              // THIS
        return false;
    }
    if (sc->needsThisTDZChecks()) {
        if (!emit1(JSOP_CHECKTHIS)) {                     // THIS
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitGetThisForSuperBase(UnaryNode* superBase)
{
    MOZ_ASSERT(superBase->isKind(ParseNodeKind::SuperBase));
    NameNode* nameNode = &superBase->kid()->as<NameNode>();
    return emitGetFunctionThis(nameNode);                 // THIS
}

bool
BytecodeEmitter::emitThisLiteral(ThisLiteral* pn)
{
    if (ParseNode* kid = pn->kid()) {
        NameNode* thisName = &kid->as<NameNode>();
        return emitGetFunctionThis(thisName);             // THIS
    }

    if (sc->thisBinding() == ThisBinding::Module) {
        return emit1(JSOP_UNDEFINED);                     // UNDEF
    }

    MOZ_ASSERT(sc->thisBinding() == ThisBinding::Global);
    return emit1(JSOP_GLOBALTHIS);                        // THIS
}

bool
BytecodeEmitter::emitCheckDerivedClassConstructorReturn()
{
    MOZ_ASSERT(lookupName(cx->names().dotThis).hasKnownSlot());
    if (!emitGetName(cx->names().dotThis)) {
        return false;
    }
    if (!emit1(JSOP_CHECKRETURN)) {
        return false;
    }
    return true;
}

bool
BytecodeEmitter::emitReturn(UnaryNode* returnNode)
{
    if (!updateSourceCoordNotes(returnNode->pn_pos.begin)) {
        return false;
    }

    bool needsIteratorResult = sc->isFunctionBox() && sc->asFunctionBox()->needsIteratorResult();
    if (needsIteratorResult) {
        if (!emitPrepareIteratorResult()) {
            return false;
        }
    }

    /* Push a return value */
    if (ParseNode* expr = returnNode->kid()) {
        if (!emitTree(expr)) {
            return false;
        }

        bool isAsyncGenerator = sc->asFunctionBox()->isAsync() &&
                                sc->asFunctionBox()->isGenerator();
        if (isAsyncGenerator) {
            if (!emitAwaitInInnermostScope()) {
                return false;
            }
        }
    } else {
        /* No explicit return value provided */
        if (!emit1(JSOP_UNDEFINED)) {
            return false;
        }
    }

    if (needsIteratorResult) {
        if (!emitFinishIteratorResult(true)) {
            return false;
        }
    }

    // We know functionBodyEndPos is set because "return" is only
    // valid in a function, and so we've passed through
    // emitFunctionScript.
    MOZ_ASSERT(functionBodyEndPosSet);
    if (!updateSourceCoordNotes(functionBodyEndPos)) {
        return false;
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
    ptrdiff_t top = offset();

    bool needsFinalYield = sc->isFunctionBox() && sc->asFunctionBox()->needsFinalYield();
    bool isDerivedClassConstructor =
        sc->isFunctionBox() && sc->asFunctionBox()->isDerivedClassConstructor();

    if (!emit1((needsFinalYield || isDerivedClassConstructor) ? JSOP_SETRVAL : JSOP_RETURN)) {
        return false;
    }

    // Make sure that we emit this before popping the blocks in prepareForNonLocalJump,
    // to ensure that the error is thrown while the scope-chain is still intact.
    if (isDerivedClassConstructor) {
        if (!emitCheckDerivedClassConstructorReturn()) {
            return false;
        }
    }

    NonLocalExitControl nle(this, NonLocalExitControl::Return);

    if (!nle.prepareForNonLocalJumpToOutermost()) {
        return false;
    }

    if (needsFinalYield) {
        // We know that .generator is on the function scope, as we just exited
        // all nested scopes.
        NameLocation loc =
            *locationOfNameBoundInFunctionScope(cx->names().dotGenerator, varEmitterScope);
        if (!emitGetNameAtLocation(cx->names().dotGenerator, loc)) {
            return false;
        }
        if (!emitYieldOp(JSOP_FINALYIELDRVAL)) {
            return false;
        }
    } else if (isDerivedClassConstructor) {
        MOZ_ASSERT(code()[top] == JSOP_SETRVAL);
        if (!emit1(JSOP_RETRVAL)) {
            return false;
        }
    } else if (top + static_cast<ptrdiff_t>(JSOP_RETURN_LENGTH) != offset()) {
        code()[top] = JSOP_SETRVAL;
        if (!emit1(JSOP_RETRVAL)) {
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitGetDotGeneratorInScope(EmitterScope& currentScope)
{
    NameLocation loc = *locationOfNameBoundInFunctionScope(cx->names().dotGenerator, &currentScope);
    return emitGetNameAtLocation(cx->names().dotGenerator, loc);
}

bool
BytecodeEmitter::emitInitialYield(UnaryNode* yieldNode)
{
    if (!emitTree(yieldNode->kid())) {
        return false;
    }

    if (!emitYieldOp(JSOP_INITIALYIELD)) {
        return false;
    }

    if (!emit1(JSOP_POP)) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitYield(UnaryNode* yieldNode)
{
    MOZ_ASSERT(sc->isFunctionBox());
    MOZ_ASSERT(yieldNode->isKind(ParseNodeKind::Yield));

    bool needsIteratorResult = sc->asFunctionBox()->needsIteratorResult();
    if (needsIteratorResult) {
        if (!emitPrepareIteratorResult()) {
            return false;
        }
    }
    if (ParseNode* expr = yieldNode->kid()) {
        if (!emitTree(expr)) {
            return false;
        }
    } else {
        if (!emit1(JSOP_UNDEFINED)) {
            return false;
        }
    }

    // 11.4.3.7 AsyncGeneratorYield step 5.
    bool isAsyncGenerator = sc->asFunctionBox()->isAsync();
    if (isAsyncGenerator) {
        if (!emitAwaitInInnermostScope()) {               // RESULT
            return false;
        }
    }

    if (needsIteratorResult) {
        if (!emitFinishIteratorResult(false)) {
            return false;
        }
    }

    if (!emitGetDotGeneratorInInnermostScope()) {
        return false;
    }

    if (!emitYieldOp(JSOP_YIELD)) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitAwaitInInnermostScope(UnaryNode* awaitNode)
{
    MOZ_ASSERT(sc->isFunctionBox());
    MOZ_ASSERT(awaitNode->isKind(ParseNodeKind::Await));

    if (!emitTree(awaitNode->kid())) {
        return false;
    }
    return emitAwaitInInnermostScope();
}

bool
BytecodeEmitter::emitAwaitInScope(EmitterScope& currentScope)
{
    if (!emit1(JSOP_TRYSKIPAWAIT)) {            // VALUE_OR_RESOLVED CANSKIP
        return false;
    }

    if (!emit1(JSOP_NOT)) {                     // VALUE_OR_RESOLVED !CANSKIP
        return false;
    }

    InternalIfEmitter ifCanSkip(this);
    if (!ifCanSkip.emitThen()) {                // VALUE_OR_RESOLVED
        return false;
    }

    if (!emitGetDotGeneratorInScope(currentScope)) {
        return false;                           // VALUE GENERATOR
    }
    if (!emitYieldOp(JSOP_AWAIT)) {             // RESOLVED
        return false;
    }

    if (!ifCanSkip.emitEnd()) {
        return false;
    }

    MOZ_ASSERT(ifCanSkip.popped() == 0);

    return true;
}

bool
BytecodeEmitter::emitYieldStar(ParseNode* iter)
{
    MOZ_ASSERT(sc->isFunctionBox());
    MOZ_ASSERT(sc->asFunctionBox()->isGenerator());

    IteratorKind iterKind = sc->asFunctionBox()->isAsync()
                            ? IteratorKind::Async
                            : IteratorKind::Sync;

    if (!emitTree(iter)) {                                // ITERABLE
        return false;
    }
    if (iterKind == IteratorKind::Async) {
        if (!emitAsyncIterator()) {                       // NEXT ITER
            return false;
        }
    } else {
        if (!emitIterator()) {                            // NEXT ITER
            return false;
        }
    }

    // Initial send value is undefined.
    if (!emit1(JSOP_UNDEFINED)) {                         // NEXT ITER RECEIVED
        return false;
    }

    int32_t savedDepthTemp;
    int32_t startDepth = stackDepth;
    MOZ_ASSERT(startDepth >= 3);

    TryEmitter tryCatch(this, TryEmitter::Kind::TryCatchFinally,
                        TryEmitter::ControlKind::NonSyntactic);
    if (!tryCatch.emitJumpOverCatchAndFinally()) {        // NEXT ITER RESULT
        return false;
    }

    JumpTarget tryStart{ offset() };
    if (!tryCatch.emitTry()) {                            // NEXT ITER RESULT
        return false;
    }

    MOZ_ASSERT(this->stackDepth == startDepth);

    // 11.4.3.7 AsyncGeneratorYield step 5.
    if (iterKind == IteratorKind::Async) {
        if (!emitAwaitInInnermostScope()) {               // NEXT ITER RESULT
            return false;
        }
    }

    // Load the generator object.
    if (!emitGetDotGeneratorInInnermostScope()) {         // NEXT ITER RESULT GENOBJ
        return false;
    }

    // Yield RESULT as-is, without re-boxing.
    if (!emitYieldOp(JSOP_YIELD)) {                       // NEXT ITER RECEIVED
        return false;
    }

    if (!tryCatch.emitCatch()) {                          // NEXT ITER RESULT
        return false;
    }

    MOZ_ASSERT(stackDepth == startDepth);

    if (!emit1(JSOP_EXCEPTION)) {                         // NEXT ITER RESULT EXCEPTION
        return false;
    }
    if (!emitDupAt(2)) {                                  // NEXT ITER RESULT EXCEPTION ITER
        return false;
    }
    if (!emit1(JSOP_DUP)) {                               // NEXT ITER RESULT EXCEPTION ITER ITER
        return false;
    }
    if (!emitAtomOp(cx->names().throw_, JSOP_CALLPROP)) { // NEXT ITER RESULT EXCEPTION ITER THROW
        return false;
    }

    savedDepthTemp = stackDepth;
    InternalIfEmitter ifThrowMethodIsNotDefined(this);
    if (!emitPushNotUndefinedOrNull()) {                  // NEXT ITER RESULT EXCEPTION ITER THROW NOT-UNDEF-OR-NULL
        return false;
    }

    if (!ifThrowMethodIsNotDefined.emitThenElse()) {      // NEXT ITER RESULT EXCEPTION ITER THROW
        return false;
    }

    // ES 14.4.13, YieldExpression : yield * AssignmentExpression, step 5.b.iii.4.
    // RESULT = ITER.throw(EXCEPTION)                     // NEXT ITER OLDRESULT EXCEPTION ITER THROW
    if (!emit1(JSOP_SWAP)) {                              // NEXT ITER OLDRESULT EXCEPTION THROW ITER
        return false;
    }
    if (!emit2(JSOP_PICK, 2)) {                           // NEXT ITER OLDRESULT THROW ITER EXCEPTION
        return false;
    }
    if (!emitCall(JSOP_CALL, 1, iter)) {                  // NEXT ITER OLDRESULT RESULT
        return false;
    }
    checkTypeSet(JSOP_CALL);

    if (iterKind == IteratorKind::Async) {
        if (!emitAwaitInInnermostScope()) {               // NEXT ITER OLDRESULT RESULT
            return false;
        }
    }

    if (!emitCheckIsObj(CheckIsObjectKind::IteratorThrow)) { // NEXT ITER OLDRESULT RESULT
        return false;
    }
    if (!emit1(JSOP_SWAP)) {                              // NEXT ITER RESULT OLDRESULT
        return false;
    }
    if (!emit1(JSOP_POP)) {                               // NEXT ITER RESULT
        return false;
    }
    MOZ_ASSERT(this->stackDepth == startDepth);
    JumpList checkResult;
    // ES 14.4.13, YieldExpression : yield * AssignmentExpression, step 5.b.ii.
    //
    // Note that there is no GOSUB to the finally block here. If the iterator has a
    // "throw" method, it does not perform IteratorClose.
    if (!emitJump(JSOP_GOTO, &checkResult)) {             // goto checkResult
        return false;
    }

    stackDepth = savedDepthTemp;
    if (!ifThrowMethodIsNotDefined.emitElse()) {          // NEXT ITER RESULT EXCEPTION ITER THROW
        return false;
    }

    if (!emit1(JSOP_POP)) {                               // NEXT ITER RESULT EXCEPTION ITER
        return false;
    }
    // ES 14.4.13, YieldExpression : yield * AssignmentExpression, step 5.b.iii.2
    //
    // If the iterator does not have a "throw" method, it calls IteratorClose
    // and then throws a TypeError.
    if (!emitIteratorCloseInInnermostScope(iterKind)) {   // NEXT ITER RESULT EXCEPTION
        return false;
    }
    if (!emitUint16Operand(JSOP_THROWMSG, JSMSG_ITERATOR_NO_THROW)) { // throw
        return false;
    }

    stackDepth = savedDepthTemp;
    if (!ifThrowMethodIsNotDefined.emitEnd()) {
        return false;
    }

    stackDepth = startDepth;
    if (!tryCatch.emitFinally()) {
         return false;
    }

    // ES 14.4.13, yield * AssignmentExpression, step 5.c
    //
    // Call iterator.return() for receiving a "forced return" completion from
    // the generator.

    InternalIfEmitter ifGeneratorClosing(this);
    if (!emit1(JSOP_ISGENCLOSING)) {                      // NEXT ITER RESULT FTYPE FVALUE CLOSING
        return false;
    }
    if (!ifGeneratorClosing.emitThen()) {                 // NEXT ITER RESULT FTYPE FVALUE
        return false;
    }

    // Step ii.
    //
    // Get the "return" method.
    if (!emitDupAt(3)) {                                  // NEXT ITER RESULT FTYPE FVALUE ITER
        return false;
    }
    if (!emit1(JSOP_DUP)) {                               // NEXT ITER RESULT FTYPE FVALUE ITER ITER
        return false;
    }
    if (!emitAtomOp(cx->names().return_, JSOP_CALLPROP)) {  // NEXT ITER RESULT FTYPE FVALUE ITER RET
        return false;
    }

    // Step iii.
    //
    // Do nothing if "return" is undefined or null.
    InternalIfEmitter ifReturnMethodIsDefined(this);
    if (!emitPushNotUndefinedOrNull()) {                  // NEXT ITER RESULT FTYPE FVALUE ITER RET NOT-UNDEF-OR-NULL
        return false;
    }

    // Step iv.
    //
    // Call "return" with the argument passed to Generator.prototype.return,
    // which is currently in rval.value.
    if (!ifReturnMethodIsDefined.emitThenElse()) {        // NEXT ITER OLDRESULT FTYPE FVALUE ITER RET
        return false;
    }
    if (!emit1(JSOP_SWAP)) {                              // NEXT ITER OLDRESULT FTYPE FVALUE RET ITER
        return false;
    }
    if (!emit1(JSOP_GETRVAL)) {                           // NEXT ITER OLDRESULT FTYPE FVALUE RET ITER RVAL
        return false;
    }
    if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) {   // NEXT ITER OLDRESULT FTYPE FVALUE RET ITER VALUE
        return false;
    }
    if (!emitCall(JSOP_CALL, 1)) {                        // NEXT ITER OLDRESULT FTYPE FVALUE RESULT
        return false;
    }
    checkTypeSet(JSOP_CALL);

    if (iterKind == IteratorKind::Async) {
        if (!emitAwaitInInnermostScope()) {               // ... FTYPE FVALUE RESULT
            return false;
        }
    }

    // Step v.
    if (!emitCheckIsObj(CheckIsObjectKind::IteratorReturn)) { // NEXT ITER OLDRESULT FTYPE FVALUE RESULT
        return false;
    }

    // Steps vi-viii.
    //
    // Check if the returned object from iterator.return() is done. If not,
    // continuing yielding.
    InternalIfEmitter ifReturnDone(this);
    if (!emit1(JSOP_DUP)) {                               // NEXT ITER OLDRESULT FTYPE FVALUE RESULT RESULT
        return false;
    }
    if (!emitAtomOp(cx->names().done, JSOP_GETPROP)) {    // NEXT ITER OLDRESULT FTYPE FVALUE RESULT DONE
        return false;
    }
    if (!ifReturnDone.emitThenElse()) {                   // NEXT ITER OLDRESULT FTYPE FVALUE RESULT
        return false;
    }
    if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) {   // NEXT ITER OLDRESULT FTYPE FVALUE VALUE
        return false;
    }

    if (!emitPrepareIteratorResult()) {                   // NEXT ITER OLDRESULT FTYPE FVALUE VALUE RESULT
        return false;
    }
    if (!emit1(JSOP_SWAP)) {                              // NEXT ITER OLDRESULT FTYPE FVALUE RESULT VALUE
        return false;
    }
    if (!emitFinishIteratorResult(true)) {                // NEXT ITER OLDRESULT FTYPE FVALUE RESULT
        return false;
    }
    if (!emit1(JSOP_SETRVAL)) {                           // NEXT ITER OLDRESULT FTYPE FVALUE
        return false;
    }
    savedDepthTemp = this->stackDepth;
    if (!ifReturnDone.emitElse()) {                       // NEXT ITER OLDRESULT FTYPE FVALUE RESULT
        return false;
    }
    if (!emit2(JSOP_UNPICK, 3)) {                         // NEXT ITER RESULT OLDRESULT FTYPE FVALUE
        return false;
    }
    if (!emitPopN(3)) {                                   // NEXT ITER RESULT
        return false;
    }
    {
        // goto tryStart;
        JumpList beq;
        JumpTarget breakTarget{ -1 };
        if (!emitBackwardJump(JSOP_GOTO, tryStart, &beq, &breakTarget)) { // NEXT ITER RESULT
            return false;
        }
    }
    this->stackDepth = savedDepthTemp;
    if (!ifReturnDone.emitEnd()) {
        return false;
    }

    if (!ifReturnMethodIsDefined.emitElse()) {            // NEXT ITER RESULT FTYPE FVALUE ITER RET
        return false;
    }
    if (!emitPopN(2)) {                                   // NEXT ITER RESULT FTYPE FVALUE
        return false;
    }
    if (!ifReturnMethodIsDefined.emitEnd()) {
        return false;
    }

    if (!ifGeneratorClosing.emitEnd()) {
        return false;
    }

    if (!tryCatch.emitEnd()) {
        return false;
    }

    // After the try-catch-finally block: send the received value to the iterator.
    // result = iter.next(received)                              // NEXT ITER RECEIVED
    if (!emit2(JSOP_UNPICK, 2)) {                                // RECEIVED NEXT ITER
        return false;
    }
    if (!emit1(JSOP_DUP2)) {                                     // RECEIVED NEXT ITER NEXT ITER
        return false;
    }
    if (!emit2(JSOP_PICK, 4)) {                                  // NEXT ITER NEXT ITER RECEIVED
        return false;
    }
    if (!emitCall(JSOP_CALL, 1, iter)) {                         // NEXT ITER RESULT
        return false;
    }
    checkTypeSet(JSOP_CALL);

    if (iterKind == IteratorKind::Async) {
        if (!emitAwaitInInnermostScope()) {                      // NEXT ITER RESULT RESULT
            return false;
        }
    }

    if (!emitCheckIsObj(CheckIsObjectKind::IteratorNext)) {      // NEXT ITER RESULT
        return false;
    }
    MOZ_ASSERT(this->stackDepth == startDepth);

    if (!emitJumpTargetAndPatch(checkResult)) {                  // checkResult:
        return false;
    }

    // if (!result.done) goto tryStart;                          // NEXT ITER RESULT
    if (!emit1(JSOP_DUP)) {                                      // NEXT ITER RESULT RESULT
        return false;
    }
    if (!emitAtomOp(cx->names().done, JSOP_GETPROP)) {           // NEXT ITER RESULT DONE
        return false;
    }
    // if (!DONE) goto tryStart;
    {
        JumpList beq;
        JumpTarget breakTarget{ -1 };
        if (!emitBackwardJump(JSOP_IFEQ, tryStart, &beq, &breakTarget)) { // NEXT ITER RESULT
            return false;
        }
    }

    // result.value
    if (!emit2(JSOP_UNPICK, 2)) {                                // RESULT NEXT ITER
        return false;
    }
    if (!emitPopN(2)) {                                          // RESULT
        return false;
    }
    if (!emitAtomOp(cx->names().value, JSOP_GETPROP)) {          // VALUE
        return false;
    }

    MOZ_ASSERT(this->stackDepth == startDepth - 2);

    return true;
}

bool
BytecodeEmitter::emitStatementList(ListNode* stmtList)
{
    for (ParseNode* stmt : stmtList->contents()) {
        if (!emitTree(stmt)) {
            return false;
        }
    }
    return true;
}

bool
BytecodeEmitter::emitExpressionStatement(UnaryNode* exprStmt)
{
    MOZ_ASSERT(exprStmt->isKind(ParseNodeKind::ExpressionStatement));

    /*
     * Top-level or called-from-a-native JS_Execute/EvaluateScript,
     * debugger, and eval frames may need the value of the ultimate
     * expression statement as the script's result, despite the fact
     * that it appears useless to the compiler.
     *
     * API users may also set the JSOPTION_NO_SCRIPT_RVAL option when
     * calling JS_Compile* to suppress JSOP_SETRVAL.
     */
    bool wantval = false;
    bool useful = false;
    if (sc->isFunctionBox()) {
        MOZ_ASSERT(!script->noScriptRval());
    } else {
        useful = wantval = !script->noScriptRval();
    }

    /* Don't eliminate expressions with side effects. */
    ParseNode* expr = exprStmt->kid();
    if (!useful) {
        if (!checkSideEffects(expr, &useful)) {
            return false;
        }

        /*
         * Don't eliminate apparently useless expressions if they are labeled
         * expression statements. The startOffset() test catches the case
         * where we are nesting in emitTree for a labeled compound statement.
         */
        if (innermostNestableControl &&
            innermostNestableControl->is<LabelControl>() &&
            innermostNestableControl->as<LabelControl>().startOffset() >= offset())
        {
            useful = true;
        }
    }

    if (useful) {
        MOZ_ASSERT_IF(expr->isKind(ParseNodeKind::Assign), expr->isOp(JSOP_NOP));
        ValueUsage valueUsage = wantval ? ValueUsage::WantValue : ValueUsage::IgnoreValue;
        ExpressionStatementEmitter ese(this, valueUsage);
        if (!ese.prepareForExpr(Some(exprStmt->pn_pos.begin))) {
            return false;
        }
        if (!emitTree(expr, valueUsage)) {
            return false;
        }
        if (!ese.emitEnd()) {
            return false;
        }
    } else if (exprStmt->isDirectivePrologueMember()) {
        // Don't complain about directive prologue members; just don't emit
        // their code.
    } else {
        if (JSAtom* atom = exprStmt->isStringExprStatement()) {
            // Warn if encountering a non-directive prologue member string
            // expression statement, that is inconsistent with the current
            // directive prologue.  That is, a script *not* starting with
            // "use strict" should warn for any "use strict" statements seen
            // later in the script, because such statements are misleading.
            const char* directive = nullptr;
            if (atom == cx->names().useStrict) {
                if (!sc->strictScript) {
                    directive = js_useStrict_str;
                }
            } else if (atom == cx->names().useAsm) {
                if (sc->isFunctionBox()) {
                    if (IsAsmJSModule(sc->asFunctionBox()->function())) {
                        directive = js_useAsm_str;
                    }
                }
            }

            if (directive) {
                if (!reportExtraWarning(expr, JSMSG_CONTRARY_NONDIRECTIVE, directive)) {
                    return false;
                }
            }
        } else {
            if (!reportExtraWarning(expr, JSMSG_USELESS_EXPR)) {
                return false;
            }
        }
    }

    return true;
}

bool
BytecodeEmitter::emitDeleteName(UnaryNode* deleteNode)
{
    MOZ_ASSERT(deleteNode->isKind(ParseNodeKind::DeleteName));

    NameNode* nameExpr = &deleteNode->kid()->as<NameNode>();
    MOZ_ASSERT(nameExpr->isKind(ParseNodeKind::Name));

    return emitAtomOp(nameExpr->atom(), JSOP_DELNAME);
}

bool
BytecodeEmitter::emitDeleteProperty(UnaryNode* deleteNode)
{
    MOZ_ASSERT(deleteNode->isKind(ParseNodeKind::DeleteProp));

    PropertyAccess* propExpr = &deleteNode->kid()->as<PropertyAccess>();
    // TODO(khyperia): Implement private field access.
    PropOpEmitter poe(this,
                      PropOpEmitter::Kind::Delete,
                      propExpr->as<PropertyAccess>().isSuper()
                      ? PropOpEmitter::ObjKind::Super
                      : PropOpEmitter::ObjKind::Other);
    if (propExpr->isSuper()) {
        // The expression |delete super.foo;| has to evaluate |super.foo|,
        // which could throw if |this| hasn't yet been set by a |super(...)|
        // call or the super-base is not an object, before throwing a
        // ReferenceError for attempting to delete a super-reference.
        UnaryNode* base = &propExpr->expression().as<UnaryNode>();
        if (!emitGetThisForSuperBase(base)) {             // THIS
            return false;
        }
    } else {
        if (!poe.prepareForObj()) {
            return false;
        }
        if (!emitPropLHS(propExpr)) {                         // OBJ
            return false;
        }
    }

    if (!poe.emitDelete(propExpr->key().atom())) {        // [Super]
        //                                                // THIS
        //                                                // [Other]
        //                                                // SUCCEEDED
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitDeleteElement(UnaryNode* deleteNode)
{
    MOZ_ASSERT(deleteNode->isKind(ParseNodeKind::DeleteElem));

    PropertyByValue* elemExpr = &deleteNode->kid()->as<PropertyByValue>();
    bool isSuper = elemExpr->isSuper();
    ElemOpEmitter eoe(this,
                      ElemOpEmitter::Kind::Delete,
                      isSuper
                      ? ElemOpEmitter::ObjKind::Super
                      : ElemOpEmitter::ObjKind::Other);
    if (isSuper) {
        // The expression |delete super[foo];| has to evaluate |super[foo]|,
        // which could throw if |this| hasn't yet been set by a |super(...)|
        // call, or trigger side-effects when evaluating ToPropertyKey(foo),
        // or also throw when the super-base is not an object, before throwing
        // a ReferenceError for attempting to delete a super-reference.
        if (!eoe.prepareForObj()) {                       //
            return false;
        }

        UnaryNode* base = &elemExpr->expression().as<UnaryNode>();
        if (!emitGetThisForSuperBase(base)) {             // THIS
            return false;
        }
        if (!eoe.prepareForKey()) {                       // THIS
            return false;
        }
        if (!emitTree(&elemExpr->key())) {                // THIS KEY
            return false;
        }
    } else {
        if (!emitElemObjAndKey(elemExpr, false, eoe)) {   // OBJ KEY
            return false;
        }
    }
    if (!eoe.emitDelete()) {                              // [Super]
        //                                                // THIS
        //                                                // [Other]
        //                                                // SUCCEEDED
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitDeleteExpression(UnaryNode* deleteNode)
{
    MOZ_ASSERT(deleteNode->isKind(ParseNodeKind::DeleteExpr));

    ParseNode* expression = deleteNode->kid();

    // If useless, just emit JSOP_TRUE; otherwise convert |delete <expr>| to
    // effectively |<expr>, true|.
    bool useful = false;
    if (!checkSideEffects(expression, &useful)) {
        return false;
    }

    if (useful) {
        if (!emitTree(expression)) {
            return false;
        }
        if (!emit1(JSOP_POP)) {
            return false;
        }
    }

    return emit1(JSOP_TRUE);
}

static const char *
SelfHostedCallFunctionName(JSAtom* name, JSContext* cx)
{
    if (name == cx->names().callFunction) {
        return "callFunction";
    }
    if (name == cx->names().callContentFunction) {
        return "callContentFunction";
    }
    if (name == cx->names().constructContentFunction) {
        return "constructContentFunction";
    }

    MOZ_CRASH("Unknown self-hosted call function name");
}

bool
BytecodeEmitter::emitSelfHostedCallFunction(BinaryNode* callNode)
{
    // Special-casing of callFunction to emit bytecode that directly
    // invokes the callee with the correct |this| object and arguments.
    // callFunction(fun, thisArg, arg0, arg1) thus becomes:
    // - emit lookup for fun
    // - emit lookup for thisArg
    // - emit lookups for arg0, arg1
    //
    // argc is set to the amount of actually emitted args and the
    // emitting of args below is disabled by setting emitArgs to false.
    NameNode* calleeNode = &callNode->left()->as<NameNode>();
    ListNode* argsList = &callNode->right()->as<ListNode>();

    const char* errorName = SelfHostedCallFunctionName(calleeNode->name(), cx);

    if (argsList->count() < 2) {
        reportError(callNode, JSMSG_MORE_ARGS_NEEDED, errorName, "2", "s");
        return false;
    }

    JSOp callOp = callNode->getOp();
    if (callOp != JSOP_CALL) {
        reportError(callNode, JSMSG_NOT_CONSTRUCTOR, errorName);
        return false;
    }

    bool constructing = calleeNode->name() == cx->names().constructContentFunction;
    ParseNode* funNode = argsList->head();
    if (constructing) {
        callOp = JSOP_NEW;
    } else if (funNode->isName(cx->names().std_Function_apply)) {
        callOp = JSOP_FUNAPPLY;
    }

    if (!emitTree(funNode)) {
        return false;
    }

#ifdef DEBUG
    if (emitterMode == BytecodeEmitter::SelfHosting &&
        calleeNode->name() == cx->names().callFunction)
    {
        if (!emit1(JSOP_DEBUGCHECKSELFHOSTED)) {
            return false;
        }
    }
#endif

    ParseNode* thisOrNewTarget = funNode->pn_next;
    if (constructing) {
        // Save off the new.target value, but here emit a proper |this| for a
        // constructing call.
        if (!emit1(JSOP_IS_CONSTRUCTING)) {
            return false;
        }
    } else {
        // It's |this|, emit it.
        if (!emitTree(thisOrNewTarget)) {
            return false;
        }
    }

    for (ParseNode* argpn = thisOrNewTarget->pn_next; argpn; argpn = argpn->pn_next) {
        if (!emitTree(argpn)) {
            return false;
        }
    }

    if (constructing) {
        if (!emitTree(thisOrNewTarget)) {
            return false;
        }
    }

    uint32_t argc = argsList->count() - 2;
    if (!emitCall(callOp, argc)) {
        return false;
    }

    checkTypeSet(callOp);
    return true;
}

bool
BytecodeEmitter::emitSelfHostedResumeGenerator(BinaryNode* callNode)
{
    ListNode* argsList = &callNode->right()->as<ListNode>();

    // Syntax: resumeGenerator(gen, value, 'next'|'throw'|'return')
    if (argsList->count() != 3) {
        reportError(callNode, JSMSG_MORE_ARGS_NEEDED, "resumeGenerator", "1", "s");
        return false;
    }

    ParseNode* genNode = argsList->head();
    if (!emitTree(genNode)) {
        return false;
    }

    ParseNode* valNode = genNode->pn_next;
    if (!emitTree(valNode)) {
        return false;
    }

    ParseNode* kindNode = valNode->pn_next;
    MOZ_ASSERT(kindNode->isKind(ParseNodeKind::String));
    uint16_t operand = GeneratorObject::getResumeKind(cx, kindNode->as<NameNode>().atom());
    MOZ_ASSERT(!kindNode->pn_next);

    if (!emitCall(JSOP_RESUME, operand)) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitSelfHostedForceInterpreter()
{
    if (!emit1(JSOP_FORCEINTERPRETER)) {
        return false;
    }
    if (!emit1(JSOP_UNDEFINED)) {
        return false;
    }
    return true;
}

bool
BytecodeEmitter::emitSelfHostedAllowContentIter(BinaryNode* callNode)
{
    ListNode* argsList = &callNode->right()->as<ListNode>();

    if (argsList->count() != 1) {
        reportError(callNode, JSMSG_MORE_ARGS_NEEDED, "allowContentIter", "1", "");
        return false;
    }

    // We're just here as a sentinel. Pass the value through directly.
    return emitTree(argsList->head());
}

bool
BytecodeEmitter::emitSelfHostedDefineDataProperty(BinaryNode* callNode)
{
    ListNode* argsList = &callNode->right()->as<ListNode>();

    // Only optimize when 3 arguments are passed.
    MOZ_ASSERT(argsList->count() == 3);

    ParseNode* objNode = argsList->head();
    if (!emitTree(objNode)) {
        return false;
    }

    ParseNode* idNode = objNode->pn_next;
    if (!emitTree(idNode)) {
        return false;
    }

    ParseNode* valNode = idNode->pn_next;
    if (!emitTree(valNode)) {
        return false;
    }

    // This will leave the object on the stack instead of pushing |undefined|,
    // but that's fine because the self-hosted code doesn't use the return
    // value.
    return emit1(JSOP_INITELEM);
}

bool
BytecodeEmitter::emitSelfHostedHasOwn(BinaryNode* callNode)
{
    ListNode* argsList = &callNode->right()->as<ListNode>();

    if (argsList->count() != 2) {
        reportError(callNode, JSMSG_MORE_ARGS_NEEDED, "hasOwn", "2", "");
        return false;
    }

    ParseNode* idNode = argsList->head();
    if (!emitTree(idNode)) {
        return false;
    }

    ParseNode* objNode = idNode->pn_next;
    if (!emitTree(objNode)) {
        return false;
    }

    return emit1(JSOP_HASOWN);
}

bool
BytecodeEmitter::emitSelfHostedGetPropertySuper(BinaryNode* callNode)
{
    ListNode* argsList = &callNode->right()->as<ListNode>();

    if (argsList->count() != 3) {
        reportError(callNode, JSMSG_MORE_ARGS_NEEDED, "getPropertySuper", "3", "");
        return false;
    }

    ParseNode* objNode = argsList->head();
    ParseNode* idNode = objNode->pn_next;
    ParseNode* receiverNode = idNode->pn_next;

    if (!emitTree(receiverNode)) {
        return false;
    }

    if (!emitTree(idNode)) {
        return false;
    }

    if (!emitTree(objNode)) {
        return false;
    }

    return emitElemOpBase(JSOP_GETELEM_SUPER);
}

bool
BytecodeEmitter::isRestParameter(ParseNode* expr)
{
    if (!sc->isFunctionBox()) {
        return false;
    }

    FunctionBox* funbox = sc->asFunctionBox();
    RootedFunction fun(cx, funbox->function());
    if (!funbox->hasRest()) {
        return false;
    }

    if (!expr->isKind(ParseNodeKind::Name)) {
        if (emitterMode == BytecodeEmitter::SelfHosting &&
            expr->isKind(ParseNodeKind::Call))
        {
            BinaryNode* callNode = &expr->as<BinaryNode>();
            ParseNode* calleeNode = callNode->left();
            if (calleeNode->isName(cx->names().allowContentIter)) {
                return isRestParameter(callNode->right()->as<ListNode>().head());
            }
        }
        return false;
    }

    JSAtom* name = expr->as<NameNode>().name();
    Maybe<NameLocation> paramLoc = locationOfNameBoundInFunctionScope(name);
    if (paramLoc && lookupName(name) == *paramLoc) {
        FunctionScope::Data* bindings = funbox->functionScopeBindings();
        if (bindings->nonPositionalFormalStart > 0) {
            // |paramName| can be nullptr when the rest destructuring syntax is
            // used: `function f(...[]) {}`.
            JSAtom* paramName =
                bindings->trailingNames[bindings->nonPositionalFormalStart - 1].name();
            return paramName && name == paramName;
        }
    }

    return false;
}

bool
BytecodeEmitter::emitCalleeAndThis(ParseNode* callee, ParseNode* call, CallOrNewEmitter& cone)
{
    switch (callee->getKind()) {
      case ParseNodeKind::Name:
        if (!cone.emitNameCallee(callee->as<NameNode>().name())) {
            return false;                                 // CALLEE THIS
        }
        break;
      case ParseNodeKind::Dot: {
        MOZ_ASSERT(emitterMode != BytecodeEmitter::SelfHosting);
        PropertyAccess* prop = &callee->as<PropertyAccess>();
        // TODO(khyperia): Implement private field access.
        bool isSuper = prop->isSuper();

        PropOpEmitter& poe = cone.prepareForPropCallee(isSuper);
        if (!poe.prepareForObj()) {
            return false;
        }
        if (isSuper) {
            UnaryNode* base = &prop->expression().as<UnaryNode>();
            if (!emitGetThisForSuperBase(base)) {        // THIS
                return false;
            }
        } else {
            if (!emitPropLHS(prop)) {                    // OBJ
                return false;
            }
        }
        if (!poe.emitGet(prop->key().atom())) {           // CALLEE THIS?
            return false;
        }

        break;
      }
      case ParseNodeKind::Elem: {
        MOZ_ASSERT(emitterMode != BytecodeEmitter::SelfHosting);
        PropertyByValue* elem = &callee->as<PropertyByValue>();
        bool isSuper = elem->isSuper();

        ElemOpEmitter& eoe = cone.prepareForElemCallee(isSuper);
        if (!emitElemObjAndKey(elem, isSuper, eoe)) {     // [Super]
            //                                            // THIS? THIS KEY
            //                                            // [needsThis,Other]
            //                                            // OBJ? OBJ KEY
            return false;
        }
        if (!eoe.emitGet()) {                             // CALLEE? THIS
            return false;
        }

        break;
      }
      case ParseNodeKind::Function:
        if (!cone.prepareForFunctionCallee()) {
            return false;
        }
        if (!emitTree(callee)) {                          // CALLEE
            return false;
        }
        break;
      case ParseNodeKind::SuperBase:
        MOZ_ASSERT(call->isKind(ParseNodeKind::SuperCall));
        MOZ_ASSERT(parser->astGenerator().isSuperBase(callee));
        if (!cone.emitSuperCallee()) {                    // CALLEE THIS
            return false;
        }
        break;
      default:
        if (!cone.prepareForOtherCallee()) {
            return false;
        }
        if (!emitTree(callee)) {
            return false;
        }
        break;
    }

    if (!cone.emitThis()) {                               // CALLEE THIS
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitPipeline(ListNode* node)
{
    MOZ_ASSERT(node->count() >= 2);

    if (!emitTree(node->head())) {                        // ARG
        return false;
    }

    ParseNode* callee = node->head()->pn_next;
    CallOrNewEmitter cone(this, JSOP_CALL,
                          CallOrNewEmitter::ArgumentsKind::Other,
                          ValueUsage::WantValue);
    do {
        if (!emitCalleeAndThis(callee, node, cone)) {     // ARG CALLEE THIS
            return false;
        }
        if (!emit2(JSOP_PICK, 2)) {                       // CALLEE THIS ARG
            return false;
        }
        if (!cone.emitEnd(1, Some(node->pn_pos.begin))) { // RVAL
            return false;
        }

        cone.reset();

        checkTypeSet(JSOP_CALL);
    } while ((callee = callee->pn_next));

    return true;
}

bool
BytecodeEmitter::emitArguments(ListNode* argsList, bool isCall, bool isSpread,
                               CallOrNewEmitter& cone)
{
    uint32_t argc = argsList->count();
    if (argc >= ARGC_LIMIT) {
        reportError(argsList, isCall ? JSMSG_TOO_MANY_FUN_ARGS : JSMSG_TOO_MANY_CON_ARGS);
        return false;
    }
    if (!isSpread) {
        if (!cone.prepareForNonSpreadArguments()) {       // CALLEE THIS
            return false;
        }
        for (ParseNode* arg : argsList->contents()) {
            if (!emitTree(arg)) {
                return false;
            }
        }
    } else {
        if (cone.wantSpreadOperand()) {
            UnaryNode* spreadNode = &argsList->head()->as<UnaryNode>();
            if (!emitTree(spreadNode->kid())) {           // CALLEE THIS ARG0
                return false;
            }
        }
        if (!cone.emitSpreadArgumentsTest()) {            // CALLEE THIS
            return false;
        }
        if (!emitArray(argsList->head(), argc)) {         // CALLEE THIS ARR
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitCallOrNew(BinaryNode* callNode,
                               ValueUsage valueUsage /* = ValueUsage::WantValue */)
{
    /*
     * Emit callable invocation or operator new (constructor call) code.
     * First, emit code for the left operand to evaluate the callable or
     * constructable object expression.
     *
     * For operator new, we emit JSOP_GETPROP instead of JSOP_CALLPROP, etc.
     * This is necessary to interpose the lambda-initialized method read
     * barrier -- see the code in jsinterp.cpp for JSOP_LAMBDA followed by
     * JSOP_{SET,INIT}PROP.
     *
     * Then (or in a call case that has no explicit reference-base
     * object) we emit JSOP_UNDEFINED to produce the undefined |this|
     * value required for calls (which non-strict mode functions
     * will box into the global object).
     */
    bool isCall =
        callNode->isKind(ParseNodeKind::Call) || callNode->isKind(ParseNodeKind::TaggedTemplate);
    ParseNode* calleeNode = callNode->left();
    ListNode* argsList = &callNode->right()->as<ListNode>();
    bool isSpread = JOF_OPTYPE(callNode->getOp()) == JOF_BYTE;
    if (calleeNode->isKind(ParseNodeKind::Name) && emitterMode == BytecodeEmitter::SelfHosting &&
        !isSpread)
    {
        // Calls to "forceInterpreter", "callFunction",
        // "callContentFunction", or "resumeGenerator" in self-hosted
        // code generate inline bytecode.
        PropertyName* calleeName = calleeNode->as<NameNode>().name();
        if (calleeName == cx->names().callFunction ||
            calleeName == cx->names().callContentFunction ||
            calleeName == cx->names().constructContentFunction)
        {
            return emitSelfHostedCallFunction(callNode);
        }
        if (calleeName == cx->names().resumeGenerator) {
            return emitSelfHostedResumeGenerator(callNode);
        }
        if (calleeName == cx->names().forceInterpreter) {
            return emitSelfHostedForceInterpreter();
        }
        if (calleeName == cx->names().allowContentIter) {
            return emitSelfHostedAllowContentIter(callNode);
        }
        if (calleeName == cx->names().defineDataPropertyIntrinsic && argsList->count() == 3) {
            return emitSelfHostedDefineDataProperty(callNode);
        }
        if (calleeName == cx->names().hasOwn) {
            return emitSelfHostedHasOwn(callNode);
        }
        if (calleeName == cx->names().getPropertySuper) {
            return emitSelfHostedGetPropertySuper(callNode);
        }
        // Fall through
    }

    JSOp op = callNode->getOp();
    uint32_t argc = argsList->count();
    CallOrNewEmitter cone(this, op,
                          isSpread && (argc == 1) &&
                          isRestParameter(argsList->head()->as<UnaryNode>().kid())
                          ? CallOrNewEmitter::ArgumentsKind::SingleSpreadRest
                          : CallOrNewEmitter::ArgumentsKind::Other,
                          valueUsage);
    if (!emitCalleeAndThis(calleeNode, callNode, cone)) { // CALLEE THIS
        return false;
    }
    if (!emitArguments(argsList, isCall, isSpread, cone)) {
        return false;                                     // CALLEE THIS ARGS...
    }

    ParseNode* coordNode = callNode;
    if (op == JSOP_CALL || op == JSOP_SPREADCALL) {
        switch (calleeNode->getKind()) {
          case ParseNodeKind::Dot: {

            // Check if this member is a simple chain of simple chain of
            // property accesses, e.g. x.y.z, this.x.y, super.x.y
            bool simpleDotChain = false;
            for (ParseNode* cur = calleeNode;
                 cur->isKind(ParseNodeKind::Dot);
                 cur = &cur->as<PropertyAccess>().expression())
            {
                PropertyAccess* prop = &cur->as<PropertyAccess>();
                ParseNode* left = &prop->expression();
                // TODO(khyperia): Implement private field access.
                if (left->isKind(ParseNodeKind::Name) || left->isKind(ParseNodeKind::This) ||
                    left->isKind(ParseNodeKind::SuperBase))
                {
                    simpleDotChain = true;
                }
            }

            if (!simpleDotChain) {
                // obj().aprop() // expression
                //       ^       // column coord
                //
                // Note: Because of the constant folding logic in FoldElement,
                // this case also applies for constant string properties.
                //
                // obj()['aprop']() // expression
                //       ^          // column coord
                coordNode = &calleeNode->as<PropertyAccess>().key();
            }
            break;
          }
          case ParseNodeKind::Elem:
            // obj[expr]() // expression
            //          ^  // column coord
            coordNode = argsList;
            break;
          default:
            break;
        }
    }
    if (!cone.emitEnd(argc, Some(coordNode->pn_pos.begin))) {
        return false;                                     // RVAL
    }

    return true;
}

static const JSOp ParseNodeKindToJSOp[] = {
    // JSOP_NOP is for pipeline operator which does not emit its own JSOp
    // but has highest precedence in binary operators
    JSOP_NOP,
    JSOP_OR,
    JSOP_AND,
    JSOP_BITOR,
    JSOP_BITXOR,
    JSOP_BITAND,
    JSOP_STRICTEQ,
    JSOP_EQ,
    JSOP_STRICTNE,
    JSOP_NE,
    JSOP_LT,
    JSOP_LE,
    JSOP_GT,
    JSOP_GE,
    JSOP_INSTANCEOF,
    JSOP_IN,
    JSOP_LSH,
    JSOP_RSH,
    JSOP_URSH,
    JSOP_ADD,
    JSOP_SUB,
    JSOP_MUL,
    JSOP_DIV,
    JSOP_MOD,
    JSOP_POW
};

static inline JSOp
BinaryOpParseNodeKindToJSOp(ParseNodeKind pnk)
{
    MOZ_ASSERT(pnk >= ParseNodeKind::BinOpFirst);
    MOZ_ASSERT(pnk <= ParseNodeKind::BinOpLast);
    return ParseNodeKindToJSOp[size_t(pnk) - size_t(ParseNodeKind::BinOpFirst)];
}

bool
BytecodeEmitter::emitRightAssociative(ListNode* node)
{
    // ** is the only right-associative operator.
    MOZ_ASSERT(node->isKind(ParseNodeKind::Pow));

    // Right-associative operator chain.
    for (ParseNode* subexpr : node->contents()) {
        if (!emitTree(subexpr)) {
            return false;
        }
    }
    for (uint32_t i = 0; i < node->count() - 1; i++) {
        if (!emit1(JSOP_POW)) {
            return false;
        }
    }
    return true;
}

bool
BytecodeEmitter::emitLeftAssociative(ListNode* node)
{
    // Left-associative operator chain.
    if (!emitTree(node->head())) {
        return false;
    }
    JSOp op = BinaryOpParseNodeKindToJSOp(node->getKind());
    ParseNode* nextExpr = node->head()->pn_next;
    do {
        if (!emitTree(nextExpr)) {
            return false;
        }
        if (!emit1(op)) {
            return false;
        }
    } while ((nextExpr = nextExpr->pn_next));
    return true;
}

bool
BytecodeEmitter::emitLogical(ListNode* node)
{
    MOZ_ASSERT(node->isKind(ParseNodeKind::Or) || node->isKind(ParseNodeKind::And));

    /*
     * JSOP_OR converts the operand on the stack to boolean, leaves the original
     * value on the stack and jumps if true; otherwise it falls into the next
     * bytecode, which pops the left operand and then evaluates the right operand.
     * The jump goes around the right operand evaluation.
     *
     * JSOP_AND converts the operand on the stack to boolean and jumps if false;
     * otherwise it falls into the right operand's bytecode.
     */

    TDZCheckCache tdzCache(this);

    /* Left-associative operator chain: avoid too much recursion. */
    ParseNode* expr = node->head();
    if (!emitTree(expr)) {
        return false;
    }
    JSOp op = node->isKind(ParseNodeKind::Or) ? JSOP_OR : JSOP_AND;
    JumpList jump;
    if (!emitJump(op, &jump)) {
        return false;
    }
    if (!emit1(JSOP_POP)) {
        return false;
    }

    /* Emit nodes between the head and the tail. */
    while ((expr = expr->pn_next)->pn_next) {
        if (!emitTree(expr)) {
            return false;
        }
        if (!emitJump(op, &jump)) {
            return false;
        }
        if (!emit1(JSOP_POP)) {
            return false;
        }
    }
    if (!emitTree(expr)) {
        return false;
    }

    if (!emitJumpTargetAndPatch(jump)) {
        return false;
    }
    return true;
}

bool
BytecodeEmitter::emitSequenceExpr(ListNode* node,
                                  ValueUsage valueUsage /* = ValueUsage::WantValue */)
{
    for (ParseNode* child = node->head(); ; child = child->pn_next) {
        if (!updateSourceCoordNotes(child->pn_pos.begin)) {
            return false;
        }
        if (!emitTree(child, child->pn_next ? ValueUsage::IgnoreValue : valueUsage)) {
            return false;
        }
        if (!child->pn_next) {
            break;
        }
        if (!emit1(JSOP_POP)) {
            return false;
        }
    }
    return true;
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See
// the comment on emitSwitch.
MOZ_NEVER_INLINE bool
BytecodeEmitter::emitIncOrDec(UnaryNode* incDec)
{
    switch (incDec->kid()->getKind()) {
      case ParseNodeKind::Dot:
        return emitPropIncDec(incDec);
      case ParseNodeKind::Elem:
        return emitElemIncDec(incDec);
      case ParseNodeKind::Call:
        return emitCallIncDec(incDec);
      default:
        return emitNameIncDec(incDec);
    }
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See
// the comment on emitSwitch.
MOZ_NEVER_INLINE bool
BytecodeEmitter::emitLabeledStatement(const LabeledStatement* pn)
{
    /*
     * Emit a JSOP_LABEL instruction. The argument is the offset to the statement
     * following the labeled statement.
     */
    uint32_t index;
    if (!makeAtomIndex(pn->label(), &index)) {
        return false;
    }

    JumpList top;
    if (!emitJump(JSOP_LABEL, &top)) {
        return false;
    }

    /* Emit code for the labeled statement. */
    LabelControl controlInfo(this, pn->label(), offset());

    if (!emitTree(pn->statement())) {
        return false;
    }

    /* Patch the JSOP_LABEL offset. */
    JumpTarget brk{ lastNonJumpTargetOffset() };
    patchJumpsToTarget(top, brk);

    if (!controlInfo.patchBreaks(this)) {
        return false;
    }

    return true;
}

bool
BytecodeEmitter::emitConditionalExpression(ConditionalExpression& conditional,
                                           ValueUsage valueUsage /* = ValueUsage::WantValue */)
{
    CondEmitter cond(this);
    if (!cond.emitCond()) {
        return false;
    }

    if (!emitTree(&conditional.condition())) {
        return false;
    }

    if (!cond.emitThenElse()) {
        return false;
    }

    if (!emitTree(&conditional.thenExpression(), valueUsage)) {
        return false;
    }

    if (!cond.emitElse()) {
        return false;
    }

    if (!emitTree(&conditional.elseExpression(), valueUsage)) {
        return false;
    }

    if (!cond.emitEnd()) {
        return false;
    }
    MOZ_ASSERT(cond.pushed() == 1);

    return true;
}

bool
BytecodeEmitter::emitPropertyList(ListNode* obj, MutableHandlePlainObject objp, PropListType type)
{
    for (ParseNode* propdef : obj->contents()) {
        if (propdef->is<ClassField>()) {
            // TODO(khyperia): Implement private field access.
            return false;
        }
        if (!updateSourceCoordNotes(propdef->pn_pos.begin)) {
            return false;
        }

        // Handle __proto__: v specially because *only* this form, and no other
        // involving "__proto__", performs [[Prototype]] mutation.
        if (propdef->isKind(ParseNodeKind::MutateProto)) {
            MOZ_ASSERT(type == ObjectLiteral);
            if (!emitTree(propdef->as<UnaryNode>().kid())) {
                return false;
            }
            objp.set(nullptr);
            if (!emit1(JSOP_MUTATEPROTO)) {
                return false;
            }
            continue;
        }

        if (propdef->isKind(ParseNodeKind::Spread)) {
            MOZ_ASSERT(type == ObjectLiteral);

            if (!emit1(JSOP_DUP)) {
                return false;
            }

            if (!emitTree(propdef->as<UnaryNode>().kid())) {
                return false;
            }

            if (!emitCopyDataProperties(CopyOption::Unfiltered)) {
                return false;
            }

            objp.set(nullptr);
            continue;
        }

        bool extraPop = false;
        if (type == ClassBody && propdef->as<ClassMethod>().isStatic()) {
            extraPop = true;
            if (!emit1(JSOP_DUP2)) {
                return false;
            }
            if (!emit1(JSOP_POP)) {
                return false;
            }
        }

        /* Emit an index for t[2] for later consumption by JSOP_INITELEM. */
        ParseNode* key = propdef->as<BinaryNode>().left();
        bool isIndex = false;
        if (key->isKind(ParseNodeKind::Number)) {
            if (!emitNumberOp(key->as<NumericLiteral>().value())) {
                return false;
            }
            isIndex = true;
        } else if (key->isKind(ParseNodeKind::ObjectPropertyName) ||
                   key->isKind(ParseNodeKind::String))
        {
            // EmitClass took care of constructor already.
            if (type == ClassBody && key->as<NameNode>().atom() == cx->names().constructor &&
                !propdef->as<ClassMethod>().isStatic())
            {
                continue;
            }
        } else {
            MOZ_ASSERT(key->isKind(ParseNodeKind::ComputedName));
            if (!emitComputedPropertyName(&key->as<UnaryNode>())) {
                return false;
            }
            isIndex = true;
        }

        /* Emit code for the property initializer. */
        ParseNode* propVal = propdef->as<BinaryNode>().right();
        if (!emitTree(propVal)) {
            return false;
        }

        JSOp op = propdef->getOp();
        MOZ_ASSERT(op == JSOP_INITPROP ||
                   op == JSOP_INITPROP_GETTER ||
                   op == JSOP_INITPROP_SETTER);

        FunctionPrefixKind prefixKind = op == JSOP_INITPROP_GETTER ? FunctionPrefixKind::Get
                                        : op == JSOP_INITPROP_SETTER ? FunctionPrefixKind::Set
                                        : FunctionPrefixKind::None;

        if (op == JSOP_INITPROP_GETTER || op == JSOP_INITPROP_SETTER) {
            objp.set(nullptr);
        }

        if (propVal->isKind(ParseNodeKind::Function) &&
            propVal->as<CodeNode>().funbox()->needsHomeObject())
        {
            FunctionBox* funbox = propVal->as<CodeNode>().funbox();
            MOZ_ASSERT(funbox->function()->allowSuperProperty());
            bool isAsync = funbox->isAsync();
            if (isAsync) {
                if (!emit1(JSOP_SWAP)) {
                    return false;
                }
            }
            if (!emit2(JSOP_INITHOMEOBJECT, isIndex + isAsync)) {
                return false;
            }
            if (isAsync) {
                if (!emit1(JSOP_POP)) {
                    return false;
                }
            }
        }

        // Class methods are not enumerable.
        if (type == ClassBody) {
            switch (op) {
              case JSOP_INITPROP:        op = JSOP_INITHIDDENPROP;          break;
              case JSOP_INITPROP_GETTER: op = JSOP_INITHIDDENPROP_GETTER;   break;
              case JSOP_INITPROP_SETTER: op = JSOP_INITHIDDENPROP_SETTER;   break;
              default: MOZ_CRASH("Invalid op");
            }
        }

        if (isIndex) {
            objp.set(nullptr);
            switch (op) {
              case JSOP_INITPROP:               op = JSOP_INITELEM;              break;
              case JSOP_INITHIDDENPROP:         op = JSOP_INITHIDDENELEM;        break;
              case JSOP_INITPROP_GETTER:        op = JSOP_INITELEM_GETTER;       break;
              case JSOP_INITHIDDENPROP_GETTER:  op = JSOP_INITHIDDENELEM_GETTER; break;
              case JSOP_INITPROP_SETTER:        op = JSOP_INITELEM_SETTER;       break;
              case JSOP_INITHIDDENPROP_SETTER:  op = JSOP_INITHIDDENELEM_SETTER; break;
              default: MOZ_CRASH("Invalid op");
            }
            if (propVal->isDirectRHSAnonFunction()) {
                if (!emitDupAt(1)) {
                    return false;
                }
                if (!emit2(JSOP_SETFUNNAME, uint8_t(prefixKind))) {
                    return false;
                }
            }
            if (!emit1(op)) {
                return false;
            }
        } else {
            MOZ_ASSERT(key->isKind(ParseNodeKind::ObjectPropertyName) ||
                       key->isKind(ParseNodeKind::String));

            uint32_t index;
            if (!makeAtomIndex(key->as<NameNode>().atom(), &index)) {
                return false;
            }

            if (objp) {
                MOZ_ASSERT(type == ObjectLiteral);
                MOZ_ASSERT(!IsHiddenInitOp(op));
                MOZ_ASSERT(!objp->inDictionaryMode());
                Rooted<jsid> id(cx, AtomToId(key->as<NameNode>().atom()));
                if (!NativeDefineDataProperty(cx, objp, id, UndefinedHandleValue,
                                              JSPROP_ENUMERATE))
                {
                    return false;
                }
                if (objp->inDictionaryMode()) {
                    objp.set(nullptr);
                }
            }

            if (propVal->isDirectRHSAnonFunction()) {
                MOZ_ASSERT(prefixKind == FunctionPrefixKind::None);

                RootedAtom keyName(cx, key->as<NameNode>().atom());
                if (!setOrEmitSetFunName(propVal, keyName)) {
                    return false;
                }
            }
            if (!emitIndex32(op, index)) {
                return false;
            }
        }

        if (extraPop) {
            if (!emit1(JSOP_POP)) {
                return false;
            }
        }
    }
    return true;
}

// Using MOZ_NEVER_INLINE in here is a workaround for llvm.org/pr14047. See
// the comment on emitSwitch.
MOZ_NEVER_INLINE bool
BytecodeEmitter::emitObject(ListNode* objNode)
{
    if (!objNode->hasNonConstInitializer() && objNode->head() && checkSingletonContext()) {
        return emitSingletonInitialiser(objNode);
    }

    /*
     * Emit code for {p:a, '%q':b, 2:c} that is equivalent to constructing
     * a new object and defining (in source order) each property on the object
     * (or mutating the object's [[Prototype]], in the case of __proto__).
     */
    ptrdiff_t offset = this->offset();
    if (!emitNewInit()) {
        return false;
    }

    // Try to construct the shape of the object as we go, so we can emit a
    // JSOP_NEWOBJECT with the final shape instead.
    // In the case of computed property names and indices, we cannot fix the
    // shape at bytecode compile time. When the shape cannot be determined,
    // |obj| is nulled out.

    // No need to do any guessing for the object kind, since we know the upper
    // bound of how many properties we plan to have.
    gc::AllocKind kind = gc::GetGCObjectKind(objNode->count());
    RootedPlainObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx, kind, TenuredObject));
    if (!obj) {
        return false;
    }

    if (!emitPropertyList(objNode, &obj, ObjectLiteral)) {
        return false;
    }

    if (obj) {
        // The object survived and has a predictable shape: update the original
        // bytecode.
        if (!replaceNewInitWithNewObject(obj, offset)) {
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::replaceNewInitWithNewObject(JSObject* obj, ptrdiff_t offset)
{
    ObjectBox* objbox = parser->newObjectBox(obj);
    if (!objbox) {
        return false;
    }

    static_assert(JSOP_NEWINIT_LENGTH == JSOP_NEWOBJECT_LENGTH,
                  "newinit and newobject must have equal length to edit in-place");

    uint32_t index = objectList.add(objbox);
    jsbytecode* code = this->code(offset);

    MOZ_ASSERT(code[0] == JSOP_NEWINIT);
    code[0] = JSOP_NEWOBJECT;
    SET_UINT32(code, index);

    return true;
}

bool
BytecodeEmitter::emitArrayLiteral(ListNode* array)
{
    if (!array->hasNonConstInitializer() && array->head()) {
        if (checkSingletonContext()) {
            // Bake in the object entirely if it will only be created once.
            return emitSingletonInitialiser(array);
        }

        // If the array consists entirely of primitive values, make a
        // template object with copy on write elements that can be reused
        // every time the initializer executes. Don't do this if the array is
        // small: copying the elements lazily is not worth it in that case.
        static const size_t MinElementsForCopyOnWrite = 5;
        if (emitterMode != BytecodeEmitter::SelfHosting &&
            array->count() >= MinElementsForCopyOnWrite)
        {
            RootedValue value(cx);
            if (!array->getConstantValue(cx, ParseNode::ForCopyOnWriteArray, &value)) {
                return false;
            }
            if (!value.isMagic(JS_GENERIC_MAGIC)) {
                // Note: the group of the template object might not yet reflect
                // that the object has copy on write elements. When the
                // interpreter or JIT compiler fetches the template, it should
                // use ObjectGroup::getOrFixupCopyOnWriteObject to make sure the
                // group for the template is accurate. We don't do this here as we
                // want to use ObjectGroup::allocationSiteGroup, which requires a
                // finished script.
                JSObject* obj = &value.toObject();
                MOZ_ASSERT(obj->is<ArrayObject>() &&
                           obj->as<ArrayObject>().denseElementsAreCopyOnWrite());

                ObjectBox* objbox = parser->newObjectBox(obj);
                if (!objbox) {
                    return false;
                }

                return emitObjectOp(objbox, JSOP_NEWARRAY_COPYONWRITE);
            }
        }
    }

    return emitArray(array->head(), array->count());
}

bool
BytecodeEmitter::emitArray(ParseNode* arrayHead, uint32_t count)
{
    /*
     * Emit code for [a, b, c] that is equivalent to constructing a new
     * array and in source order evaluating each element value and adding
     * it to the array, without invoking latent setters.  We use the
     * JSOP_NEWINIT and JSOP_INITELEM_ARRAY bytecodes to ignore setters and
     * to avoid dup'ing and popping the array as each element is added, as
     * JSOP_SETELEM/JSOP_SETPROP would do.
     */

    uint32_t nspread = 0;
    for (ParseNode* elem = arrayHead; elem; elem = elem->pn_next) {
        if (elem->isKind(ParseNodeKind::Spread)) {
            nspread++;
        }
    }

    // Array literal's length is limited to NELEMENTS_LIMIT in parser.
    static_assert(NativeObject::MAX_DENSE_ELEMENTS_COUNT <= INT32_MAX,
                  "array literals' maximum length must not exceed limits "
                  "required by BaselineCompiler::emit_JSOP_NEWARRAY, "
                  "BaselineCompiler::emit_JSOP_INITELEM_ARRAY, "
                  "and DoSetElemFallback's handling of JSOP_INITELEM_ARRAY");
    MOZ_ASSERT(count >= nspread);
    MOZ_ASSERT(count <= NativeObject::MAX_DENSE_ELEMENTS_COUNT,
               "the parser must throw an error if the array exceeds maximum "
               "length");

    // For arrays with spread, this is a very pessimistic allocation, the
    // minimum possible final size.
    if (!emitUint32Operand(JSOP_NEWARRAY, count - nspread)) {       // ARRAY
        return false;
    }

    ParseNode* elem = arrayHead;
    uint32_t index;
    bool afterSpread = false;
    for (index = 0; elem; index++, elem = elem->pn_next) {
        if (!afterSpread && elem->isKind(ParseNodeKind::Spread)) {
            afterSpread = true;
            if (!emitNumberOp(index)) {                             // ARRAY INDEX
                return false;
            }
        }
        if (!updateSourceCoordNotes(elem->pn_pos.begin)) {
            return false;
        }

        bool allowSelfHostedIter = false;
        if (elem->isKind(ParseNodeKind::Elision)) {
            if (!emit1(JSOP_HOLE)) {
                return false;
            }
        } else {
            ParseNode* expr;
            if (elem->isKind(ParseNodeKind::Spread)) {
                expr = elem->as<UnaryNode>().kid();

                if (emitterMode == BytecodeEmitter::SelfHosting &&
                    expr->isKind(ParseNodeKind::Call) &&
                    expr->as<BinaryNode>().left()->isName(cx->names().allowContentIter)) {
                    allowSelfHostedIter = true;
                }
            } else {
                expr = elem;
            }
            if (!emitTree(expr)) {                                       // ARRAY INDEX? VALUE
                return false;
            }
        }
        if (elem->isKind(ParseNodeKind::Spread)) {
            if (!emitIterator()) {                                       // ARRAY INDEX NEXT ITER
                return false;
            }
            if (!emit2(JSOP_PICK, 3)) {                                  // INDEX NEXT ITER ARRAY
                return false;
            }
            if (!emit2(JSOP_PICK, 3)) {                                  // NEXT ITER ARRAY INDEX
                return false;
            }
            if (!emitSpread(allowSelfHostedIter)) {                      // ARRAY INDEX
                return false;
            }
        } else if (afterSpread) {
            if (!emit1(JSOP_INITELEM_INC)) {
                return false;
            }
        } else {
            if (!emitUint32Operand(JSOP_INITELEM_ARRAY, index)) {
                return false;
            }
        }
    }
    MOZ_ASSERT(index == count);
    if (afterSpread) {
        if (!emit1(JSOP_POP)) {                                          // ARRAY
            return false;
        }
    }
    return true;
}

static inline JSOp
UnaryOpParseNodeKindToJSOp(ParseNodeKind pnk)
{
    switch (pnk) {
      case ParseNodeKind::Throw: return JSOP_THROW;
      case ParseNodeKind::Void: return JSOP_VOID;
      case ParseNodeKind::Not: return JSOP_NOT;
      case ParseNodeKind::BitNot: return JSOP_BITNOT;
      case ParseNodeKind::Pos: return JSOP_POS;
      case ParseNodeKind::Neg: return JSOP_NEG;
      default: MOZ_CRASH("unexpected unary op");
    }
}

bool
BytecodeEmitter::emitUnary(UnaryNode* unaryNode)
{
    if (!updateSourceCoordNotes(unaryNode->pn_pos.begin)) {
        return false;
    }
    if (!emitTree(unaryNode->kid())) {
        return false;
    }
    return emit1(UnaryOpParseNodeKindToJSOp(unaryNode->getKind()));
}

bool
BytecodeEmitter::emitTypeof(UnaryNode* typeofNode, JSOp op)
{
    MOZ_ASSERT(op == JSOP_TYPEOF || op == JSOP_TYPEOFEXPR);

    if (!updateSourceCoordNotes(typeofNode->pn_pos.begin)) {
        return false;
    }

    if (!emitTree(typeofNode->kid())) {
        return false;
    }

    return emit1(op);
}

bool
BytecodeEmitter::emitFunctionFormalParametersAndBody(ListNode* paramsBody)
{
    MOZ_ASSERT(paramsBody->isKind(ParseNodeKind::ParamsBody));

    ParseNode* funBody = paramsBody->last();
    FunctionBox* funbox = sc->asFunctionBox();

    TDZCheckCache tdzCache(this);

    if (funbox->hasParameterExprs) {
        EmitterScope funEmitterScope(this);
        if (!funEmitterScope.enterFunction(this, funbox)) {
            return false;
        }

        if (!emitInitializeFunctionSpecialNames()) {
            return false;
        }

        if (!emitFunctionFormalParameters(paramsBody)) {
            return false;
        }

        {
            Maybe<EmitterScope> extraVarEmitterScope;

            if (funbox->hasExtraBodyVarScope()) {
                extraVarEmitterScope.emplace(this);
                if (!extraVarEmitterScope->enterFunctionExtraBodyVar(this, funbox)) {
                    return false;
                }

                // After emitting expressions for all parameters, copy over any
                // formal parameters which have been redeclared as vars. For
                // example, in the following, the var y in the body scope is 42:
                //
                //   function f(x, y = 42) { var y; }
                //
                RootedAtom name(cx);
                if (funbox->extraVarScopeBindings() && funbox->functionScopeBindings()) {
                    for (BindingIter bi(*funbox->functionScopeBindings(), true); bi; bi++) {
                        name = bi.name();

                        // There may not be a var binding of the same name.
                        if (!locationOfNameBoundInScope(name, extraVarEmitterScope.ptr())) {
                            continue;
                        }

                        // The '.this' and '.generator' function special
                        // bindings should never appear in the extra var
                        // scope. 'arguments', however, may.
                        MOZ_ASSERT(name != cx->names().dotThis &&
                                   name != cx->names().dotGenerator);

                        NameOpEmitter noe(this, name, NameOpEmitter::Kind::Initialize);
                        if (!noe.prepareForRhs()) {
                            return false;
                        }

                        NameLocation paramLoc = *locationOfNameBoundInScope(name, &funEmitterScope);
                        if (!emitGetNameAtLocation(name, paramLoc)) {
                            return false;
                        }
                        if (!noe.emitAssignment()) {
                            return false;
                        }
                        if (!emit1(JSOP_POP)) {
                            return false;
                        }
                    }
                }
            }

            if (!emitFunctionBody(funBody)) {
                return false;
            }

            if (extraVarEmitterScope && !extraVarEmitterScope->leave(this)) {
                return false;
            }
        }

        return funEmitterScope.leave(this);
    }

    // No parameter expressions. Enter the function body scope and emit
    // everything.
    //
    // One caveat is that Debugger considers ops in the prologue to be
    // unreachable (i.e. cannot set a breakpoint on it). If there are no
    // parameter exprs, any unobservable environment ops (like pushing the
    // call object, setting '.this', etc) need to go in the prologue, else it
    // messes up breakpoint tests.
    EmitterScope emitterScope(this);

    switchToPrologue();
    if (!emitterScope.enterFunction(this, funbox)) {
        return false;
    }

    if (!emitInitializeFunctionSpecialNames()) {
        return false;
    }
    switchToMain();

    if (!emitFunctionFormalParameters(paramsBody)) {
        return false;
    }

    if (!emitFunctionBody(funBody)) {
        return false;
    }

    return emitterScope.leave(this);
}

bool
BytecodeEmitter::emitFunctionFormalParameters(ListNode* paramsBody)
{
    ParseNode* funBody = paramsBody->last();
    FunctionBox* funbox = sc->asFunctionBox();
    EmitterScope* funScope = innermostEmitterScope();

    bool hasParameterExprs = funbox->hasParameterExprs;
    bool hasRest = funbox->hasRest();

    uint16_t argSlot = 0;
    for (ParseNode* arg = paramsBody->head(); arg != funBody; arg = arg->pn_next, argSlot++) {
        ParseNode* bindingElement = arg;
        ParseNode* initializer = nullptr;
        if (arg->isKind(ParseNodeKind::Assign)) {
            bindingElement = arg->as<AssignmentNode>().left();
            initializer = arg->as<AssignmentNode>().right();
        }

        // Left-hand sides are either simple names or destructuring patterns.
        MOZ_ASSERT(bindingElement->isKind(ParseNodeKind::Name) ||
                   bindingElement->isKind(ParseNodeKind::Array) ||
                   bindingElement->isKind(ParseNodeKind::Object));

        // The rest parameter doesn't have an initializer.
        bool isRest = hasRest && arg->pn_next == funBody;
        MOZ_ASSERT_IF(isRest, !initializer);

        bool isDestructuring = !bindingElement->isKind(ParseNodeKind::Name);

        // ES 14.1.19 says if BindingElement contains an expression in the
        // production FormalParameter : BindingElement, it is evaluated in a
        // new var environment. This is needed to prevent vars from escaping
        // direct eval in parameter expressions.
        Maybe<EmitterScope> paramExprVarScope;
        if (funbox->hasDirectEvalInParameterExpr && (isDestructuring || initializer)) {
            paramExprVarScope.emplace(this);
            if (!paramExprVarScope->enterParameterExpressionVar(this)) {
                return false;
            }
        }

        // First push the RHS if there is a default expression or if it is
        // rest.

        if (initializer) {
            // If we have an initializer, emit the initializer and assign it
            // to the argument slot. TDZ is taken care of afterwards.
            MOZ_ASSERT(hasParameterExprs);

            if (!emitArgOp(JSOP_GETARG, argSlot)) {       // ARG
                return false;
            }

            if (!emitDefault(initializer, bindingElement)) {
                return false;                             // ARG/DEFAULT
            }
        } else if (isRest) {
            if (!emit1(JSOP_REST)) {
                return false;
            }
            checkTypeSet(JSOP_REST);
        }

        // Initialize the parameter name.

        if (isDestructuring) {
            // If we had an initializer or the rest parameter, the value is
            // already on the stack.
            if (!initializer && !isRest && !emitArgOp(JSOP_GETARG, argSlot)) {
                return false;
            }

            // If there's an parameter expression var scope, the destructuring
            // declaration needs to initialize the name in the function scope,
            // which is not the innermost scope.
            if (!emitDestructuringOps(&bindingElement->as<ListNode>(),
                                      paramExprVarScope
                                      ? DestructuringFormalParameterInVarScope
                                      : DestructuringDeclaration))
            {
                return false;
            }

            if (!emit1(JSOP_POP)) {
                return false;
            }
        } else if (hasParameterExprs || isRest) {
            RootedAtom paramName(cx, bindingElement->as<NameNode>().name());
            NameLocation paramLoc = *locationOfNameBoundInScope(paramName, funScope);
            NameOpEmitter noe(this, paramName, paramLoc, NameOpEmitter::Kind::Initialize);
            if (!noe.prepareForRhs()) {
                return false;
            }
            if (hasParameterExprs) {
                // If we had an initializer or a rest parameter, the value is
                // already on the stack.
                if (!initializer && !isRest) {
                    if (!emitArgOp(JSOP_GETARG, argSlot)) {
                        return false;
                    }
                }
            }
            if (!noe.emitAssignment()) {
                return false;
            }
            if (!emit1(JSOP_POP)) {
                return false;
            }
        }

        if (paramExprVarScope) {
            if (!paramExprVarScope->leave(this)) {
                return false;
            }
        }
    }

    return true;
}

bool
BytecodeEmitter::emitInitializeFunctionSpecialNames()
{
    FunctionBox* funbox = sc->asFunctionBox();

    auto emitInitializeFunctionSpecialName = [](BytecodeEmitter* bce, HandlePropertyName name,
                                                JSOp op)
    {
        // A special name must be slotful, either on the frame or on the
        // call environment.
        MOZ_ASSERT(bce->lookupName(name).hasKnownSlot());

        NameOpEmitter noe(bce, name, NameOpEmitter::Kind::Initialize);
        if (!noe.prepareForRhs()) {
            return false;
        }
        if (!bce->emit1(op)) {
            return false;
        }
        if (!noe.emitAssignment()) {
            return false;
        }
        if (!bce->emit1(JSOP_POP)) {
            return false;
        }

        return true;
    };

    // Do nothing if the function doesn't have an arguments binding.
    if (funbox->argumentsHasLocalBinding()) {
        if (!emitInitializeFunctionSpecialName(this, cx->names().arguments, JSOP_ARGUMENTS)) {
            return false;
        }
    }

    // Do nothing if the function doesn't have a this-binding (this
    // happens for instance if it doesn't use this/eval or if it's an
    // arrow function).
    if (funbox->hasThisBinding()) {
        if (!emitInitializeFunctionSpecialName(this, cx->names().dotThis, JSOP_FUNCTIONTHIS)) {
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitFunctionBody(ParseNode* funBody)
{
    FunctionBox* funbox = sc->asFunctionBox();

    if (!emitTree(funBody)) {
        return false;
    }

    if (funbox->needsFinalYield()) {
        // If we fall off the end of a generator, do a final yield.
        bool needsIteratorResult = funbox->needsIteratorResult();
        if (needsIteratorResult) {
            if (!emitPrepareIteratorResult()) {
                return false;
            }
        }

        if (!emit1(JSOP_UNDEFINED)) {
            return false;
        }

        if (needsIteratorResult) {
            if (!emitFinishIteratorResult(true)) {
                return false;
            }
        }

        if (!emit1(JSOP_SETRVAL)) {
            return false;
        }

        if (!emitGetDotGeneratorInInnermostScope()) {
            return false;
        }

        // No need to check for finally blocks, etc as in EmitReturn.
        if (!emitYieldOp(JSOP_FINALYIELDRVAL)) {
            return false;
        }
    } else {
        // Non-generator functions just return |undefined|. The
        // JSOP_RETRVAL emitted below will do that, except if the
        // script has a finally block: there can be a non-undefined
        // value in the return value slot. Make sure the return value
        // is |undefined|.
        if (hasTryFinally) {
            if (!emit1(JSOP_UNDEFINED)) {
                return false;
            }
            if (!emit1(JSOP_SETRVAL)) {
                return false;
            }
        }
    }

    if (funbox->isDerivedClassConstructor()) {
        if (!emitCheckDerivedClassConstructorReturn()) {
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitLexicalInitialization(NameNode* name)
{
    NameOpEmitter noe(this, name->name(), NameOpEmitter::Kind::Initialize);
    if (!noe.prepareForRhs()) {
        return false;
    }

    // The caller has pushed the RHS to the top of the stack. Assert that the
    // name is lexical and no BIND[G]NAME ops were emitted.
    MOZ_ASSERT(noe.loc().isLexical());
    MOZ_ASSERT(!noe.emittedBindOp());

    if (!noe.emitAssignment()) {
        return false;
    }

    return true;
}

// This follows ES6 14.5.14 (ClassDefinitionEvaluation) and ES6 14.5.15
// (BindingClassDeclarationEvaluation).
bool
BytecodeEmitter::emitClass(ClassNode* classNode)
{
    ClassNames* names = classNode->names();
    ParseNode* heritageExpression = classNode->heritage();
    ListNode* classMembers = classNode->memberList();
    CodeNode* constructor = nullptr;
    for (ParseNode* mn : classMembers->contents()) {
        if (mn->is<ClassField>()) {
            // TODO(khyperia): Implement private field access.
            return false;
        }
        ClassMethod& method = mn->as<ClassMethod>();
        ParseNode& methodName = method.name();
        if (!method.isStatic() &&
            (methodName.isKind(ParseNodeKind::ObjectPropertyName) ||
             methodName.isKind(ParseNodeKind::String)) &&
            methodName.as<NameNode>().atom() == cx->names().constructor)
        {
            constructor = &method.method();
            break;
        }
    }

    bool savedStrictness = sc->setLocalStrictMode(true);

    Maybe<TDZCheckCache> tdzCache;
    Maybe<EmitterScope> emitterScope;
    if (names) {
        tdzCache.emplace(this);
        emitterScope.emplace(this);
        if (!emitterScope->enterLexical(this, ScopeKind::Lexical, classNode->scopeBindings())) {
            return false;
        }
    }

    // Pseudocode for class declarations:
    //
    //     class extends BaseExpression {
    //       constructor() { ... }
    //       ...
    //       }
    //
    //
    //   if defined <BaseExpression> {
    //     let heritage = BaseExpression;
    //
    //     if (heritage !== null) {
    //       funProto = heritage;
    //       objProto = heritage.prototype;
    //     } else {
    //       funProto = %FunctionPrototype%;
    //       objProto = null;
    //     }
    //   } else {
    //     objProto = %ObjectPrototype%;
    //   }
    //
    //   let homeObject = ObjectCreate(objProto);
    //
    //   if defined <constructor> {
    //     if defined <BaseExpression> {
    //       cons = DefineMethod(<constructor>, proto=homeObject, funProto=funProto);
    //     } else {
    //       cons = DefineMethod(<constructor>, proto=homeObject);
    //     }
    //   } else {
    //     if defined <BaseExpression> {
    //       cons = DefaultDerivedConstructor(proto=homeObject, funProto=funProto);
    //     } else {
    //       cons = DefaultConstructor(proto=homeObject);
    //     }
    //   }
    //
    //   cons.prototype = homeObject;
    //   homeObject.constructor = cons;
    //
    //   EmitPropertyList(...)

    // This is kind of silly. In order to the get the home object defined on
    // the constructor, we have to make it second, but we want the prototype
    // on top for EmitPropertyList, because we expect static properties to be
    // rarer. The result is a few more swaps than we would like. Such is life.
    if (heritageExpression) {
        InternalIfEmitter ifThenElse(this);

        if (!emitTree(heritageExpression)) {                    // ... HERITAGE
            return false;
        }

        // Heritage must be null or a non-generator constructor
        if (!emit1(JSOP_CHECKCLASSHERITAGE)) {                  // ... HERITAGE
            return false;
        }

        // [IF] (heritage !== null)
        if (!emit1(JSOP_DUP)) {                                 // ... HERITAGE HERITAGE
            return false;
        }
        if (!emit1(JSOP_NULL)) {                                // ... HERITAGE HERITAGE NULL
            return false;
        }
        if (!emit1(JSOP_STRICTNE)) {                            // ... HERITAGE NE
            return false;
        }

        // [THEN] funProto = heritage, objProto = heritage.prototype
        if (!ifThenElse.emitThenElse()) {
            return false;
        }
        if (!emit1(JSOP_DUP)) {                                 // ... HERITAGE HERITAGE
            return false;
        }
        if (!emitAtomOp(cx->names().prototype, JSOP_GETPROP)) { // ... HERITAGE PROTO
            return false;
        }

        // [ELSE] funProto = %FunctionPrototype%, objProto = null
        if (!ifThenElse.emitElse()) {
            return false;
        }
        if (!emit1(JSOP_POP)) {                                 // ...
            return false;
        }
        if (!emit2(JSOP_BUILTINPROTO, JSProto_Function)) {      // ... PROTO
            return false;
        }
        if (!emit1(JSOP_NULL)) {                                // ... PROTO NULL
            return false;
        }

        // [ENDIF]
        if (!ifThenElse.emitEnd()) {
            return false;
        }

        if (!emit1(JSOP_OBJWITHPROTO)) {                        // ... HERITAGE HOMEOBJ
            return false;
        }
        if (!emit1(JSOP_SWAP)) {                                // ... HOMEOBJ HERITAGE
            return false;
        }
    } else {
        if (!emitNewInit()) {                                   // ... HOMEOBJ
            return false;
        }
    }

    // Stack currently has HOMEOBJ followed by optional HERITAGE. When HERITAGE
    // is not used, an implicit value of %FunctionPrototype% is implied.

    if (constructor) {
        if (!emitFunction(constructor, !!heritageExpression)) { // ... HOMEOBJ CONSTRUCTOR
            return false;
        }
        if (constructor->funbox()->needsHomeObject()) {
            if (!emit2(JSOP_INITHOMEOBJECT, 0)) {               // ... HOMEOBJ CONSTRUCTOR
                return false;
            }
        }
    } else {
        // In the case of default class constructors, emit the start and end
        // offsets in the source buffer as source notes so that when we
        // actually make the constructor during execution, we can give it the
        // correct toString output.
        ptrdiff_t classStart = ptrdiff_t(classNode->pn_pos.begin);
        ptrdiff_t classEnd = ptrdiff_t(classNode->pn_pos.end);
        if (!newSrcNote3(SRC_CLASS_SPAN, classStart, classEnd)) {
            return false;
        }

        JSAtom *name = names ? names->innerBinding()->as<NameNode>().atom() : cx->names().empty;
        if (heritageExpression) {
            if (!emitAtomOp(name, JSOP_DERIVEDCONSTRUCTOR)) {   // ... HOMEOBJ CONSTRUCTOR
                return false;
            }
        } else {
            if (!emitAtomOp(name, JSOP_CLASSCONSTRUCTOR)) {     // ... HOMEOBJ CONSTRUCTOR
                return false;
            }
        }
    }

    if (!emit1(JSOP_SWAP)) {                                    // ... CONSTRUCTOR HOMEOBJ
        return false;
    }

    if (!emit1(JSOP_DUP2)) {                                        // ... CONSTRUCTOR HOMEOBJ CONSTRUCTOR HOMEOBJ
        return false;
    }
    if (!emitAtomOp(cx->names().prototype, JSOP_INITLOCKEDPROP)) {  // ... CONSTRUCTOR HOMEOBJ CONSTRUCTOR
        return false;
    }
    if (!emitAtomOp(cx->names().constructor, JSOP_INITHIDDENPROP)) {  // ... CONSTRUCTOR HOMEOBJ
        return false;
    }

    RootedPlainObject obj(cx);
    if (!emitPropertyList(classMembers, &obj, ClassBody)) {     // ... CONSTRUCTOR HOMEOBJ
        return false;
    }

    if (!emit1(JSOP_POP)) {                                     // ... CONSTRUCTOR
        return false;
    }

    if (names) {
        NameNode* innerName = names->innerBinding();
        if (!emitLexicalInitialization(innerName)) {            // ... CONSTRUCTOR
            return false;
        }

        // Pop the inner scope.
        if (!emitterScope->leave(this)) {
            return false;
        }
        emitterScope.reset();

        if (NameNode* outerName = names->outerBinding()) {
            if (!emitLexicalInitialization(outerName)) {        // ... CONSTRUCTOR
                return false;
            }
            // Only class statements make outer bindings, and they do not leave
            // themselves on the stack.
            if (!emit1(JSOP_POP)) {                             // ...
                return false;
            }
        }
    }

    // The CONSTRUCTOR is left on stack if this is an expression.

    MOZ_ALWAYS_TRUE(sc->setLocalStrictMode(savedStrictness));

    return true;
}

bool
BytecodeEmitter::emitExportDefault(BinaryNode* exportNode)
{
    MOZ_ASSERT(exportNode->isKind(ParseNodeKind::ExportDefault));

    ParseNode* nameNode = exportNode->left();
    if (!emitTree(nameNode)) {
        return false;
    }

    if (ParseNode* binding = exportNode->right()) {
        if (!emitLexicalInitialization(&binding->as<NameNode>())) {
            return false;
        }

        if (nameNode->isDirectRHSAnonFunction()) {
            HandlePropertyName name = cx->names().default_;
            if (!setOrEmitSetFunName(nameNode, name)) {
                return false;
            }
        }

        if (!emit1(JSOP_POP)) {
            return false;
        }
    }

    return true;
}

bool
BytecodeEmitter::emitTree(ParseNode* pn, ValueUsage valueUsage /* = ValueUsage::WantValue */,
                          EmitLineNumberNote emitLineNote /* = EMIT_LINENOTE */)
{
    if (!CheckRecursionLimit(cx)) {
        return false;
    }

    EmitLevelManager elm(this);

    /* Emit notes to tell the current bytecode's source line number.
       However, a couple trees require special treatment; see the
       relevant emitter functions for details. */
    if (emitLineNote == EMIT_LINENOTE && !ParseNodeRequiresSpecialLineNumberNotes(pn)) {
        if (!updateLineNumberNotes(pn->pn_pos.begin)) {
            return false;
        }
    }

    switch (pn->getKind()) {
      case ParseNodeKind::Function:
        if (!emitFunction(&pn->as<CodeNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::ParamsBody:
        if (!emitFunctionFormalParametersAndBody(&pn->as<ListNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::If:
        if (!emitIf(&pn->as<TernaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Switch:
        if (!emitSwitch(&pn->as<SwitchStatement>())) {
            return false;
        }
        break;

      case ParseNodeKind::While:
        if (!emitWhile(&pn->as<BinaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::DoWhile:
        if (!emitDo(&pn->as<BinaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::For:
        if (!emitFor(&pn->as<ForNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Break:
        // Ensure that the column of the 'break' is set properly.
        if (!updateSourceCoordNotes(pn->pn_pos.begin)) {
            return false;
        }

        if (!emitBreak(pn->as<BreakStatement>().label())) {
            return false;
        }
        break;

      case ParseNodeKind::Continue:
        // Ensure that the column of the 'continue' is set properly.
        if (!updateSourceCoordNotes(pn->pn_pos.begin)) {
            return false;
        }

        if (!emitContinue(pn->as<ContinueStatement>().label())) {
            return false;
        }
        break;

      case ParseNodeKind::With:
        if (!emitWith(&pn->as<BinaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Try:
        if (!emitTry(&pn->as<TryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Catch:
        if (!emitCatch(&pn->as<BinaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Var:
        if (!emitDeclarationList(&pn->as<ListNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Return:
        if (!emitReturn(&pn->as<UnaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::YieldStar:
        if (!emitYieldStar(pn->as<UnaryNode>().kid())) {
            return false;
        }
        break;

      case ParseNodeKind::Generator:
        if (!emit1(JSOP_GENERATOR)) {
            return false;
        }
        break;

      case ParseNodeKind::InitialYield:
        if (!emitInitialYield(&pn->as<UnaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Yield:
        if (!emitYield(&pn->as<UnaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Await:
        if (!emitAwaitInInnermostScope(&pn->as<UnaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::StatementList:
        if (!emitStatementList(&pn->as<ListNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::EmptyStatement:
        break;

      case ParseNodeKind::ExpressionStatement:
        if (!emitExpressionStatement(&pn->as<UnaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Label:
        if (!emitLabeledStatement(&pn->as<LabeledStatement>())) {
            return false;
        }
        break;

      case ParseNodeKind::Comma:
        if (!emitSequenceExpr(&pn->as<ListNode>(), valueUsage)) {
            return false;
        }
        break;

      case ParseNodeKind::Assign:
      case ParseNodeKind::AddAssign:
      case ParseNodeKind::SubAssign:
      case ParseNodeKind::BitOrAssign:
      case ParseNodeKind::BitXorAssign:
      case ParseNodeKind::BitAndAssign:
      case ParseNodeKind::LshAssign:
      case ParseNodeKind::RshAssign:
      case ParseNodeKind::UrshAssign:
      case ParseNodeKind::MulAssign:
      case ParseNodeKind::DivAssign:
      case ParseNodeKind::ModAssign:
      case ParseNodeKind::PowAssign: {
        AssignmentNode* assignNode = &pn->as<AssignmentNode>();
        if (!emitAssignment(assignNode->left(),
                            CompoundAssignmentParseNodeKindToJSOp(assignNode->getKind()),
                            assignNode->right()))
        {
            return false;
        }
        break;
      }

      case ParseNodeKind::Conditional:
        if (!emitConditionalExpression(pn->as<ConditionalExpression>(), valueUsage)) {
            return false;
        }
        break;

      case ParseNodeKind::Or:
      case ParseNodeKind::And:
        if (!emitLogical(&pn->as<ListNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Add:
      case ParseNodeKind::Sub:
      case ParseNodeKind::BitOr:
      case ParseNodeKind::BitXor:
      case ParseNodeKind::BitAnd:
      case ParseNodeKind::StrictEq:
      case ParseNodeKind::Eq:
      case ParseNodeKind::StrictNe:
      case ParseNodeKind::Ne:
      case ParseNodeKind::Lt:
      case ParseNodeKind::Le:
      case ParseNodeKind::Gt:
      case ParseNodeKind::Ge:
      case ParseNodeKind::In:
      case ParseNodeKind::InstanceOf:
      case ParseNodeKind::Lsh:
      case ParseNodeKind::Rsh:
      case ParseNodeKind::Ursh:
      case ParseNodeKind::Star:
      case ParseNodeKind::Div:
      case ParseNodeKind::Mod:
        if (!emitLeftAssociative(&pn->as<ListNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Pow:
        if (!emitRightAssociative(&pn->as<ListNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Pipeline:
        if (!emitPipeline(&pn->as<ListNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::TypeOfName:
        if (!emitTypeof(&pn->as<UnaryNode>(), JSOP_TYPEOF)) {
            return false;
        }
        break;

      case ParseNodeKind::TypeOfExpr:
        if (!emitTypeof(&pn->as<UnaryNode>(), JSOP_TYPEOFEXPR)) {
            return false;
        }
        break;

      case ParseNodeKind::Throw:
      case ParseNodeKind::Void:
      case ParseNodeKind::Not:
      case ParseNodeKind::BitNot:
      case ParseNodeKind::Pos:
      case ParseNodeKind::Neg:
        if (!emitUnary(&pn->as<UnaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::PreIncrement:
      case ParseNodeKind::PreDecrement:
      case ParseNodeKind::PostIncrement:
      case ParseNodeKind::PostDecrement:
        if (!emitIncOrDec(&pn->as<UnaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::DeleteName:
        if (!emitDeleteName(&pn->as<UnaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::DeleteProp:
        if (!emitDeleteProperty(&pn->as<UnaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::DeleteElem:
        if (!emitDeleteElement(&pn->as<UnaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::DeleteExpr:
        if (!emitDeleteExpression(&pn->as<UnaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Dot: {
        PropertyAccess* prop = &pn->as<PropertyAccess>();
        // TODO(khyperia): Implement private field access.
        bool isSuper = prop->isSuper();
        PropOpEmitter poe(this,
                          PropOpEmitter::Kind::Get,
                          isSuper
                          ? PropOpEmitter::ObjKind::Super
                          : PropOpEmitter::ObjKind::Other);
        if (!poe.prepareForObj()) {
            return false;
        }
        if (isSuper) {
            UnaryNode* base = &prop->expression().as<UnaryNode>();
            if (!emitGetThisForSuperBase(base)) {         // THIS
                return false;
            }
        } else {
            if (!emitPropLHS(prop)) {                     // OBJ
                return false;
            }
        }
        if (!poe.emitGet(prop->key().atom())) {           // PROP
            return false;
        }
        break;
      }

      case ParseNodeKind::Elem: {
        PropertyByValue* elem = &pn->as<PropertyByValue>();
        bool isSuper = elem->isSuper();
        ElemOpEmitter eoe(this,
                          ElemOpEmitter::Kind::Get,
                          isSuper
                          ? ElemOpEmitter::ObjKind::Super
                          : ElemOpEmitter::ObjKind::Other);
        if (!emitElemObjAndKey(elem, isSuper, eoe)) {     // [Super]
            //                                            // THIS KEY
            //                                            // [Other]
            //                                            // OBJ KEY
            return false;
        }
        if (!eoe.emitGet()) {                             // ELEM
            return false;
        }

        break;
      }

      case ParseNodeKind::New:
      case ParseNodeKind::TaggedTemplate:
      case ParseNodeKind::Call:
      case ParseNodeKind::SuperCall:
        if (!emitCallOrNew(&pn->as<BinaryNode>(), valueUsage)) {
            return false;
        }
        break;

      case ParseNodeKind::LexicalScope:
        if (!emitLexicalScope(&pn->as<LexicalScopeNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Const:
      case ParseNodeKind::Let:
        if (!emitDeclarationList(&pn->as<ListNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Import:
        MOZ_ASSERT(sc->isModuleContext());
        break;

      case ParseNodeKind::Export: {
        MOZ_ASSERT(sc->isModuleContext());
        UnaryNode* node = &pn->as<UnaryNode>();
        ParseNode* decl = node->kid();
        if (decl->getKind() != ParseNodeKind::ExportSpecList) {
            if (!emitTree(decl)) {
                return false;
            }
        }
        break;
      }

      case ParseNodeKind::ExportDefault:
        MOZ_ASSERT(sc->isModuleContext());
        if (!emitExportDefault(&pn->as<BinaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::ExportFrom:
        MOZ_ASSERT(sc->isModuleContext());
        break;

      case ParseNodeKind::CallSiteObj:
        if (!emitCallSiteObject(&pn->as<CallSiteNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Array:
        if (!emitArrayLiteral(&pn->as<ListNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Object:
        if (!emitObject(&pn->as<ListNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::Name:
        if (!emitGetName(&pn->as<NameNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::TemplateStringList:
        if (!emitTemplateString(&pn->as<ListNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::TemplateString:
      case ParseNodeKind::String:
        if (!emitAtomOp(pn->as<NameNode>().atom(), JSOP_STRING)) {
            return false;
        }
        break;

      case ParseNodeKind::Number:
        if (!emitNumberOp(pn->as<NumericLiteral>().value())) {
            return false;
        }
        break;

      case ParseNodeKind::RegExp:
        if (!emitRegExp(objectList.add(pn->as<RegExpLiteral>().objbox()))) {
            return false;
        }
        break;

      case ParseNodeKind::True:
      case ParseNodeKind::False:
      case ParseNodeKind::Null:
      case ParseNodeKind::RawUndefined:
        if (!emit1(pn->getOp())) {
            return false;
        }
        break;

      case ParseNodeKind::This:
        if (!emitThisLiteral(&pn->as<ThisLiteral>())) {
            return false;
        }
        break;

      case ParseNodeKind::Debugger:
        if (!updateSourceCoordNotes(pn->pn_pos.begin)) {
            return false;
        }
        if (!emit1(JSOP_DEBUGGER)) {
            return false;
        }
        break;

      case ParseNodeKind::Class:
        if (!emitClass(&pn->as<ClassNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::NewTarget:
        if (!emit1(JSOP_NEWTARGET)) {
            return false;
        }
        break;

      case ParseNodeKind::ImportMeta:
        if (!emit1(JSOP_IMPORTMETA)) {
            return false;
        }
        break;

      case ParseNodeKind::CallImport:
        if (!emitTree(pn->as<BinaryNode>().right()) || !emit1(JSOP_DYNAMIC_IMPORT)) {
            return false;
        }
        break;

      case ParseNodeKind::SetThis:
        if (!emitSetThis(&pn->as<BinaryNode>())) {
            return false;
        }
        break;

      case ParseNodeKind::PropertyName:
      case ParseNodeKind::PosHolder:
        MOZ_FALLTHROUGH_ASSERT("Should never try to emit ParseNodeKind::PosHolder or ::Property");

      default:
        MOZ_ASSERT(0);
    }

    /* bce->emitLevel == 1 means we're last on the stack, so finish up. */
    if (emitLevel == 1) {
        if (!updateSourceCoordNotes(pn->pn_pos.end)) {
            return false;
        }
    }
    return true;
}

static bool
AllocSrcNote(JSContext* cx, SrcNotesVector& notes, unsigned* index)
{
    if (!notes.growBy(1)) {
        ReportOutOfMemory(cx);
        return false;
    }

    *index = notes.length() - 1;
    return true;
}

bool
BytecodeEmitter::addTryNote(JSTryNoteKind kind, uint32_t stackDepth, size_t start, size_t end)
{
    // The tryNoteList stores offsets relative to current section should must
    // be main section. During tryNoteList.finish(), the prologueLength will be
    // added to correct offset.
    MOZ_ASSERT(!inPrologue());
    return tryNoteList.append(kind, stackDepth, start, end);
}

bool
BytecodeEmitter::newSrcNote(SrcNoteType type, unsigned* indexp)
{
    SrcNotesVector& notes = this->notes();
    unsigned index;
    if (!AllocSrcNote(cx, notes, &index)) {
        return false;
    }

    /*
     * Compute delta from the last annotated bytecode's offset.  If it's too
     * big to fit in sn, allocate one or more xdelta notes and reset sn.
     */
    ptrdiff_t offset = this->offset();
    ptrdiff_t delta = offset - lastNoteOffset();
    current->lastNoteOffset = offset;
    if (delta >= SN_DELTA_LIMIT) {
        do {
            ptrdiff_t xdelta = Min(delta, SN_XDELTA_MASK);
            SN_MAKE_XDELTA(&notes[index], xdelta);
            delta -= xdelta;
            if (!AllocSrcNote(cx, notes, &index)) {
                return false;
            }
        } while (delta >= SN_DELTA_LIMIT);
    }

    /*
     * Initialize type and delta, then allocate the minimum number of notes
     * needed for type's arity.  Usually, we won't need more, but if an offset
     * does take two bytes, setSrcNoteOffset will grow notes.
     */
    SN_MAKE_NOTE(&notes[index], type, delta);
    for (int n = (int)js_SrcNoteSpec[type].arity; n > 0; n--) {
        if (!newSrcNote(SRC_NULL)) {
            return false;
        }
    }

    if (indexp) {
        *indexp = index;
    }
    return true;
}

bool
BytecodeEmitter::newSrcNote2(SrcNoteType type, ptrdiff_t offset, unsigned* indexp)
{
    unsigned index;
    if (!newSrcNote(type, &index)) {
        return false;
    }
    if (!setSrcNoteOffset(index, 0, offset)) {
        return false;
    }
    if (indexp) {
        *indexp = index;
    }
    return true;
}

bool
BytecodeEmitter::newSrcNote3(SrcNoteType type, ptrdiff_t offset1, ptrdiff_t offset2,
                             unsigned* indexp)
{
    unsigned index;
    if (!newSrcNote(type, &index)) {
        return false;
    }
    if (!setSrcNoteOffset(index, 0, offset1)) {
        return false;
    }
    if (!setSrcNoteOffset(index, 1, offset2)) {
        return false;
    }
    if (indexp) {
        *indexp = index;
    }
    return true;
}

bool
BytecodeEmitter::addToSrcNoteDelta(jssrcnote* sn, ptrdiff_t delta)
{
    /*
     * Called only from finishTakingSrcNotes to add to main script note
     * deltas, and only by a small positive amount.
     */
    MOZ_ASSERT(current == &main);
    MOZ_ASSERT((unsigned) delta < (unsigned) SN_XDELTA_LIMIT);

    ptrdiff_t base = SN_DELTA(sn);
    ptrdiff_t limit = SN_IS_XDELTA(sn) ? SN_XDELTA_LIMIT : SN_DELTA_LIMIT;
    ptrdiff_t newdelta = base + delta;
    if (newdelta < limit) {
        SN_SET_DELTA(sn, newdelta);
    } else {
        jssrcnote xdelta;
        SN_MAKE_XDELTA(&xdelta, delta);
        if (!main.notes.insert(sn, xdelta)) {
            return false;
        }
    }
    return true;
}

bool
BytecodeEmitter::setSrcNoteOffset(unsigned index, unsigned which, ptrdiff_t offset)
{
    if (!SN_REPRESENTABLE_OFFSET(offset)) {
        reportError(nullptr, JSMSG_NEED_DIET, js_script_str);
        return false;
    }

    SrcNotesVector& notes = this->notes();

    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    jssrcnote* sn = &notes[index];
    MOZ_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    MOZ_ASSERT((int) which < js_SrcNoteSpec[SN_TYPE(sn)].arity);
    for (sn++; which; sn++, which--) {
        if (*sn & SN_4BYTE_OFFSET_FLAG) {
            sn += 3;
        }
    }

    /*
     * See if the new offset requires four bytes either by being too big or if
     * the offset has already been inflated (in which case, we need to stay big
     * to not break the srcnote encoding if this isn't the last srcnote).
     */
    if (offset > (ptrdiff_t)SN_4BYTE_OFFSET_MASK || (*sn & SN_4BYTE_OFFSET_FLAG)) {
        /* Maybe this offset was already set to a four-byte value. */
        if (!(*sn & SN_4BYTE_OFFSET_FLAG)) {
            /* Insert three dummy bytes that will be overwritten shortly. */
            jssrcnote dummy = 0;
            if (!(sn = notes.insert(sn, dummy)) ||
                !(sn = notes.insert(sn, dummy)) ||
                !(sn = notes.insert(sn, dummy)))
            {
                ReportOutOfMemory(cx);
                return false;
            }
        }
        *sn++ = (jssrcnote)(SN_4BYTE_OFFSET_FLAG | (offset >> 24));
        *sn++ = (jssrcnote)(offset >> 16);
        *sn++ = (jssrcnote)(offset >> 8);
    }
    *sn = (jssrcnote)offset;
    return true;
}

bool
BytecodeEmitter::finishTakingSrcNotes(uint32_t* out)
{
    MOZ_ASSERT(current == &main);

    MOZ_ASSERT(prologue.notes.length() == 0);
    MOZ_ASSERT(prologue.lastNoteOffset == 0);

    // We may need to adjust the offset of the first main note, by adding to
    // its delta and possibly even prepending SRC_XDELTA notes to it to account
    // for prologue bytecodes.
    ptrdiff_t offset = prologueOffset();
    MOZ_ASSERT(offset >= 0);
    if (offset > 0 && main.notes.length() != 0) {
        // NB: Use as much of the first main note's delta as we can.
        jssrcnote* sn = main.notes.begin();
        ptrdiff_t delta = SN_IS_XDELTA(sn)
                          ? SN_XDELTA_MASK - (*sn & SN_XDELTA_MASK)
                          : SN_DELTA_MASK - (*sn & SN_DELTA_MASK);
        if (offset < delta) {
            delta = offset;
        }
        for (;;) {
            if (!addToSrcNoteDelta(sn, delta)) {
                return false;
            }
            offset -= delta;
            if (offset == 0) {
                break;
            }
            delta = Min(offset, SN_XDELTA_MASK);
            sn = main.notes.begin();
        }
    }

    // The + 1 is to account for the final SN_MAKE_TERMINATOR that is appended
    // when the notes are copied to their final destination by copySrcNotes.
    *out = main.notes.length() + 1;
    return true;
}

void
BytecodeEmitter::copySrcNotes(jssrcnote* destination, uint32_t nsrcnotes)
{
    MOZ_ASSERT(prologue.notes.length() == 0);
    unsigned mainCount = main.notes.length();
    // nsrcnotes includes SN_MAKE_TERMINATOR in addition to main.notes.
    MOZ_ASSERT(mainCount == nsrcnotes - 1);
    PodCopy(destination, main.notes.begin(), mainCount);
    SN_MAKE_TERMINATOR(&destination[mainCount]);
}

void
CGNumberList::finish(mozilla::Span<GCPtrValue> array)
{
    MOZ_ASSERT(length() == array.size());

    for (unsigned i = 0; i < length(); i++) {
        array[i].init(DoubleValue(list[i]));
    }
}

/*
 * Find the index of the given object for code generator.
 *
 * Since the emitter refers to each parsed object only once, for the index we
 * use the number of already indexed objects. We also add the object to a list
 * to convert the list to a fixed-size array when we complete code generation,
 * see js::CGObjectList::finish below.
 */
unsigned
CGObjectList::add(ObjectBox* objbox)
{
    MOZ_ASSERT(!objbox->emitLink);
    objbox->emitLink = lastbox;
    lastbox = objbox;
    return length++;
}

void
CGObjectList::finish(mozilla::Span<GCPtrObject> array)
{
    MOZ_ASSERT(length <= INDEX_LIMIT);
    MOZ_ASSERT(length == array.size());

    ObjectBox* objbox = lastbox;
    for (GCPtrObject& obj : mozilla::Reversed(array)) {
        MOZ_ASSERT(obj == nullptr);
        MOZ_ASSERT(objbox->object->isTenured());
        if (objbox->isFunctionBox()) {
            objbox->asFunctionBox()->finish();
        }
        obj.init(objbox->object);
        objbox = objbox->emitLink;
    }
}

void
CGScopeList::finish(mozilla::Span<GCPtrScope> array)
{
    MOZ_ASSERT(length() <= INDEX_LIMIT);
    MOZ_ASSERT(length() == array.size());

    for (uint32_t i = 0; i < length(); i++) {
        array[i].init(vector[i]);
    }
}

bool
CGTryNoteList::append(JSTryNoteKind kind, uint32_t stackDepth, size_t start, size_t end)
{
    MOZ_ASSERT(start <= end);
    MOZ_ASSERT(size_t(uint32_t(start)) == start);
    MOZ_ASSERT(size_t(uint32_t(end)) == end);

    // Offsets are given relative to sections, but we only expect main-section
    // to have TryNotes. In finish() we will fixup base offset.

    JSTryNote note;
    note.kind = kind;
    note.stackDepth = stackDepth;
    note.start = uint32_t(start);
    note.length = uint32_t(end - start);

    return list.append(note);
}

void
CGTryNoteList::finish(mozilla::Span<JSTryNote> array, uint32_t prologueLength)
{
    MOZ_ASSERT(length() == array.size());

    for (unsigned i = 0; i < length(); i++) {
        list[i].start += prologueLength;
        array[i] = list[i];
    }
}

bool
CGScopeNoteList::append(uint32_t scopeIndex, uint32_t offset, bool inPrologue,
                        uint32_t parent)
{
    CGScopeNote note;
    mozilla::PodZero(&note);

    // Offsets are given relative to sections. In finish() we will fixup base
    // offset if needed.

    note.index = scopeIndex;
    note.start = offset;
    note.parent = parent;
    note.startInPrologue = inPrologue;

    return list.append(note);
}

void
CGScopeNoteList::recordEnd(uint32_t index, uint32_t offset, bool inPrologue)
{
    MOZ_ASSERT(index < length());
    MOZ_ASSERT(list[index].length == 0);
    list[index].end = offset;
    list[index].endInPrologue = inPrologue;
}

void
CGScopeNoteList::finish(mozilla::Span<ScopeNote> array, uint32_t prologueLength)
{
    MOZ_ASSERT(length() == array.size());

    for (unsigned i = 0; i < length(); i++) {
        if (!list[i].startInPrologue) {
            list[i].start += prologueLength;
        }
        if (!list[i].endInPrologue && list[i].end != UINT32_MAX) {
            list[i].end += prologueLength;
        }
        MOZ_ASSERT(list[i].end >= list[i].start);
        list[i].length = list[i].end - list[i].start;
        array[i] = list[i];
    }
}

void
CGResumeOffsetList::finish(mozilla::Span<uint32_t> array, uint32_t prologueLength)
{
    MOZ_ASSERT(length() == array.size());

    for (unsigned i = 0; i < length(); i++) {
        array[i] = prologueLength + list[i];
    }
}

const JSSrcNoteSpec js_SrcNoteSpec[] = {
#define DEFINE_SRC_NOTE_SPEC(sym, name, arity) { name, arity },
    FOR_EACH_SRC_NOTE_TYPE(DEFINE_SRC_NOTE_SPEC)
#undef DEFINE_SRC_NOTE_SPEC
};

static int
SrcNoteArity(jssrcnote* sn)
{
    MOZ_ASSERT(SN_TYPE(sn) < SRC_LAST);
    return js_SrcNoteSpec[SN_TYPE(sn)].arity;
}

JS_FRIEND_API(unsigned)
js::SrcNoteLength(jssrcnote* sn)
{
    unsigned arity;
    jssrcnote* base;

    arity = SrcNoteArity(sn);
    for (base = sn++; arity; sn++, arity--) {
        if (*sn & SN_4BYTE_OFFSET_FLAG) {
            sn += 3;
        }
    }
    return sn - base;
}

JS_FRIEND_API(ptrdiff_t)
js::GetSrcNoteOffset(jssrcnote* sn, unsigned which)
{
    /* Find the offset numbered which (i.e., skip exactly which offsets). */
    MOZ_ASSERT(SN_TYPE(sn) != SRC_XDELTA);
    MOZ_ASSERT((int) which < SrcNoteArity(sn));
    for (sn++; which; sn++, which--) {
        if (*sn & SN_4BYTE_OFFSET_FLAG) {
            sn += 3;
        }
    }
    if (*sn & SN_4BYTE_OFFSET_FLAG) {
        return (ptrdiff_t)(((uint32_t)(sn[0] & SN_4BYTE_OFFSET_MASK) << 24)
                           | (sn[1] << 16)
                           | (sn[2] << 8)
                           | sn[3]);
    }
    return (ptrdiff_t)*sn;
}
