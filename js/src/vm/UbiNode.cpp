/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/UbiNode.h"

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Scoped.h"

#include "jscntxt.h"
#include "jsinfer.h"
#include "jsobj.h"
#include "jsscript.h"

#include "jit/IonCode.h"
#include "js/TracingAPI.h"
#include "js/TypeDecls.h"
#include "js/Utility.h"
#include "js/Vector.h"
#include "vm/ScopeObject.h"
#include "vm/Shape.h"
#include "vm/String.h"
#include "vm/Symbol.h"

#include "jsobjinlines.h"

using JS::HandleValue;
using JS::Value;
using JS::ubi::Concrete;
using JS::ubi::Edge;
using JS::ubi::EdgeRange;
using JS::ubi::Node;
using JS::ubi::TracerConcrete;
using JS::ubi::TracerConcreteWithCompartment;

// All operations on null ubi::Nodes crash.
const char16_t *Concrete<void>::typeName() const          { MOZ_CRASH("null ubi::Node"); }
EdgeRange *Concrete<void>::edges(JSContext *, bool) const { MOZ_CRASH("null ubi::Node"); }
JS::Zone *Concrete<void>::zone() const                    { MOZ_CRASH("null ubi::Node"); }
JSCompartment *Concrete<void>::compartment() const        { MOZ_CRASH("null ubi::Node"); }

size_t
Concrete<void>::size(mozilla::MallocSizeOf mallocSizeof) const
{
    MOZ_CRASH("null ubi::Node");
}

Node::Node(JSGCTraceKind kind, void *ptr)
{
    switch (kind) {
      case JSTRACE_OBJECT:      construct(static_cast<JSObject *>(ptr));              break;
      case JSTRACE_STRING:      construct(static_cast<JSString *>(ptr));              break;
      case JSTRACE_SYMBOL:      construct(static_cast<JS::Symbol *>(ptr));            break;
      case JSTRACE_SCRIPT:      construct(static_cast<JSScript *>(ptr));              break;
      case JSTRACE_LAZY_SCRIPT: construct(static_cast<js::LazyScript *>(ptr));        break;
      case JSTRACE_JITCODE:     construct(static_cast<js::jit::JitCode *>(ptr));      break;
      case JSTRACE_SHAPE:       construct(static_cast<js::Shape *>(ptr));             break;
      case JSTRACE_BASE_SHAPE:  construct(static_cast<js::BaseShape *>(ptr));         break;
      case JSTRACE_TYPE_OBJECT: construct(static_cast<js::types::TypeObject *>(ptr)); break;

      default:
        MOZ_CRASH("bad JSGCTraceKind passed to JS::ubi::Node::Node");
    }
}

Node::Node(HandleValue value)
{
    if (value.isObject())
        construct(&value.toObject());
    else if (value.isString())
        construct(value.toString());
    else if (value.isSymbol())
        construct(value.toSymbol());
    else
        construct<void>(nullptr);
}

Value
Node::exposeToJS() const
{
    Value v;

    if (is<JSObject>()) {
        JSObject &obj = *as<JSObject>();
        if (obj.is<js::ScopeObject>()) {
            v.setUndefined();
        } else if (obj.is<JSFunction>() && js::IsInternalFunctionObject(&obj)) {
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

// A dumb Edge concrete class. All but the most essential members have the
// default behavior.
class SimpleEdge : public Edge {
    SimpleEdge(SimpleEdge &) MOZ_DELETE;
    SimpleEdge &operator=(const SimpleEdge &) MOZ_DELETE;

  public:
    SimpleEdge() : Edge() { }

    // Construct an initialized SimpleEdge, taking ownership of |name|.
    SimpleEdge(char16_t *name, const Node &referent) {
        this->name = name;
        this->referent = referent;
    }
    ~SimpleEdge() {
        js_free(const_cast<char16_t *>(name));
    }

    // Move construction and assignment.
    SimpleEdge(SimpleEdge &&rhs) {
        name = rhs.name;
        referent = rhs.referent;

        rhs.name = nullptr;
    }
    SimpleEdge &operator=(SimpleEdge &&rhs) {
        MOZ_ASSERT(&rhs != this);
        this->~SimpleEdge();
        new(this) SimpleEdge(mozilla::Move(rhs));
        return *this;
    }
};


typedef mozilla::Vector<SimpleEdge, 8, js::TempAllocPolicy> SimpleEdgeVector;


// A JSTracer subclass that adds a SimpleEdge to a Vector for each edge on
// which it is invoked.
class SimpleEdgeVectorTracer : public JSTracer {
    // The vector to which we add SimpleEdges.
    SimpleEdgeVector *vec;

    // True if we should populate the edge's names.
    bool wantNames;

    static void staticCallback(JSTracer *trc, void **thingp, JSGCTraceKind kind) {
        static_cast<SimpleEdgeVectorTracer *>(trc)->callback(thingp, kind);
    }

    void callback(void **thingp, JSGCTraceKind kind) {
        if (!okay)
            return;

        char16_t *name16 = nullptr;
        if (wantNames) {
            // Ask the tracer to compute an edge name for us.
            char buffer[1024];
            const char *name = getTracingEdgeName(buffer, sizeof(buffer));

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
        if (!vec->append(mozilla::Move(SimpleEdge(name16, Node(kind, *thingp))))) {
            okay = false;
            return;
        }
    }

  public:
    // True if no errors (OOM, say) have yet occurred.
    bool okay;

    SimpleEdgeVectorTracer(JSContext *cx, SimpleEdgeVector *vec, bool wantNames)
      : JSTracer(JS_GetRuntime(cx), staticCallback),
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
    explicit SimpleEdgeRange(JSContext *cx) : edges(cx), i(0) { }

    bool init(JSContext *cx, void *thing, JSGCTraceKind kind, bool wantNames = true) {
        SimpleEdgeVectorTracer tracer(cx, &edges, wantNames);
        JS_TraceChildren(&tracer, thing, kind);
        settle();
        return tracer.okay;
    }

    void popFront() MOZ_OVERRIDE { i++; settle(); }
};


template<typename Referent>
JS::Zone *
TracerConcrete<Referent>::zone() const
{
    return get().zone();
}

template<typename Referent>
EdgeRange *
TracerConcrete<Referent>::edges(JSContext *cx, bool wantNames) const {
    js::ScopedJSDeletePtr<SimpleEdgeRange> r(js_new<SimpleEdgeRange>(cx));
    if (!r)
        return nullptr;

    if (!r->init(cx, ptr, ::js::gc::MapTypeToTraceKind<Referent>::kind, wantNames))
        return nullptr;

    return r.forget();
}

template<typename Referent>
JSCompartment *
TracerConcreteWithCompartment<Referent>::compartment() const
{
    return TracerBase::get().compartment();
}

template<> const char16_t TracerConcrete<JSObject>::concreteTypeName[] =
    MOZ_UTF16("JSObject");
template<> const char16_t TracerConcrete<JSString>::concreteTypeName[] =
    MOZ_UTF16("JSString");
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
template<> const char16_t TracerConcrete<js::types::TypeObject>::concreteTypeName[] =
    MOZ_UTF16("js::types::TypeObject");


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
template class TracerConcrete<js::types::TypeObject>;
}
}
