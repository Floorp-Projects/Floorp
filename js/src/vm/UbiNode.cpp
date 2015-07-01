/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/UbiNode.h"

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Range.h"
#include "mozilla/Scoped.h"

#include "jscntxt.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jsstr.h"

#include "jit/IonCode.h"
#include "js/Debug.h"
#include "js/TracingAPI.h"
#include "js/TypeDecls.h"
#include "js/Utility.h"
#include "js/Vector.h"
#include "vm/GlobalObject.h"
#include "vm/ScopeObject.h"
#include "vm/Shape.h"
#include "vm/String.h"
#include "vm/Symbol.h"

#include "jsobjinlines.h"
#include "vm/Debugger-inl.h"

using mozilla::Some;
using mozilla::UniquePtr;
using JS::HandleValue;
using JS::Value;
using JS::ZoneSet;
using JS::ubi::Concrete;
using JS::ubi::Edge;
using JS::ubi::EdgeRange;
using JS::ubi::Node;
using JS::ubi::SimpleEdge;
using JS::ubi::SimpleEdgeVector;
using JS::ubi::TracerConcrete;
using JS::ubi::TracerConcreteWithCompartment;

// All operations on null ubi::Nodes crash.
const char16_t* Concrete<void>::typeName() const          { MOZ_CRASH("null ubi::Node"); }
JS::Zone* Concrete<void>::zone() const                    { MOZ_CRASH("null ubi::Node"); }
JSCompartment* Concrete<void>::compartment() const        { MOZ_CRASH("null ubi::Node"); }

UniquePtr<EdgeRange>
Concrete<void>::edges(JSContext*, bool) const {
    MOZ_CRASH("null ubi::Node");
}

size_t
Concrete<void>::size(mozilla::MallocSizeOf mallocSizeof) const
{
    MOZ_CRASH("null ubi::Node");
}

struct Node::ConstructFunctor : public js::BoolDefaultAdaptor<Value, false> {
    template <typename T> bool operator()(T* t, Node* node) { node->construct(t); return true; }
};

Node::Node(const JS::GCCellPtr &thing)
{
    js::gc::CallTyped(ConstructFunctor(), thing.asCell(), thing.kind(), this);
}

Node::Node(HandleValue value)
{
    if (!DispatchValueTyped(ConstructFunctor(), value, this))
        construct<void>(nullptr);
}

Value
Node::exposeToJS() const
{
    Value v;

    if (is<JSObject>()) {
        JSObject& obj = *as<JSObject>();
        if (obj.is<js::ScopeObject>()) {
            v.setUndefined();
        } else if (obj.is<JSFunction>() && js::IsInternalFunctionObject(obj)) {
            v.setUndefined();
        } else {
            v.setObject(obj);
        }
    } else if (is<JSString>()) {
        v.setString(as<JSString>());
    } else if (is<JS::Symbol>()) {
        v.setSymbol(as<JS::Symbol>());
    } else {
        v.setUndefined();
    }

    return v;
}


// A JS::CallbackTracer subclass that adds a SimpleEdge to a Vector for each
// edge on which it is invoked.
class SimpleEdgeVectorTracer : public JS::CallbackTracer {
    // The vector to which we add SimpleEdges.
    SimpleEdgeVector* vec;

    // True if we should populate the edge's names.
    bool wantNames;

    void onChild(const JS::GCCellPtr& thing) override {
        if (!okay)
            return;

        // Don't trace permanent atoms and well-known symbols that are owned by
        // a parent JSRuntime.
        if (thing.isString() && thing.toString()->isPermanentAtom())
            return;
        if (thing.isSymbol() && thing.toSymbol()->isWellKnownSymbol())
            return;

        char16_t* name16 = nullptr;
        if (wantNames) {
            // Ask the tracer to compute an edge name for us.
            char buffer[1024];
            getTracingEdgeName(buffer, sizeof(buffer));
            const char* name = buffer;

            // Convert the name to char16_t characters.
            name16 = js_pod_malloc<char16_t>(strlen(name) + 1);
            if (!name16) {
                okay = false;
                return;
            }

            size_t i;
            for (i = 0; name[i]; i++)
                name16[i] = name[i];
            name16[i] = '\0';
        }

        // The simplest code is correct! The temporary SimpleEdge takes
        // ownership of name; if the append succeeds, the vector element
        // then takes ownership; if the append fails, then the temporary
        // retains it, and its destructor will free it.
        if (!vec->append(mozilla::Move(SimpleEdge(name16, Node(thing))))) {
            okay = false;
            return;
        }
    }

  public:
    // True if no errors (OOM, say) have yet occurred.
    bool okay;

    SimpleEdgeVectorTracer(JSContext* cx, SimpleEdgeVector* vec, bool wantNames)
      : JS::CallbackTracer(JS_GetRuntime(cx)),
        vec(vec),
        wantNames(wantNames),
        okay(true)
    { }
};


// An EdgeRange concrete class that simply holds a vector of SimpleEdges,
// populated by the init method.
class SimpleEdgeRange : public EdgeRange {
    SimpleEdgeVector edges;
    size_t i;

    void settle() {
        front_ = i < edges.length() ? &edges[i] : nullptr;
    }

  public:
    explicit SimpleEdgeRange(JSContext* cx) : edges(cx), i(0) { }

    bool init(JSContext* cx, void* thing, JS::TraceKind kind, bool wantNames = true) {
        SimpleEdgeVectorTracer tracer(cx, &edges, wantNames);
        JS_TraceChildren(&tracer, thing, kind);
        settle();
        return tracer.okay;
    }

    void popFront() override { i++; settle(); }
};


template<typename Referent>
JS::Zone*
TracerConcrete<Referent>::zone() const
{
    return get().zoneFromAnyThread();
}

template<typename Referent>
UniquePtr<EdgeRange>
TracerConcrete<Referent>::edges(JSContext* cx, bool wantNames) const {
    UniquePtr<SimpleEdgeRange, JS::DeletePolicy<SimpleEdgeRange>> range(
      cx->new_<SimpleEdgeRange>(cx));
    if (!range)
        return nullptr;

    if (!range->init(cx, ptr, ::js::gc::MapTypeToTraceKind<Referent>::kind, wantNames))
        return nullptr;

    return UniquePtr<EdgeRange>(range.release());
}

template<typename Referent>
JSCompartment*
TracerConcreteWithCompartment<Referent>::compartment() const
{
    return TracerBase::get().compartment();
}

const char*
Concrete<JSObject>::jsObjectClassName() const
{
    return Concrete::get().getClass()->name;
}

bool
Concrete<JSObject>::jsObjectConstructorName(JSContext* cx,
                                            UniquePtr<char16_t[], JS::FreePolicy>& outName) const
{
    JSAtom* name = Concrete::get().maybeConstructorDisplayAtom();
    if (!name) {
        outName.reset(nullptr);
        return true;
    }

    auto len = JS_GetStringLength(name);
    auto size = len + 1;

    outName.reset(cx->pod_malloc<char16_t>(size * sizeof(char16_t)));
    if (!outName)
        return false;

    mozilla::Range<char16_t> chars(outName.get(), size);
    if (!JS_CopyStringChars(cx, chars, name))
        return false;

    outName[len] = '\0';
    return true;
}

template<> const char16_t TracerConcrete<JS::Symbol>::concreteTypeName[] =
    MOZ_UTF16("JS::Symbol");
template<> const char16_t TracerConcrete<JSScript>::concreteTypeName[] =
    MOZ_UTF16("JSScript");
template<> const char16_t TracerConcrete<js::LazyScript>::concreteTypeName[] =
    MOZ_UTF16("js::LazyScript");
template<> const char16_t TracerConcrete<js::jit::JitCode>::concreteTypeName[] =
    MOZ_UTF16("js::jit::JitCode");
template<> const char16_t TracerConcrete<js::Shape>::concreteTypeName[] =
    MOZ_UTF16("js::Shape");
template<> const char16_t TracerConcrete<js::BaseShape>::concreteTypeName[] =
    MOZ_UTF16("js::BaseShape");
template<> const char16_t TracerConcrete<js::ObjectGroup>::concreteTypeName[] =
    MOZ_UTF16("js::ObjectGroup");


// Instantiate all the TracerConcrete and templates here, where
// we have the member functions' definitions in scope.
namespace JS {
namespace ubi {
template class TracerConcreteWithCompartment<JSObject>;
template class TracerConcrete<JSString>;
template class TracerConcrete<JS::Symbol>;
template class TracerConcreteWithCompartment<JSScript>;
template class TracerConcrete<js::LazyScript>;
template class TracerConcrete<js::jit::JitCode>;
template class TracerConcreteWithCompartment<js::Shape>;
template class TracerConcreteWithCompartment<js::BaseShape>;
template class TracerConcrete<js::ObjectGroup>;
}
}

namespace JS {
namespace ubi {

RootList::RootList(JSContext* cx, Maybe<AutoCheckCannotGC>& noGC, bool wantNames /* = false */)
  : noGC(noGC),
    cx(cx),
    edges(cx),
    wantNames(wantNames)
{ }


bool
RootList::init()
{
    SimpleEdgeVectorTracer tracer(cx, &edges, wantNames);
    JS_TraceRuntime(&tracer);
    if (!tracer.okay)
        return false;
    noGC.emplace(cx->runtime());
    return true;
}

bool
RootList::init(ZoneSet& debuggees)
{
    SimpleEdgeVector allRootEdges(cx);
    SimpleEdgeVectorTracer tracer(cx, &allRootEdges, wantNames);

    JS_TraceRuntime(&tracer);
    if (!tracer.okay)
        return false;
    JS_TraceIncomingCCWs(&tracer, debuggees);
    if (!tracer.okay)
        return false;

    for (SimpleEdgeVector::Range r = allRootEdges.all(); !r.empty(); r.popFront()) {
        SimpleEdge& edge = r.front();
        Zone* zone = edge.referent.zone();
        if (zone && !debuggees.has(zone))
            continue;
        if (!edges.append(mozilla::Move(edge)))
            return false;
    }

    noGC.emplace(cx->runtime());
    return true;
}

bool
RootList::init(HandleObject debuggees)
{
    MOZ_ASSERT(debuggees && JS::dbg::IsDebugger(*debuggees));
    js::Debugger* dbg = js::Debugger::fromJSObject(debuggees.get());

    ZoneSet debuggeeZones;
    if (!debuggeeZones.init())
        return false;

    for (js::WeakGlobalObjectSet::Range r = dbg->allDebuggees(); !r.empty(); r.popFront()) {
        if (!debuggeeZones.put(r.front()->zone()))
            return false;
    }

    if (!init(debuggeeZones))
        return false;

    // Ensure that each of our debuggee globals are in the root list.
    for (js::WeakGlobalObjectSet::Range r = dbg->allDebuggees(); !r.empty(); r.popFront()) {
        if (!addRoot(JS::ubi::Node(static_cast<JSObject*>(r.front())),
                     MOZ_UTF16("debuggee global")))
        {
            return false;
        }
    }

    return true;
}

bool
RootList::addRoot(Node node, const char16_t* edgeName)
{
    MOZ_ASSERT(noGC.isSome());
    MOZ_ASSERT_IF(wantNames, edgeName);

    UniquePtr<char16_t[], JS::FreePolicy> name;
    if (edgeName) {
        name = DuplicateString(cx, edgeName);
        if (!name)
            return false;
    }

    return edges.append(mozilla::Move(SimpleEdge(name.release(), node)));
}

const char16_t Concrete<RootList>::concreteTypeName[] = MOZ_UTF16("RootList");

UniquePtr<EdgeRange>
Concrete<RootList>::edges(JSContext* cx, bool wantNames) const {
    MOZ_ASSERT_IF(wantNames, get().wantNames);
    return UniquePtr<EdgeRange>(cx->new_<PreComputedEdgeRange>(cx, get().edges));
}

} // namespace ubi
} // namespace JS
