/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "vm/SavedStacks.h"

#include "mozilla/Attributes.h"

#include <math.h>

#include "jsapi.h"
#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jshashutil.h"
#include "jsmath.h"
#include "jsnum.h"

#include "gc/Marking.h"
#include "js/Vector.h"
#include "vm/Debugger.h"
#include "vm/GlobalObject.h"
#include "vm/StringBuffer.h"

#include "jscntxtinlines.h"

#include "vm/ObjectImpl-inl.h"

using mozilla::AddToHash;
using mozilla::HashString;

namespace js {

struct SavedFrame::Lookup {
    Lookup(JSAtom *source, uint32_t line, uint32_t column, JSAtom *functionDisplayName,
           SavedFrame *parent, JSPrincipals *principals)
      : source(source),
        line(line),
        column(column),
        functionDisplayName(functionDisplayName),
        parent(parent),
        principals(principals)
    {
        MOZ_ASSERT(source);
    }

    JSAtom       *source;
    uint32_t     line;
    uint32_t     column;
    JSAtom       *functionDisplayName;
    SavedFrame   *parent;
    JSPrincipals *principals;
};

class SavedFrame::AutoLookupRooter : public JS::CustomAutoRooter
{
  public:
    AutoLookupRooter(JSContext *cx, JSAtom *source, uint32_t line, uint32_t column,
                     JSAtom *functionDisplayName, SavedFrame *parent, JSPrincipals *principals)
      : JS::CustomAutoRooter(cx),
        value(source, line, column, functionDisplayName, parent, principals) {}

    operator const SavedFrame::Lookup&() const { return value; }
    SavedFrame::Lookup &get() { return value; }

  private:
    virtual void trace(JSTracer *trc) {
        gc::MarkStringUnbarriered(trc, &value.source, "SavedFrame::Lookup::source");
        if (value.functionDisplayName) {
            gc::MarkStringUnbarriered(trc, &value.functionDisplayName,
                                      "SavedFrame::Lookup::functionDisplayName");
        }
        if (value.parent)
            gc::MarkObjectUnbarriered(trc, &value.parent, "SavedFrame::Lookup::parent");
    }

    SavedFrame::Lookup value;
};

class SavedFrame::HandleLookup
{
  public:
    MOZ_IMPLICIT HandleLookup(SavedFrame::AutoLookupRooter &lookup) : ref(lookup) { }
    SavedFrame::Lookup *operator->() { return &ref.get(); }
    operator const SavedFrame::Lookup&() const { return ref; }
  private:
    SavedFrame::AutoLookupRooter &ref;
};

/* static */ HashNumber
SavedFrame::HashPolicy::hash(const Lookup &lookup)
{
    JS::AutoCheckCannotGC nogc;
    // Assume that we can take line mod 2^32 without losing anything of
    // interest.  If that assumption changes, we'll just need to start with 0
    // and add another overload of AddToHash with more arguments.
    return AddToHash(lookup.line,
                     lookup.column,
                     lookup.source,
                     lookup.functionDisplayName,
                     SavedFramePtrHasher::hash(lookup.parent),
                     JSPrincipalsPtrHasher::hash(lookup.principals));
}

/* static */ bool
SavedFrame::HashPolicy::match(SavedFrame *existing, const Lookup &lookup)
{
    if (existing->getLine() != lookup.line)
        return false;

    if (existing->getColumn() != lookup.column)
        return false;

    if (existing->getParent() != lookup.parent)
        return false;

    if (existing->getPrincipals() != lookup.principals)
        return false;

    JSAtom *source = existing->getSource();
    if (source != lookup.source)
        return false;

    JSAtom *functionDisplayName = existing->getFunctionDisplayName();
    if (functionDisplayName != lookup.functionDisplayName)
        return false;

    return true;
}

/* static */ void
SavedFrame::HashPolicy::rekey(Key &key, const Key &newKey)
{
    key = newKey;
}

/* static */ const Class SavedFrame::class_ = {
    "SavedFrame",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(SavedFrame::JSSLOT_COUNT),

    JS_PropertyStub,       // addProperty
    JS_DeletePropertyStub, // delProperty
    JS_PropertyStub,       // getProperty
    JS_StrictPropertyStub, // setProperty
    JS_EnumerateStub,      // enumerate
    JS_ResolveStub,        // resolve
    JS_ConvertStub,        // convert

    SavedFrame::finalize   // finalize
};

/* static */ void
SavedFrame::finalize(FreeOp *fop, JSObject *obj)
{
    JSPrincipals *p = obj->as<SavedFrame>().getPrincipals();
    if (p) {
        JSRuntime *rt = obj->runtimeFromMainThread();
        JS_DropPrincipals(rt, p);
    }
}

JSAtom *
SavedFrame::getSource()
{
    const Value &v = getReservedSlot(JSSLOT_SOURCE);
    JSString *s = v.toString();
    return &s->asAtom();
}

uint32_t
SavedFrame::getLine()
{
    const Value &v = getReservedSlot(JSSLOT_LINE);
    return v.toInt32();
}

uint32_t
SavedFrame::getColumn()
{
    const Value &v = getReservedSlot(JSSLOT_COLUMN);
    return v.toInt32();
}

JSAtom *
SavedFrame::getFunctionDisplayName()
{
    const Value &v = getReservedSlot(JSSLOT_FUNCTIONDISPLAYNAME);
    if (v.isNull())
        return nullptr;
    JSString *s = v.toString();
    return &s->asAtom();
}

SavedFrame *
SavedFrame::getParent()
{
    const Value &v = getReservedSlot(JSSLOT_PARENT);
    return v.isObject() ? &v.toObject().as<SavedFrame>() : nullptr;
}

JSPrincipals *
SavedFrame::getPrincipals()
{
    const Value &v = getReservedSlot(JSSLOT_PRINCIPALS);
    if (v.isUndefined())
        return nullptr;
    return static_cast<JSPrincipals *>(v.toPrivate());
}

void
SavedFrame::initFromLookup(SavedFrame::HandleLookup lookup)
{
    MOZ_ASSERT(lookup->source);
    MOZ_ASSERT(getReservedSlot(JSSLOT_SOURCE).isUndefined());
    setReservedSlot(JSSLOT_SOURCE, StringValue(lookup->source));

    setReservedSlot(JSSLOT_LINE, NumberValue(lookup->line));
    setReservedSlot(JSSLOT_COLUMN, NumberValue(lookup->column));
    setReservedSlot(JSSLOT_FUNCTIONDISPLAYNAME,
                    lookup->functionDisplayName
                        ? StringValue(lookup->functionDisplayName)
                        : NullValue());
    setReservedSlot(JSSLOT_PARENT, ObjectOrNullValue(lookup->parent));
    setReservedSlot(JSSLOT_PRIVATE_PARENT, PrivateValue(lookup->parent));

    MOZ_ASSERT(getReservedSlot(JSSLOT_PRINCIPALS).isUndefined());
    if (lookup->principals)
        JS_HoldPrincipals(lookup->principals);
    setReservedSlot(JSSLOT_PRINCIPALS, PrivateValue(lookup->principals));
}

bool
SavedFrame::parentMoved()
{
    const Value &v = getReservedSlot(JSSLOT_PRIVATE_PARENT);
    JSObject *p = static_cast<JSObject *>(v.toPrivate());
    return p == getParent();
}

void
SavedFrame::updatePrivateParent()
{
    setReservedSlot(JSSLOT_PRIVATE_PARENT, PrivateValue(getParent()));
}

bool
SavedFrame::isSelfHosted()
{
    JSAtom *source = getSource();
    return StringEqualsAscii(source, "self-hosted");
}

/* static */ bool
SavedFrame::construct(JSContext *cx, unsigned argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                         "SavedFrame");
    return false;
}

/* static */ SavedFrame *
SavedFrame::checkThis(JSContext *cx, CallArgs &args, const char *fnName)
{
    const Value &thisValue = args.thisv();

    if (!thisValue.isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT);
        return nullptr;
    }

    JSObject &thisObject = thisValue.toObject();
    if (!thisObject.is<SavedFrame>()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                             SavedFrame::class_.name, fnName, thisObject.getClass()->name);
        return nullptr;
    }

    // Check for SavedFrame.prototype, which has the same class as SavedFrame
    // instances, however doesn't actually represent a captured stack frame. It
    // is the only object that is<SavedFrame>() but doesn't have a source.
    if (thisObject.as<SavedFrame>().getReservedSlot(JSSLOT_SOURCE).isNull()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                             SavedFrame::class_.name, fnName, "prototype object");
        return nullptr;
    }

    return &thisObject.as<SavedFrame>();
}

// Get the SavedFrame * from the current this value and handle any errors that
// might occur therein.
//
// These parameters must already exist when calling this macro:
//   - JSContext  *cx
//   - unsigned   argc
//   - Value      *vp
//   - const char *fnName
// These parameters will be defined after calling this macro:
//   - CallArgs args
//   - Rooted<SavedFrame *> frame (will be non-null)
#define THIS_SAVEDFRAME(cx, argc, vp, fnName, args, frame)         \
    CallArgs args = CallArgsFromVp(argc, vp);                      \
    RootedSavedFrame frame(cx, checkThis(cx, args, fnName));   \
    if (!frame)                                                    \
        return false

/* static */ bool
SavedFrame::sourceProperty(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_SAVEDFRAME(cx, argc, vp, "(get source)", args, frame);
    args.rval().setString(frame->getSource());
    return true;
}

/* static */ bool
SavedFrame::lineProperty(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_SAVEDFRAME(cx, argc, vp, "(get line)", args, frame);
    uint32_t line = frame->getLine();
    args.rval().setNumber(line);
    return true;
}

/* static */ bool
SavedFrame::columnProperty(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_SAVEDFRAME(cx, argc, vp, "(get column)", args, frame);
    uint32_t column = frame->getColumn();
    args.rval().setNumber(column);
    return true;
}

/* static */ bool
SavedFrame::functionDisplayNameProperty(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_SAVEDFRAME(cx, argc, vp, "(get functionDisplayName)", args, frame);
    RootedAtom name(cx, frame->getFunctionDisplayName());
    if (name)
        args.rval().setString(name);
    else
        args.rval().setNull();
    return true;
}

/* static */ bool
SavedFrame::parentProperty(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_SAVEDFRAME(cx, argc, vp, "(get parent)", args, frame);
    JSSubsumesOp subsumes = cx->runtime()->securityCallbacks->subsumes;
    JSPrincipals *principals = cx->compartment()->principals;

    do
        frame = frame->getParent();
    while (frame && principals && subsumes &&
           !subsumes(principals, frame->getPrincipals()));

    args.rval().setObjectOrNull(frame);
    return true;
}

/* static */ const JSPropertySpec SavedFrame::properties[] = {
    JS_PSG("source", SavedFrame::sourceProperty, 0),
    JS_PSG("line", SavedFrame::lineProperty, 0),
    JS_PSG("column", SavedFrame::columnProperty, 0),
    JS_PSG("functionDisplayName", SavedFrame::functionDisplayNameProperty, 0),
    JS_PSG("parent", SavedFrame::parentProperty, 0),
    JS_PS_END
};

/* static */ bool
SavedFrame::toStringMethod(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_SAVEDFRAME(cx, argc, vp, "toString", args, frame);
    StringBuffer sb(cx);
    JSSubsumesOp subsumes = cx->runtime()->securityCallbacks->subsumes;
    JSPrincipals *principals = cx->compartment()->principals;

    do {
        if (principals && subsumes && !subsumes(principals, frame->getPrincipals()))
            continue;
        if (frame->isSelfHosted())
            continue;

        RootedAtom name(cx, frame->getFunctionDisplayName());
        if ((name && !sb.append(name))
            || !sb.append('@')
            || !sb.append(frame->getSource())
            || !sb.append(':')
            || !NumberValueToStringBuffer(cx, NumberValue(frame->getLine()), sb)
            || !sb.append(':')
            || !NumberValueToStringBuffer(cx, NumberValue(frame->getColumn()), sb)
            || !sb.append('\n')) {
            return false;
        }
    } while ((frame = frame->getParent()));

    args.rval().setString(sb.finishString());
    return true;
}

/* static */ const JSFunctionSpec SavedFrame::methods[] = {
    JS_FN("constructor", SavedFrame::construct, 0, 0),
    JS_FN("toString", SavedFrame::toStringMethod, 0, 0),
    JS_FS_END
};

bool
SavedStacks::init()
{
    if (!pcLocationMap.init())
        return false;

    return frames.init();
}

bool
SavedStacks::saveCurrentStack(JSContext *cx, MutableHandleSavedFrame frame, unsigned maxFrameCount)
{
    MOZ_ASSERT(initialized());
    assertSameCompartment(cx, this);

    FrameIter iter(cx, FrameIter::ALL_CONTEXTS, FrameIter::GO_THROUGH_SAVED);
    return insertFrames(cx, iter, frame, maxFrameCount);
}

void
SavedStacks::sweep(JSRuntime *rt)
{
    if (frames.initialized()) {
        for (SavedFrame::Set::Enum e(frames); !e.empty(); e.popFront()) {
            JSObject *obj = static_cast<JSObject *>(e.front());
            JSObject *temp = obj;

            if (IsObjectAboutToBeFinalized(&obj)) {
                e.removeFront();
            } else {
                SavedFrame *frame = &obj->as<SavedFrame>();
                bool parentMoved = frame->parentMoved();

                if (parentMoved) {
                    frame->updatePrivateParent();
                }

                if (obj != temp || parentMoved) {
                    e.rekeyFront(SavedFrame::Lookup(frame->getSource(),
                                                    frame->getLine(),
                                                    frame->getColumn(),
                                                    frame->getFunctionDisplayName(),
                                                    frame->getParent(),
                                                    frame->getPrincipals()),
                                 ReadBarriered<SavedFrame *>(frame));
                }
            }
        }
    }

    sweepPCLocationMap();

    if (savedFrameProto && IsObjectAboutToBeFinalized(savedFrameProto.unsafeGet())) {
        savedFrameProto.set(nullptr);
    }
}

void
SavedStacks::trace(JSTracer *trc)
{
    if (!pcLocationMap.initialized())
        return;

    // Mark each of the source strings in our pc to location cache.
    for (PCLocationMap::Enum e(pcLocationMap); !e.empty(); e.popFront()) {
        LocationValue &loc = e.front().value();
        MarkString(trc, &loc.source, "SavedStacks::PCLocationMap's memoized script source name");
    }
}

uint32_t
SavedStacks::count()
{
    MOZ_ASSERT(initialized());
    return frames.count();
}

void
SavedStacks::clear()
{
    frames.clear();
}

size_t
SavedStacks::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    return frames.sizeOfExcludingThis(mallocSizeOf);
}

bool
SavedStacks::insertFrames(JSContext *cx, FrameIter &iter, MutableHandleSavedFrame frame,
                          unsigned maxFrameCount)
{
    // In order to lookup a cached SavedFrame object, we need to have its parent
    // SavedFrame, which means we need to walk the stack from oldest frame to
    // youngest. However, FrameIter walks the stack from youngest frame to
    // oldest. The solution is to append stack frames to a vector as we walk the
    // stack with FrameIter, and then do a second pass through that vector in
    // reverse order after the traversal has completed and get or create the
    // SavedFrame objects at that time.
    //
    // To avoid making many copies of FrameIter (whose copy constructor is
    // relatively slow), we save the subset of FrameIter's data that is relevant
    // to our needs in a FrameState object, and maintain a vector of FrameState
    // objects instead of a vector of FrameIter objects.

    // Accumulate the vector of FrameState objects in |stackState|.
    AutoFrameStateVector stackState(cx);
    while (!iter.done()) {
        AutoLocationValueRooter location(cx);

        {
            AutoCompartment ac(cx, iter.compartment());
            if (!cx->compartment()->savedStacks().getLocation(cx, iter, &location))
                return false;
        }

        {
            FrameState frameState(iter);
            frameState.location = location.get();
            if (!stackState->append(frameState))
                return false;
        }

        ++iter;

        if (maxFrameCount == 0) {
            // If maxFrameCount is zero, then there's no limit on the number of
            // frames.
            continue;
        } else if (maxFrameCount == 1) {
            // Since we were only asked to save one frame, do not continue
            // walking the stack and saving frame state.
            break;
        } else {
            maxFrameCount--;
        }
    }

    // Iterate through |stackState| in reverse order and get or create the
    // actual SavedFrame instances.
    RootedSavedFrame parentFrame(cx, nullptr);
    for (size_t i = stackState->length(); i != 0; i--) {
        SavedFrame::AutoLookupRooter lookup(cx,
                                            stackState[i-1].location.source,
                                            stackState[i-1].location.line,
                                            stackState[i-1].location.column,
                                            stackState[i-1].name,
                                            parentFrame,
                                            stackState[i-1].principals);
        parentFrame.set(getOrCreateSavedFrame(cx, lookup));
        if (!parentFrame)
            return false;
    }

    frame.set(parentFrame);
    return true;
}

SavedFrame *
SavedStacks::getOrCreateSavedFrame(JSContext *cx, SavedFrame::HandleLookup lookup)
{
    DependentAddPtr<SavedFrame::Set> p(cx, frames, lookup);
    if (p)
        return *p;

    RootedSavedFrame frame(cx, createFrameFromLookup(cx, lookup));
    if (!frame)
        return nullptr;

    if (!p.add(cx, frames, lookup, frame))
        return nullptr;

    return frame;
}

JSObject *
SavedStacks::getOrCreateSavedFramePrototype(JSContext *cx)
{
    if (savedFrameProto)
        return savedFrameProto;

    Rooted<GlobalObject *> global(cx, cx->compartment()->maybeGlobal());
    if (!global)
        return nullptr;

    RootedNativeObject proto(cx,
        NewNativeObjectWithGivenProto(cx, &SavedFrame::class_,
                                      global->getOrCreateObjectPrototype(cx),
                                      global));
    if (!proto
        || !JS_DefineProperties(cx, proto, SavedFrame::properties)
        || !JS_DefineFunctions(cx, proto, SavedFrame::methods)
        || !JSObject::freeze(cx, proto))
    {
        return nullptr;
    }

    // The only object with the SavedFrame::class_ that doesn't have a source
    // should be the prototype.
    proto->setReservedSlot(SavedFrame::JSSLOT_SOURCE, NullValue());

    savedFrameProto.set(proto);
    return savedFrameProto;
}

SavedFrame *
SavedStacks::createFrameFromLookup(JSContext *cx, SavedFrame::HandleLookup lookup)
{
    RootedObject proto(cx, getOrCreateSavedFramePrototype(cx));
    if (!proto)
        return nullptr;

    assertSameCompartment(cx, proto);

    RootedObject global(cx, cx->compartment()->maybeGlobal());
    if (!global)
        return nullptr;

    assertSameCompartment(cx, global);

    RootedObject frameObj(cx, NewObjectWithGivenProto(cx, &SavedFrame::class_, proto, global));
    if (!frameObj)
        return nullptr;

    RootedSavedFrame f(cx, &frameObj->as<SavedFrame>());
    f->initFromLookup(lookup);

    if (!JSObject::freeze(cx, frameObj))
        return nullptr;

    return f.get();
}

/*
 * Remove entries from the table whose JSScript is being collected.
 */
void
SavedStacks::sweepPCLocationMap()
{
    for (PCLocationMap::Enum e(pcLocationMap); !e.empty(); e.popFront()) {
        PCKey key = e.front().key();
        JSScript *script = key.script.get();
        if (IsScriptAboutToBeFinalized(&script)) {
            e.removeFront();
        } else if (script != key.script.get()) {
            key.script = script;
            e.rekeyFront(key);
        }
    }
}

bool
SavedStacks::getLocation(JSContext *cx, const FrameIter &iter, MutableHandleLocationValue locationp)
{
    // We should only ever be caching location values for scripts in this
    // compartment. Otherwise, we would get dead cross-compartment scripts in
    // the cache because our compartment's sweep method isn't called when their
    // compartment gets collected.
    assertSameCompartment(cx, this, iter.compartment());

    // When we have a |JSScript| for this frame, use a potentially memoized
    // location from our PCLocationMap and copy it into |locationp|. When we do
    // not have a |JSScript| for this frame (asm.js frames), we take a slow path
    // that doesn't employ memoization, and update |locationp|'s slots directly.

    if (!iter.hasScript()) {
        const char *filename = iter.scriptFilename();
        if (!filename)
            filename = "";
        locationp->source = Atomize(cx, filename, strlen(filename));
        if (!locationp->source)
            return false;

        locationp->line = iter.computeLine(&locationp->column);
        return true;
    }

    RootedScript script(cx, iter.script());
    jsbytecode *pc = iter.pc();

    PCKey key(script, pc);
    PCLocationMap::AddPtr p = pcLocationMap.lookupForAdd(key);

    if (!p) {
        const char *filename = script->filename() ? script->filename() : "";
        RootedAtom source(cx, Atomize(cx, filename, strlen(filename)));
        if (!source)
            return false;

        uint32_t column;
        uint32_t line = PCToLineNumber(script, pc, &column);

        LocationValue value(source, line, column);
        if (!pcLocationMap.add(p, key, value))
            return false;
    }

    locationp.set(p->value());
    return true;
}

void
SavedStacks::chooseSamplingProbability(JSContext *cx)
{
    GlobalObject::DebuggerVector *dbgs = cx->global()->getDebuggers();
    if (!dbgs || dbgs->empty())
        return;

    Debugger *allocationTrackingDbg = nullptr;
    mozilla::DebugOnly<Debugger **> begin = dbgs->begin();

    for (Debugger **dbgp = dbgs->begin(); dbgp < dbgs->end(); dbgp++) {
        // The set of debuggers had better not change while we're iterating,
        // such that the vector gets reallocated.
        MOZ_ASSERT(dbgs->begin() == begin);

        if ((*dbgp)->trackingAllocationSites && (*dbgp)->enabled)
            allocationTrackingDbg = *dbgp;
    }

    if (!allocationTrackingDbg)
        return;

    allocationSamplingProbability = allocationTrackingDbg->allocationSamplingProbability;
}

SavedStacks::FrameState::FrameState(const FrameIter &iter)
    : principals(iter.compartment()->principals),
      name(iter.isNonEvalFunctionFrame() ? iter.functionDisplayAtom() : nullptr),
      location()
{
    if (principals)
        JS_HoldPrincipals(principals);
}

SavedStacks::FrameState::FrameState(const FrameState &fs)
    : principals(fs.principals),
      name(fs.name),
      location(fs.location)
{
    if (principals)
        JS_HoldPrincipals(principals);
}

SavedStacks::FrameState::~FrameState() {
    if (principals)
        JS_DropPrincipals(TlsPerThreadData.get()->runtimeFromMainThread(), principals);
}

void
SavedStacks::FrameState::trace(JSTracer *trc) {
    if (name)
        gc::MarkStringUnbarriered(trc, &name, "SavedStacks::FrameState::name");
    location.trace(trc);
}

bool
SavedStacksMetadataCallback(JSContext *cx, JSObject **pmetadata)
{
    SavedStacks &stacks = cx->compartment()->savedStacks();
    if (stacks.allocationSkipCount > 0) {
        stacks.allocationSkipCount--;
        return true;
    }

    stacks.chooseSamplingProbability(cx);
    if (stacks.allocationSamplingProbability == 0.0)
        return true;

    // If the sampling probability is set to 1.0, we are always taking a sample
    // and can therefore leave allocationSkipCount at 0.
    if (stacks.allocationSamplingProbability != 1.0) {
        // Rather than generating a random number on every allocation to decide
        // if we want to sample that particular allocation (which would be
        // expensive), we calculate the number of allocations to skip before
        // taking the next sample.
        //
        // P = the probability we sample any given event.
        //
        // ~P = 1-P, the probability we don't sample a given event.
        //
        // (~P)^n = the probability that we skip at least the next n events.
        //
        // let X = random between 0 and 1.
        //
        // floor(log base ~P of X) = n, aka the number of events we should skip
        // until we take the next sample. Any value for X less than (~P)^n
        // yields a skip count greater than n, so the likelihood of a skip count
        // greater than n is (~P)^n, as required.
        double notSamplingProb = 1.0 - stacks.allocationSamplingProbability;
        stacks.allocationSkipCount = std::floor(std::log(random_nextDouble(&stacks.rngState)) /
                                                std::log(notSamplingProb));
    }

    RootedSavedFrame frame(cx);
    if (!stacks.saveCurrentStack(cx, &frame))
        return false;
    *pmetadata = frame;

    return Debugger::onLogAllocationSite(cx, frame);
}

#ifdef JS_CRASH_DIAGNOSTICS
void
CompartmentChecker::check(SavedStacks *stacks)
{
    if (&compartment->savedStacks() != stacks) {
        printf("*** Compartment SavedStacks mismatch: %p vs. %p\n",
               (void *) &compartment->savedStacks(), stacks);
        MOZ_CRASH();
    }
}
#endif /* JS_CRASH_DIAGNOSTICS */

} /* namespace js */
