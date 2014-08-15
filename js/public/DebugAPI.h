/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Interfaces by which the embedding can interact with the Debugger API.

#ifndef js_DebugAPI_h
#define js_DebugAPI_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Move.h"

#include "jspubtd.h"

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace js {
class Debugger;
}

namespace JS {
namespace dbg {

// Helping embedding code build objects for Debugger
// -------------------------------------------------
//
// Some Debugger API features lean on the embedding application to construct
// their result values. For example, Debugger.Frame.prototype.scriptEntryReason
// calls hooks provided by the embedding to construct values explaining why it
// invoked JavaScript; if F is a frame called from a mouse click event handler,
// F.scriptEntryReason would return an object of the form:
//
//   { eventType: "mousedown", event: <object> }
//
// where <object> is a Debugger.Object whose referent is the event being
// dispatched.
//
// However, Debugger implements a trust boundary. Debuggee code may be
// considered untrusted; debugger code needs to be protected from debuggee
// getters, setters, proxies, Object.watch watchpoints, and any other feature
// that might accidentally cause debugger code to set the debuggee running. The
// Debugger API tries to make it easy to write safe debugger code by only
// offering access to debuggee objects via Debugger.Object instances, which
// ensure that only those operations whose explicit purpose is to invoke
// debuggee code do so. But this protective membrane is only helpful if we
// interpose Debugger.Object instances in all the necessary spots.
//
// SpiderMonkey's compartment system also implements a trust boundary. The
// debuggee and debugger are always in different compartments. Inter-compartment
// work requires carefully tracking which compartment each JSObject or JS::Value
// belongs to, and ensuring that is is correctly wrapped for each operation.
//
// It seems precarious to expect the embedding's hooks to implement these trust
// boundaries. Instead, the JS::dbg::Builder API segregates the code which
// constructs trusted objects from that which deals with untrusted objects.
// Trusted objects have an entirely different C++ type, so code that improperly
// mixes trusted and untrusted objects is caught at compile time.
//
// In the structure shown above, there are two trusted objects, and one
// untrusted object:
//
// - The overall object, with the 'eventType' and 'event' properties, is a
//   trusted object. We're going to return it to D.F.p.scriptEntryReason's
//   caller, which will handle it directly.
//
// - The Debugger.Object instance appearing as the value of the 'event' property
//   is a trusted object. It belongs to the same Debugger instance as the
//   Debugger.Frame instance whose scriptEntryReason accessor was called, and
//   presents a safe reflection-oriented API for inspecting its referent, which
//   is:
//
// - The actual event object, an untrusted object, and the referent of the
//   Debugger.Object above. (Content can do things like replacing accessors on
//   Event.prototype.)
//
// Using JS::dbg::Builder, all objects and values the embedding deals with
// directly are considered untrusted, and are assumed to be debuggee values. The
// only way to construct trusted objects is to use Builder's own methods, which
// return a separate Object type. The only way to set a property on a trusted
// object is through that Object type. The actual trusted object is never
// exposed to the embedding.
//
// So, for example, the embedding might use code like the following to construct
// the object shown above, given a Builder passed to it by Debugger:
//
//    bool
//    MyScriptEntryReason::explain(JSContext *cx,
//                                 Builder &builder,
//                                 Builder::Object &result)
//    {
//        JSObject *eventObject = ... obtain debuggee event object somehow ...;
//        if (!eventObject)
//            return false;
//        result = builder.newObject(cx);
//        return result &&
//               result.defineProperty(cx, "eventType", SafelyFetchType(eventObject)) &&
//               result.defineProperty(cx, "event", eventObject);
//    }
//
//
// Object::defineProperty also accepts an Object as the value to store on the
// property. By its type, we know that the value is trusted, so we set it
// directly as the property's value, without interposing a Debugger.Object
// wrapper. This allows the embedding to builted nested structures of trusted
// objects.
//
// The Builder and Builder::Object methods take care of doing whatever
// compartment switching and wrapping are necessary to construct the trusted
// values in the Debugger's compartment.
//
// The Object type is self-rooting. Construction, assignment, and destruction
// all properly root the referent object.

class BuilderOrigin;

class Builder {
    // The Debugger instance whose client we are building a value for. We build
    // objects in this object's compartment.
    PersistentRootedObject debuggerObject;

    // debuggerObject's Debugger structure, for convenience.
    js::Debugger *debugger;

    // Check that |thing| is in the same compartment as our debuggerObject. Used
    // for assertions when constructing BuiltThings. We can overload this as we
    // add more instantiations of BuiltThing.
#if DEBUG
    void assertBuilt(JSObject *obj);
#else
    void assertBuilt(JSObject *obj) { }
#endif

  protected:
    // A reference to a trusted object or value. At the moment, we only use it
    // with JSObject *.
    template<typename T>
    class BuiltThing {
        friend class BuilderOrigin;

        void nonNull() {}

      protected:
        // The Builder to which this trusted thing belongs.
        Builder &owner;

        // A rooted reference to our value.
        PersistentRooted<T> value;

        BuiltThing(JSContext *cx, Builder &owner_, T value_ = js::GCMethods<T>::initial())
          : owner(owner_), value(cx, value_)
        {
            owner.assertBuilt(value_);
        }

        // Forward some things from our owner, for convenience.
        js::Debugger *debugger() const { return owner.debugger; }
        JSObject *debuggerObject() const { return owner.debuggerObject; }

      public:
        BuiltThing(const BuiltThing &rhs) : owner(rhs.owner), value(rhs.value) { }
        BuiltThing &operator=(const BuiltThing &rhs) {
            MOZ_ASSERT(&owner == &rhs.owner);
            owner.assertBuilt(rhs.value);
            value = rhs.value;
            return *this;
        }

        typedef void (BuiltThing::* ConvertibleToBool)();
        operator ConvertibleToBool() const {
            // If we ever instantiate BuiltThink<Value>, this might not suffice.
            return value ? &BuiltThing::nonNull : 0;
        }

      private:
        BuiltThing() MOZ_DELETE;
    };

  public:
    // A reference to a trusted object, possibly null. Instances of Object are
    // always properly rooted. They can be copied and assigned, as if they were
    // pointers.
    class Object: private BuiltThing<JSObject *> {
        friend class Builder;           // for construction
        friend class BuilderOrigin;     // for unwrapping

        typedef BuiltThing<JSObject *> Base;

        // This is private, because only Builders can create Objects that
        // actually point to something (hence the 'friend' declaration).
        Object(JSContext *cx, Builder &owner_, HandleObject obj) : Base(cx, owner_, obj.get()) { }

        bool definePropertyToTrusted(JSContext *cx, const char *name,
                                     JS::MutableHandleValue value);

      public:
        Object(JSContext *cx, Builder &owner_) : Base(cx, owner_, nullptr) { }
        Object(const Object &rhs) : Base(rhs) { }

        // Our automatically-generated assignment operator can see our base
        // class's assignment operator, so we don't need to write one out here.

        // Set the property named |name| on this object to |value|.
        //
        // If |value| is a string or primitive, re-wrap it for the debugger's
        // compartment.
        //
        // If |value| is an object, assume it is a debuggee object and make a
        // Debugger.Object instance referring to it. Set that as the propery's
        // value.
        //
        // If |value| is another trusted object, store it directly as the
        // property's value.
        //
        // On error, report the problem on cx and return false.
        bool defineProperty(JSContext *cx, const char *name, JS::HandleValue value);
        bool defineProperty(JSContext *cx, const char *name, JS::HandleObject value);
        bool defineProperty(JSContext *cx, const char *name, Object &value);

        using Base::ConvertibleToBool;
        using Base::operator ConvertibleToBool;
    };

    // Build an empty object for direct use by debugger code, owned by this
    // Builder. If an error occurs, report it on cx and return a false Object.
    Object newObject(JSContext *cx);

  protected:
    Builder(JSContext *cx, js::Debugger *debugger);
};

// Debugger itself instantiates this subclass of Builder, which can unwrap
// BuiltThings that belong to it.
class BuilderOrigin : public Builder {
    template<typename T>
    T unwrapAny(const BuiltThing<T> &thing) {
        MOZ_ASSERT(&thing.owner == this);
        return thing.value.get();
    }

  public:
    BuilderOrigin(JSContext *cx, js::Debugger *debugger_)
      : Builder(cx, debugger_)
    { }

    JSObject *unwrap(Object &object) { return unwrapAny(object); }
};

} // namespace dbg
} // namespace JS


#endif /* js_DebugAPI_h */
