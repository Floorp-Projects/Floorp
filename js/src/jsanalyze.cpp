/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla SpiderMonkey bytecode analysis
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Hackett <bhackett@mozilla.com>
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

#include "jsanalyze.h"
#include "jsautooplen.h"
#include "jscompartment.h"
#include "jscntxt.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

/*
 * These are the definitions needed for skeleton TypeCompartments and TypeObjects
 * as used when type inference is disabled.
 */

namespace js {
namespace types {

void
TypeCompartment::init()
{
    PodZero(this);
    JS_InitArenaPool(&pool, "typeinfer", 512, 8, NULL);

#ifdef DEBUG
    emptyObject.name_ = JSID_VOID;
#endif
    emptyObject.unknownProperties = true;
    emptyObject.pool = &pool;
}

TypeCompartment::~TypeCompartment()
{
    JS_FinishArenaPool(&pool);
}

types::TypeObject *
TypeCompartment::newTypeObject(JSContext *cx, analyze::Script *script, const char *name,
                               bool isFunction, JSObject *proto)
{
    JSArenaPool &pool = script ? script->pool : this->pool;

#ifdef DEBUG
#if 1 /* Define to get unique printed names, including when there are multiple globals. */
    static unsigned nameCount = 0;
    unsigned len = strlen(name) + 15;
    char *newName = (char *) alloca(len);
    JS_snprintf(newName, len, "%u:%s", ++nameCount, name);
    name = newName;
#endif
    jsid id = ATOM_TO_JSID(js_Atomize(cx, name, strlen(name), ATOM_PINNED));
#else
    jsid id = JSID_VOID;
#endif

    TypeObject *object;
    if (isFunction)
        object = ArenaNew<TypeFunction>(pool, &pool, id, proto);
    else
        object = ArenaNew<TypeObject>(pool, &pool, id, proto);

#ifdef JS_TYPE_INFERENCE
    TypeObject *&objects = script ? script->objects : this->objects;
    object->next = objects;
    objects = object;
#else
    object->next = this->objects;
    this->objects = object;
#endif

    return object;
}

const char *
TypeIdStringImpl(jsid id)
{
    if (JSID_IS_VOID(id))
        return "(index)";
    if (JSID_IS_EMPTY(id))
        return "(new)";
    static char bufs[4][100];
    static unsigned which = 0;
    which = (which + 1) & 3;
    PutEscapedString(bufs[which], 100, JSID_TO_FLAT_STRING(id), 0);
    return bufs[which];
}

void
TypeObject::splicePrototype(JSObject *proto)
{
    JS_ASSERT(!this->proto);
    this->proto = proto;
    this->instanceNext = proto->getType()->instanceList;
    proto->getType()->instanceList = this;
}

} } /* namespace js::types */

js::types::TypeFunction *
JSContext::newTypeFunction(const char *name, JSObject *proto)
{
    return (js::types::TypeFunction *) compartment->types.newTypeObject(this, NULL, name, true, proto);
}

js::types::TypeObject *
JSContext::newTypeObject(const char *name, JSObject *proto)
{
    return compartment->types.newTypeObject(this, NULL, name, false, proto);
}

js::types::TypeObject *
JSContext::newTypeObject(const char *base, const char *postfix, JSObject *proto)
{
    char *name = NULL;
#ifdef DEBUG
    unsigned len = strlen(base) + strlen(postfix) + 5;
    name = (char *)alloca(len);
    JS_snprintf(name, len, "%s:%s", base, postfix);
#endif
    return compartment->types.newTypeObject(this, NULL, name, false, proto);
}

void
JSObject::makeNewType(JSContext *cx)
{
    JS_ASSERT(!newType);
    setDelegate();
    newType = cx->newTypeObject(getType()->name(), "new", this);

#ifdef JS_TYPE_INFERENCE
    if (!getType()->unknownProperties) {
        /* Update the possible 'new' types for all prototype objects sharing the same type object. */
        js::types::TypeSet *types = getType()->getProperty(cx, JSID_EMPTY, true);
        cx->compartment->types.addDynamicType(cx, types, (js::types::jstype) newType);
    }
#endif
}

/////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////

namespace js {

void
types::TypeObject::trace(JSTracer *trc)
{
    JS_ASSERT(!marked);

    /*
     * Only mark types if the Mark/Sweep GC is running; the bit won't be cleared
     * by the cycle collector.
     */
    if (trc->context->runtime->gcMarkAndSweep)
        marked = true;

    if (emptyShapes) {
        int count = gc::FINALIZE_OBJECT_LAST - gc::FINALIZE_OBJECT0 + 1;
        for (int i = 0; i < count; i++) {
            if (emptyShapes[i])
                emptyShapes[i]->trace(trc);
        }
    }

    if (proto)
        gc::MarkObject(trc, *proto, "type_proto");
}

void
analyze::Script::trace(JSTracer *trc)
{
#ifdef JS_TYPE_INFERENCE
    /* If a script is live, so are all type objects within it. */
    types::TypeObject *object = objects;
    while (object) {
        if (!object->marked)
            object->trace(trc);
        object = object->next;
    }
#endif
}

static inline void
SweepObjectList(JSContext *cx, types::TypeObject *objects)
{
    types::TypeObject *object = objects;
    while (object) {
        if (object->marked) {
            object->marked = false;
        } else {
            object->proto = NULL;
            if (object->emptyShapes) {
                cx->free(object->emptyShapes);
                object->emptyShapes = NULL;
            }
        }
        object = object->next;
    }
}

void
analyze::Script::sweep(JSContext *cx)
{
#ifdef JS_TYPE_INFERENCE
    SweepObjectList(cx, objects);
#endif
}

void
analyze::Script::detach()
{
    /* :FIXME: bug 613221 should unlink incoming type constraints and destroy the analysis. */
    script = NULL;
#ifdef JS_TYPE_INFERENCE
    fun = NULL;
    localNames = NULL;
#endif
}

void
types::TypeCompartment::sweep(JSContext *cx)
{
    SweepObjectList(cx, objects);
}

} /* namespace js */

namespace js {
namespace analyze {

/////////////////////////////////////////////////////////////////////
// Script
/////////////////////////////////////////////////////////////////////

void
Script::destroy()
{
    JS_FinishArenaPool(&pool);
}

/////////////////////////////////////////////////////////////////////
// Bytecode
/////////////////////////////////////////////////////////////////////

bool
Bytecode::mergeDefines(JSContext *cx, Script *script, bool initial,
                       unsigned newDepth, types::TypeStack *newStack,
                       uint32 *newArray, unsigned newCount)
{
    if (initial) {
        /*
         * Haven't handled any incoming edges to this bytecode before.
         * Define arrays are copy on write, so just reuse the array for this bytecode.
         */
        stackDepth = newDepth;
        defineArray = newArray;
        defineCount = newCount;
#ifdef JS_TYPE_INFERENCE
        inStack = newStack;
#endif
        return true;
    }

#ifdef JS_TYPE_INFERENCE
    types::TypeStack::merge(cx, newStack, inStack);
#endif

    /*
     * This bytecode has multiple incoming edges, intersect the new array with any
     * variables known to be defined along other incoming edges.
     */
    if (analyzed) {
#ifdef DEBUG
        /*
         * Once analyzed, a bytecode has its full set of definitions.  There are two
         * properties we depend on to ensure this.  First, bytecode for a function
         * is emitted in topological order, and since we analyze bytecodes in the
         * order they were emitted we will have seen all incoming jumps except
         * for any loop back edges.  Second, javascript has structured control flow,
         * so loop heads dominate their bodies; the set of variables defined
         * on a back edge will be at least as large as at the head of the loop,
         * so no intersection or further analysis needs to be done.
         */
        JS_ASSERT(stackDepth == newDepth);
        for (unsigned i = 0; i < defineCount; i++) {
            bool found = false;
            for (unsigned j = 0; j < newCount; j++) {
                if (newArray[j] == defineArray[i])
                    found = true;
            }
            JS_ASSERT(found);
        }
#endif
    } else {
        JS_ASSERT(stackDepth == newDepth);
        bool owned = false;
        for (unsigned i = 0; i < defineCount; i++) {
            bool found = false;
            for (unsigned j = 0; j < newCount; j++) {
                if (newArray[j] == defineArray[i])
                    found = true;
            }
            if (!found) {
                /*
                 * Get a mutable copy of the defines.  This can end up making
                 * several copies for a bytecode if it has many incoming edges
                 * with progressively smaller sets of defined variables.
                 */
                if (!owned) {
                    uint32 *reallocArray = ArenaArray<uint32>(script->pool, defineCount);
                    if (!reallocArray) {
                        script->setOOM(cx);
                        return false;
                    }
                    memcpy(reallocArray, defineArray, defineCount * sizeof(uint32));
                    defineArray = reallocArray;
                    owned = true;
                }

                /* Swap with the last element and pop the array. */
                defineArray[i--] = defineArray[--defineCount];
            }
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////////
// Analysis
/////////////////////////////////////////////////////////////////////

inline bool
Script::addJump(JSContext *cx, unsigned offset,
                unsigned *currentOffset, unsigned *forwardJump,
                unsigned stackDepth, types::TypeStack *stack,
                uint32 *defineArray, unsigned defineCount)
{
    JS_ASSERT(offset < script->length);

    Bytecode *&code = codeArray[offset];
    bool initial = (code == NULL);
    if (initial) {
        code = ArenaNew<Bytecode>(pool, this, offset);
        if (!code) {
            setOOM(cx);
            return false;
        }
    }

    if (!code->mergeDefines(cx, this, initial, stackDepth, stack, defineArray, defineCount))
        return false;
    code->jumpTarget = true;

    if (offset < *currentOffset) {
        /* Don't follow back edges to bytecode which has already been analyzed. */
        if (!code->analyzed) {
            if (*forwardJump == 0)
                *forwardJump = *currentOffset;
            *currentOffset = offset;
        }
    } else if (offset > *forwardJump) {
        *forwardJump = offset;
    }

    return true;
}

inline void
Script::setLocal(uint32 local, uint32 offset)
{
    JS_ASSERT(local < localCount());
    JS_ASSERT(offset != LOCAL_CONDITIONALLY_DEFINED);

    /*
     * It isn't possible to change the point when a variable becomes unconditionally
     * defined, or to mark it as unconditionally defined after it has already been
     * marked as having a use before def.  It *is* possible to mark it as having
     * a use before def after marking it as unconditionally defined.  In a loop such as:
     *
     * while ((a = b) != 0) { x = a; }
     *
     * When walking through the body of this loop, we will first analyze the test
     * (which comes after the body in the bytecode stream) as unconditional code,
     * and mark a as definitely defined.  a is not in the define array when taking
     * the loop's back edge, so it is treated as possibly undefined when written to x.
     */
    JS_ASSERT(locals[local] == LOCAL_CONDITIONALLY_DEFINED ||
              locals[local] == offset || offset == LOCAL_USE_BEFORE_DEF);

    locals[local] = offset;
}

static inline ptrdiff_t
GetJumpOffset(jsbytecode *pc, jsbytecode *pc2)
{
    uint32 type = JOF_OPTYPE(*pc);
    if (JOF_TYPE_IS_EXTENDED_JUMP(type))
        return GET_JUMPX_OFFSET(pc2);
    return GET_JUMP_OFFSET(pc2);
}

// return whether op bytecodes do not fallthrough (they may do a jump).
static inline bool
BytecodeNoFallThrough(JSOp op)
{
    switch (op) {
      case JSOP_GOTO:
      case JSOP_GOTOX:
      case JSOP_DEFAULT:
      case JSOP_DEFAULTX:
      case JSOP_RETURN:
      case JSOP_STOP:
      case JSOP_RETRVAL:
      case JSOP_THROW:
      case JSOP_TABLESWITCH:
      case JSOP_TABLESWITCHX:
      case JSOP_LOOKUPSWITCH:
      case JSOP_LOOKUPSWITCHX:
      case JSOP_FILTER:
        return true;
      case JSOP_GOSUB:
      case JSOP_GOSUBX:
        // these fall through indirectly, after executing a 'finally'.
        return false;
      default:
        return false;
    }
}

#ifdef JS_TYPE_INFERENCE

/*
 * Information about a currently active static initializer.  We keep the stack
 * of initializers around during analysis so we can reuse objects when
 * processing arrays of arrays or arrays of objects.
 */
struct InitializerInfo
{
    /* Object being initialized. */
    types::TypeObject *object;

    /* Whether this object is an array. */
    bool isArray;

    /* Any object to use for initializers appearing in the array's elements. */
    types::TypeObject *initObject;

    /* Whether initObject is an array vs. object. */
    bool initArray;

    /* Outer initializer, the one this initializer is nested in. */
    InitializerInfo *outer;

    InitializerInfo() { PodZero(this); }
};

#endif /* JS_TYPE_INFERENCE */

void
Script::init(JSScript *script)
{
    this->script = script;
    JS_InitArenaPool(&pool, "script_analyze", 256, 8, NULL);
}

void
Script::analyze(JSContext *cx)
{
    JS_ASSERT(script && !codeArray && !locals);

    unsigned length = script->length;
    unsigned nfixed = localCount();

    codeArray = ArenaArray<Bytecode*>(pool, length);
    locals = ArenaArray<uint32>(pool, nfixed);

    if (!codeArray || !locals) {
        setOOM(cx);
        return;
    }

    PodZero(codeArray, length);

    for (unsigned i = 0; i < nfixed; i++)
        locals[i] = LOCAL_CONDITIONALLY_DEFINED;

    /*
     * Treat locals as having a possible use-before-def if they could be accessed
     * by debug code or by eval, or if they could be accessed by an inner script.
     */

    if (script->usesEval || cx->compartment->debugMode) {
        for (uint32 i = 0; i < nfixed; i++)
            setLocal(i, LOCAL_USE_BEFORE_DEF);
    }

    for (uint32 i = 0; i < script->nClosedVars; i++) {
        uint32 slot = script->getClosedVar(i);
        if (slot < nfixed)
            setLocal(slot, LOCAL_USE_BEFORE_DEF);
    }

    /*
     * If the script is in debug mode, JS_SetFrameReturnValue can be called at
     * any safe point.
     */
    if (cx->compartment->debugMode)
        usesRval = true;

    /*
     * If we are in the middle of one or more jumps, the offset of the highest
     * target jumping over this bytecode.  Includes implicit jumps from
     * try/catch/finally blocks.
     */
    unsigned forwardJump = 0;

    /*
     * If we are in the middle of a try block, the offset of the highest
     * catch/finally/enditer.
     */
    unsigned forwardCatch = 0;

#ifdef JS_TYPE_INFERENCE
    /* Stack of active initializers. */
    InitializerInfo *initializerStack = NULL;
#endif

    /* Fill in stack depth and definitions at initial bytecode. */
    Bytecode *startcode = ArenaNew<Bytecode>(pool, this, 0);
    if (!startcode) {
        setOOM(cx);
        return;
    }

    startcode->stackDepth = 0;
    codeArray[0] = startcode;

    unsigned offset, nextOffset = 0;
    while (nextOffset < length) {
        offset = nextOffset;

        JS_ASSERT(forwardCatch <= forwardJump);

        /* Check if the current forward jump/try-block has finished. */
        if (forwardJump && forwardJump == offset)
            forwardJump = 0;
        if (forwardCatch && forwardCatch == offset)
            forwardCatch = 0;

        Bytecode *code = maybeCode(offset);
        jsbytecode *pc = script->code + offset;

        UntrapOpcode untrap(cx, script, pc);

        JSOp op = (JSOp)*pc;
        JS_ASSERT(op < JSOP_LIMIT);

        /* Immediate successor of this bytecode. */
        unsigned successorOffset = offset + GetBytecodeLength(pc);

        /*
         * Next bytecode to analyze.  This is either the successor, or is an
         * earlier bytecode if this bytecode has a loop backedge.
         */
        nextOffset = successorOffset;

        if (!code) {
            /* Haven't found a path by which this bytecode is reachable. */
            continue;
        }

        if (code->analyzed) {
            /* No need to reanalyze, see Bytecode::mergeDefines. */
            continue;
        }

        code->analyzed = true;

        if (forwardCatch)
            code->inTryBlock = true;

        if (untrap.trap)
            code->safePoint = true;

        unsigned stackDepth = code->stackDepth;
        uint32 *defineArray = code->defineArray;
        unsigned defineCount = code->defineCount;

        if (!forwardJump) {
            /*
             * There is no jump over this bytecode, nor a containing try block.
             * Either this bytecode will definitely be executed, or an exception
             * will be thrown which the script does not catch.  Either way,
             * any variables definitely defined at this bytecode will stay
             * defined throughout the rest of the script.  We just need to
             * remember the offset where the variable became unconditionally
             * defined, rather than continue to maintain it in define arrays.
             */
            for (unsigned i = 0; i < defineCount; i++) {
                uint32 local = defineArray[i];
                JS_ASSERT_IF(locals[local] != LOCAL_CONDITIONALLY_DEFINED &&
                             locals[local] != LOCAL_USE_BEFORE_DEF,
                             locals[local] <= offset);
                if (locals[local] == LOCAL_CONDITIONALLY_DEFINED)
                    setLocal(local, offset);
            }
            defineArray = NULL;
            defineCount = 0;
        }

        unsigned nuses = GetUseCount(script, offset);
        unsigned ndefs = GetDefCount(script, offset);

        JS_ASSERT(stackDepth >= nuses);
        stackDepth -= nuses;
        stackDepth += ndefs;

        types::TypeStack *stack = NULL;

#ifdef JS_TYPE_INFERENCE

        stack = code->inStack;

        for (unsigned i = 0; i < nuses; i++)
            stack = stack->group()->innerStack;

        code->pushedArray = ArenaArray<types::TypeStack>(pool, ndefs);
        PodZero(code->pushedArray, ndefs);

        for (unsigned i = 0; i < ndefs; i++) {
            code->pushedArray[i].types.setPool(&pool);
            code->pushedArray[i].setInnerStack(stack);
            stack = &code->pushedArray[i];

            types::InferSpew(types::ISpewOps, "pushed #%u:%05u %u T%u",
                             id, offset, i, stack->types.id());
        }

        /* Track the initializer stack and compute new objects for encountered initializers. */
        if (op == JSOP_NEWINIT || op == JSOP_NEWARRAY || op == JSOP_NEWOBJECT) {
            bool newArray = (op == JSOP_NEWARRAY) || (op == JSOP_NEWINIT && pc[1] == JSProto_Array);

            types::TypeObject *object;
            if (!script->compileAndGo) {
                object = NULL;
            } else if (initializerStack && initializerStack->initObject &&
                       initializerStack->initArray == newArray) {
                object = initializerStack->initObject;
                if (newArray)
                    code->initArray = object;
                else
                    code->initObject = object;
            } else {
                object = code->getInitObject(cx, newArray);

                if (initializerStack && initializerStack->isArray) {
                    initializerStack->initObject = object;
                    initializerStack->initArray = newArray;
                }
            }

            InitializerInfo *info = (InitializerInfo *) cx->calloc(sizeof(InitializerInfo));
            info->outer = initializerStack;
            info->object = object;
            info->isArray = newArray;
            initializerStack = info;
        } else if (op == JSOP_INITELEM || op == JSOP_INITPROP || op == JSOP_INITMETHOD) {
            JS_ASSERT(initializerStack);
            code->initObject = initializerStack->object;
        } else if (op == JSOP_ENDINIT) {
            JS_ASSERT(initializerStack);
            InitializerInfo *info = initializerStack;
            initializerStack = initializerStack->outer;
            cx->free(info);
        }

#endif /* JS_TYPE_INFERENCE */

        switch (op) {

          case JSOP_SETRVAL:
          case JSOP_POPV:
            usesRval = true;
            break;

          case JSOP_NAME:
          case JSOP_CALLNAME:
          case JSOP_BINDNAME:
          case JSOP_SETNAME:
          case JSOP_DELNAME:
          case JSOP_INCNAME:
          case JSOP_DECNAME:
          case JSOP_NAMEINC:
          case JSOP_NAMEDEC:
          case JSOP_FORNAME:
            usesScope = true;
            break;

          case JSOP_TABLESWITCH:
          case JSOP_TABLESWITCHX: {
            jsbytecode *pc2 = pc;
            unsigned jmplen = (op == JSOP_TABLESWITCH) ? JUMP_OFFSET_LEN : JUMPX_OFFSET_LEN;
            unsigned defaultOffset = offset + GetJumpOffset(pc, pc2);
            pc2 += jmplen;
            jsint low = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;
            jsint high = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;

            if (!addJump(cx, defaultOffset, &nextOffset, &forwardJump,
                         stackDepth, stack, defineArray, defineCount)) {
                return;
            }
            getCode(defaultOffset).switchTarget = true;
            getCode(defaultOffset).safePoint = true;

            for (jsint i = low; i <= high; i++) {
                unsigned targetOffset = offset + GetJumpOffset(pc, pc2);
                if (targetOffset != offset) {
                    if (!addJump(cx, targetOffset, &nextOffset, &forwardJump,
                                 stackDepth, stack, defineArray, defineCount)) {
                        return;
                    }
                }
                getCode(targetOffset).switchTarget = true;
                getCode(targetOffset).safePoint = true;
                pc2 += jmplen;
            }
            break;
          }

          case JSOP_LOOKUPSWITCH:
          case JSOP_LOOKUPSWITCHX: {
            jsbytecode *pc2 = pc;
            unsigned jmplen = (op == JSOP_LOOKUPSWITCH) ? JUMP_OFFSET_LEN : JUMPX_OFFSET_LEN;
            unsigned defaultOffset = offset + GetJumpOffset(pc, pc2);
            pc2 += jmplen;
            unsigned npairs = GET_UINT16(pc2);
            pc2 += UINT16_LEN;

            if (!addJump(cx, defaultOffset, &nextOffset, &forwardJump,
                         stackDepth, stack, defineArray, defineCount)) {
                return;
            }
            getCode(defaultOffset).switchTarget = true;
            getCode(defaultOffset).safePoint = true;

            while (npairs) {
                pc2 += INDEX_LEN;
                unsigned targetOffset = offset + GetJumpOffset(pc, pc2);
                if (!addJump(cx, targetOffset, &nextOffset, &forwardJump,
                             stackDepth, stack, defineArray, defineCount)) {
                    return;
                }
                getCode(targetOffset).switchTarget = true;
                getCode(targetOffset).safePoint = true;
                pc2 += jmplen;
                npairs--;
            }
            break;
          }

          case JSOP_TRY: {
            /*
             * Everything between a try and corresponding catch or finally is conditional.
             * Note that there is no problem with code which is skipped by a thrown
             * exception but is not caught by a later handler in the same function:
             * no more code will execute, and it does not matter what is defined.
             */
            JSTryNote *tn = script->trynotes()->vector;
            JSTryNote *tnlimit = tn + script->trynotes()->length;
            for (; tn < tnlimit; tn++) {
                unsigned startOffset = script->main - script->code + tn->start;
                if (startOffset == offset + 1) {
                    unsigned catchOffset = startOffset + tn->length;

                    /* This will overestimate try block code, for multiple catch/finally. */
                    if (catchOffset > forwardCatch)
                        forwardCatch = catchOffset;

                    if (tn->kind != JSTRY_ITER) {
                        if (!addJump(cx, catchOffset, &nextOffset, &forwardJump,
                                     stackDepth, stack, defineArray, defineCount)) {
                            return;
                        }
                        getCode(catchOffset).exceptionEntry = true;
                        getCode(catchOffset).safePoint = true;
                    }
                }
            }
            break;
          }

          case JSOP_GETLOCAL:
            /*
             * Watch for uses of variables not known to be defined, and mark
             * them as having possible uses before definitions.  Ignore GETLOCAL
             * followed by a POP, these are generated for, e.g. 'var x;'
             */
            if (pc[JSOP_GETLOCAL_LENGTH] != JSOP_POP) {
                uint32 local = GET_SLOTNO(pc);
                if (local < nfixed && !localDefined(local, offset))
                    setLocal(local, LOCAL_USE_BEFORE_DEF);
            }
            break;

          case JSOP_CALLLOCAL:
          case JSOP_INCLOCAL:
          case JSOP_DECLOCAL:
          case JSOP_LOCALINC:
          case JSOP_LOCALDEC: {
            uint32 local = GET_SLOTNO(pc);
            if (local < nfixed && !localDefined(local, offset))
                setLocal(local, LOCAL_USE_BEFORE_DEF);
            break;
          }

          case JSOP_SETLOCAL:
          case JSOP_FORLOCAL: {
            uint32 local = GET_SLOTNO(pc);

            /*
             * The local variable may already have been marked as unconditionally
             * defined at a later point in the script, if that definition was in the
             * condition for a loop which then jumped back here.  In such cases we
             * will not treat the variable as ever being defined in the loop body
             * (see setLocal).
             */
            if (local < nfixed && locals[local] == LOCAL_CONDITIONALLY_DEFINED) {
                if (forwardJump) {
                    /* Add this local to the variables defined after this bytecode. */
                    uint32 *newArray = ArenaArray<uint32>(pool, defineCount + 1);
                    if (!newArray) {
                        setOOM(cx);
                        return;
                    }
                    if (defineCount)
                        memcpy(newArray, defineArray, defineCount * sizeof(uint32));
                    defineArray = newArray;
                    defineArray[defineCount++] = local;
                } else {
                    /* This local is unconditionally defined by this bytecode. */
                    setLocal(local, offset);
                }
            }
            break;
          }

          default:
            break;
        }

        uint32 type = JOF_TYPE(js_CodeSpec[op].format);

        /* Check basic jump opcodes, which may or may not have a fallthrough. */
        if (type == JOF_JUMP || type == JOF_JUMPX) {
            /* Some opcodes behave differently on their branching path. */
            unsigned newStackDepth = stackDepth;
            types::TypeStack *newStack = stack;

            switch (op) {
              case JSOP_OR:
              case JSOP_AND:
              case JSOP_ORX:
              case JSOP_ANDX:
                /*
                 * OR/AND instructions push the operation result when branching.
                 * We accounted for this in GetDefCount, so subtract the pushed value
                 * for the fallthrough case.
                 */
                stackDepth--;
#ifdef JS_TYPE_INFERENCE
                stack = stack->group()->innerStack;
#endif
                break;

              case JSOP_CASE:
              case JSOP_CASEX:
                /* Case instructions do not push the lvalue back when branching. */
                newStackDepth--;
#ifdef JS_TYPE_INFERENCE
                newStack = newStack->group()->innerStack;
#endif
                break;

              default:;
            }

            unsigned targetOffset = offset + GetJumpOffset(pc, pc);
            if (!addJump(cx, targetOffset, &nextOffset, &forwardJump,
                         newStackDepth, newStack, defineArray, defineCount)) {
                return;
            }
        }

        /* Handle any fallthrough from this opcode. */
        if (!BytecodeNoFallThrough(op)) {
            JS_ASSERT(successorOffset < script->length);

            Bytecode *&nextcode = codeArray[successorOffset];
            bool initial = (nextcode == NULL);

            if (initial) {
                nextcode = ArenaNew<Bytecode>(pool, this, successorOffset);
                if (!nextcode) {
                    setOOM(cx);
                    return;
                }
            }

            if (type == JOF_JUMP || type == JOF_JUMPX)
                nextcode->jumpFallthrough = true;

            if (!nextcode->mergeDefines(cx, this, initial, stackDepth, stack,
                                        defineArray, defineCount)) {
                return;
            }

            /* Treat the fallthrough of a branch instruction as a jump target. */
            if (type == JOF_JUMP || type == JOF_JUMPX)
                nextcode->jumpTarget = true;
            else
                nextcode->fallthrough = true;
        }
    }

    JS_ASSERT(!failed());
    JS_ASSERT(forwardJump == 0 && forwardCatch == 0);

#ifdef JS_TYPE_INFERENCE
    /* Generate type constraints for the script. */

    AnalyzeState state;
    state.init(cx, script);

    offset = 0;
    while (offset < script->length) {
        Bytecode *code = maybeCode(offset);

        jsbytecode *pc = script->code + offset;
        UntrapOpcode untrap(cx, script, pc);

        offset += GetBytecodeLength(pc);

        if (code && code->analyzed)
            analyzeTypes(cx, code, state);
    }

    state.destroy(cx);

#endif /* JS_TYPE_INFERENCE */
}

/////////////////////////////////////////////////////////////////////
// Live Range Analysis
/////////////////////////////////////////////////////////////////////

LifetimeScript::LifetimeScript()
    : analysis(NULL), script(NULL), fun(NULL), codeArray(NULL)
{
    JS_InitArenaPool(&pool, "script_liverange", 256, 8, NULL);
}

LifetimeScript::~LifetimeScript()
{
    JS_FinishArenaPool(&pool);
}

bool
LifetimeScript::analyze(JSContext *cx, analyze::Script *analysis, JSScript *script, JSFunction *fun)
{
    JS_ASSERT(analysis->hasAnalyzed() && !analysis->failed());

    this->analysis = analysis;
    this->script = script;
    this->fun = fun;

    codeArray = ArenaArray<LifetimeBytecode>(pool, script->length);
    if (!codeArray)
        return false;
    PodZero(codeArray, script->length);

    if (script->nfixed) {
        locals = ArenaArray<LifetimeVariable>(pool, script->nfixed);
        if (!locals)
            return false;
        PodZero(locals, script->nfixed);
    } else {
        locals = NULL;
    }

    if (fun && fun->nargs) {
        args = ArenaArray<LifetimeVariable>(pool, fun->nargs);
        if (!args)
            return false;
        PodZero(args, fun->nargs);
    } else {
        args = NULL;
    }

    PodZero(&thisVar);

    saved = ArenaArray<LifetimeVariable*>(pool, script->nfixed + (fun ? fun->nargs : 0));
    savedCount = 0;

    uint32 offset = script->length - 1;
    while (offset < script->length) {
        Bytecode *code = analysis->maybeCode(offset);
        if (!code) {
            offset--;
            continue;
        }

        UntrapOpcode untrap(cx, script, script->code + offset);

        if (codeArray[offset].loopBackedge) {
            /*
             * This is the head of a loop, we need to go and make sure that any
             * variables live at the head are live at the backedge and points prior.
             * For each such variable, look for the last lifetime segment in the body
             * and extend it to the end of the loop.
             */
            unsigned backedge = codeArray[offset].loopBackedge;
            for (unsigned i = 0; i < script->nfixed; i++) {
                if (locals[i].lifetime && !extendVariable(cx, locals[i], offset, backedge))
                    return false;
            }
            for (unsigned i = 0; fun && i < fun->nargs; i++) {
                if (args[i].lifetime && !extendVariable(cx, args[i], offset, backedge))
                    return false;
            }
        }

        jsbytecode *pc = script->code + offset;
        JSOp op = (JSOp) *pc;

        switch (op) {
          case JSOP_GETARG:
          case JSOP_CALLARG:
          case JSOP_INCARG:
          case JSOP_DECARG:
          case JSOP_ARGINC:
          case JSOP_ARGDEC:
          case JSOP_GETARGPROP: {
            unsigned arg = GET_ARGNO(pc);
            if (!analysis->argEscapes(arg)) {
                if (!addVariable(cx, args[arg], offset))
                    return false;
            }
            break;
          }

          case JSOP_SETARG: {
            unsigned arg = GET_ARGNO(pc);
            if (!analysis->argEscapes(arg))
                killVariable(cx, args[arg], offset);
            break;
          }

          case JSOP_GETLOCAL:
          case JSOP_CALLLOCAL:
          case JSOP_INCLOCAL:
          case JSOP_DECLOCAL:
          case JSOP_LOCALINC:
          case JSOP_LOCALDEC:
          case JSOP_GETLOCALPROP: {
            unsigned local = GET_SLOTNO(pc);
            if (!analysis->localEscapes(local)) {
                if (!addVariable(cx, locals[local], offset))
                    return false;
            }
            break;
          }

          case JSOP_SETLOCAL:
          case JSOP_SETLOCALPOP: {
            unsigned local = GET_SLOTNO(pc);
            if (!analysis->localEscapes(local))
                killVariable(cx, locals[local], offset);
            break;
          }

          case JSOP_THIS:
          case JSOP_GETTHISPROP:
            if (!addVariable(cx, thisVar, offset))
                return false;
            break;

          case JSOP_IFEQ:
          case JSOP_IFEQX:
          case JSOP_IFNE:
          case JSOP_IFNEX:
          case JSOP_OR:
          case JSOP_ORX:
          case JSOP_AND:
          case JSOP_ANDX:
          case JSOP_GOTO:
          case JSOP_GOTOX: {
            uint32 targetOffset = offset + GetJumpOffset(pc, pc);
            if (targetOffset < offset) {
                JSOp nop = JSOp(script->code[targetOffset]);
                if (nop == JSOP_GOTO || nop == JSOP_GOTOX) {
                    /* This is a continue, short circuit the backwards goto. */
                    jsbytecode *target = script->code + targetOffset;
                    targetOffset = targetOffset + GetJumpOffset(target, target);
                } else {
                    /* This is a loop back edge, no lifetime to pull in yet. */
                    JS_ASSERT(nop == JSOP_TRACE || nop == JSOP_NOTRACE);
                    codeArray[targetOffset].loopBackedge = offset;
                    break;
                }
            }
            for (unsigned i = 0; i < savedCount; i++) {
                LifetimeVariable &var = *saved[i];
                JS_ASSERT(!var.lifetime && var.saved);
                if (!var.savedEnd) {
                    /*
                     * This jump precedes the basic block which killed the variable,
                     * remember it and use it for the end of the next lifetime
                     * segment should the variable become live again. This is needed
                     * for loops, as if we wrap liveness around the loop the isLive
                     * test below may have given the wrong answer.
                     */
                    var.savedEnd = offset;
                }
                if (var.live(targetOffset)) {
                    /*
                     * Jumping to a place where this variable is live. Make a new
                     * lifetime segment for the variable.
                     */
                    var.lifetime = ArenaNew<Lifetime>(pool, offset, var.saved);
                    if (!var.lifetime)
                        return false;
                    var.saved = NULL;
                    saved[i--] = saved[--savedCount];
                }
            }
            break;
          }

          case JSOP_LOOKUPSWITCH:
          case JSOP_LOOKUPSWITCHX:
          case JSOP_TABLESWITCH:
          case JSOP_TABLESWITCHX:
            /* Restore all saved variables. :FIXME: maybe do this precisely. */
            for (unsigned i = 0; i < savedCount; i++) {
                LifetimeVariable &var = *saved[i];
                var.lifetime = ArenaNew<Lifetime>(pool, offset, var.saved);
                if (!var.lifetime)
                    return false;
                var.saved = NULL;
                saved[i--] = saved[--savedCount];
            }
            savedCount = 0;
            break;

          default:;
        }

        offset--;
    }

    return true;
}

#ifdef DEBUG
void
LifetimeScript::dumpVariable(LifetimeVariable &var)
{
    Lifetime *segment = var.lifetime ? var.lifetime : var.saved;
    while (segment) {
        printf(" (%u,%u%s)", segment->start, segment->end, segment->loopTail ? ",tail" : "");
        segment = segment->next;
    }
    printf("\n");
}
#endif /* DEBUG */

inline bool
LifetimeScript::addVariable(JSContext *cx, LifetimeVariable &var, unsigned offset)
{
    if (var.lifetime) {
        JS_ASSERT(offset < var.lifetime->start);
        var.lifetime->start = offset;
    } else {
        if (var.saved) {
            /* Remove from the list of saved entries. */
            for (unsigned i = 0; i < savedCount; i++) {
                if (saved[i] == &var) {
                    JS_ASSERT(savedCount);
                    saved[i--] = saved[--savedCount];
                    break;
                }
            }
        }
        var.lifetime = ArenaNew<Lifetime>(pool, offset, var.saved);
        if (!var.lifetime)
            return false;
        var.saved = NULL;
    }
    return true;
}

inline void
LifetimeScript::killVariable(JSContext *cx, LifetimeVariable &var, unsigned offset)
{
    if (!var.lifetime)
        return;
    JS_ASSERT(offset < var.lifetime->start);

    /*
     * The variable is considered to be live at the bytecode which kills it
     * (just not at earlier bytecodes). This behavior is needed by downstream
     * register allocation (see FrameState::bestEvictReg).
     */
    var.lifetime->start = offset;

    var.saved = var.lifetime;
    var.savedEnd = 0;
    var.lifetime = NULL;

    saved[savedCount++] = &var;
}

inline bool
LifetimeScript::extendVariable(JSContext *cx, LifetimeVariable &var, unsigned start, unsigned end)
{
    JS_ASSERT(var.lifetime);
    var.lifetime->start = start;

    Lifetime *segment = var.lifetime;
    if (segment->start >= end)
        return true;
    while (segment->next && segment->next->start < end)
        segment = segment->next;
    if (segment->end >= end)
        return true;

    Lifetime *tail = ArenaNew<Lifetime>(pool, end, segment->next);
    if (!tail)
        return false;
    tail->start = segment->end;
    tail->loopTail = true;
    segment->next = tail;

    return true;
}

} /* namespace analyze */
} /* namespace js */
