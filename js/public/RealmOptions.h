/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Options specified when creating a realm to determine its behavior, immutable
 * options determining the behavior of an existing realm, and mutable options on
 * an existing realm that may be changed when desired.
 */

#ifndef js_RealmOptions_h
#define js_RealmOptions_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include "jstypes.h"  // JS_PUBLIC_API

#include "js/Class.h"  // JSTraceOp

struct JSContext;
class JSObject;

namespace JS {

class Compartment;
class Realm;
class Zone;

}  // namespace JS

namespace JS {

/**
 * Specification for which compartment/zone a newly created realm should use.
 */
enum class CompartmentSpecifier {
  // Create a new realm and compartment in the single runtime wide system
  // zone. The meaning of this zone is left to the embedder.
  NewCompartmentInSystemZone,

  // Create a new realm and compartment in a particular existing zone.
  NewCompartmentInExistingZone,

  // Create a new zone/compartment.
  NewCompartmentAndZone,

  // Create a new realm in an existing compartment.
  ExistingCompartment,
};

/**
 * RealmCreationOptions specifies options relevant to creating a new realm, that
 * are either immutable characteristics of that realm or that are discarded
 * after the realm has been created.
 *
 * Access to these options on an existing realm is read-only: if you need
 * particular selections, you must make them before you create the realm.
 */
class JS_PUBLIC_API RealmCreationOptions {
 public:
  RealmCreationOptions() : comp_(nullptr) {}

  JSTraceOp getTrace() const { return traceGlobal_; }
  RealmCreationOptions& setTrace(JSTraceOp op) {
    traceGlobal_ = op;
    return *this;
  }

  Zone* zone() const {
    MOZ_ASSERT(compSpec_ == CompartmentSpecifier::NewCompartmentInExistingZone);
    return zone_;
  }
  Compartment* compartment() const {
    MOZ_ASSERT(compSpec_ == CompartmentSpecifier::ExistingCompartment);
    return comp_;
  }
  CompartmentSpecifier compartmentSpecifier() const { return compSpec_; }

  // Set the compartment/zone to use for the realm. See CompartmentSpecifier
  // above.
  RealmCreationOptions& setNewCompartmentInSystemZone();
  RealmCreationOptions& setNewCompartmentInExistingZone(JSObject* obj);
  RealmCreationOptions& setNewCompartmentAndZone();
  RealmCreationOptions& setExistingCompartment(JSObject* obj);
  RealmCreationOptions& setExistingCompartment(Compartment* compartment);

  // Certain compartments are implementation details of the embedding, and
  // references to them should never leak out to script. This flag causes this
  // realm to skip firing onNewGlobalObject and makes addDebuggee a no-op for
  // this global.
  //
  // Debugger visibility is per-compartment, not per-realm (it's only practical
  // to enforce visibility on compartment boundaries), so if a realm is being
  // created in an extant compartment, its requested visibility must match that
  // of the compartment.
  bool invisibleToDebugger() const { return invisibleToDebugger_; }
  RealmCreationOptions& setInvisibleToDebugger(bool flag) {
    invisibleToDebugger_ = flag;
    return *this;
  }

  // Realms used for off-thread compilation have their contents merged into a
  // target realm when the compilation is finished. This is only allowed if
  // this flag is set. The invisibleToDebugger flag must also be set for such
  // realms.
  bool mergeable() const { return mergeable_; }
  RealmCreationOptions& setMergeable(bool flag) {
    mergeable_ = flag;
    return *this;
  }

  // Determines whether this realm should preserve JIT code on non-shrinking
  // GCs.
  bool preserveJitCode() const { return preserveJitCode_; }
  RealmCreationOptions& setPreserveJitCode(bool flag) {
    preserveJitCode_ = flag;
    return *this;
  }

  bool cloneSingletons() const { return cloneSingletons_; }
  RealmCreationOptions& setCloneSingletons(bool flag) {
    cloneSingletons_ = flag;
    return *this;
  }

  bool getSharedMemoryAndAtomicsEnabled() const;
  RealmCreationOptions& setSharedMemoryAndAtomicsEnabled(bool flag);

  bool getStreamsEnabled() const { return streams_; }
  RealmCreationOptions& setStreamsEnabled(bool flag) {
    streams_ = flag;
    return *this;
  }

  bool getFieldsEnabled() const { return fields_; }
  RealmCreationOptions& setFieldsEnabled(bool flag) {
    fields_ = flag;
    return *this;
  }

  bool getAwaitFixEnabled() const { return awaitFix_; }
  RealmCreationOptions& setAwaitFixEnabled(bool flag) {
    awaitFix_ = flag;
    return *this;
  }

  // This flag doesn't affect JS engine behavior.  It is used by Gecko to
  // mark whether content windows and workers are "Secure Context"s. See
  // https://w3c.github.io/webappsec-secure-contexts/
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1162772#c34
  bool secureContext() const { return secureContext_; }
  RealmCreationOptions& setSecureContext(bool flag) {
    secureContext_ = flag;
    return *this;
  }

  bool clampAndJitterTime() const { return clampAndJitterTime_; }
  RealmCreationOptions& setClampAndJitterTime(bool flag) {
    clampAndJitterTime_ = flag;
    return *this;
  }

 private:
  JSTraceOp traceGlobal_ = nullptr;
  CompartmentSpecifier compSpec_ = CompartmentSpecifier::NewCompartmentAndZone;
  union {
    Compartment* comp_;
    Zone* zone_;
  };
  bool invisibleToDebugger_ = false;
  bool mergeable_ = false;
  bool preserveJitCode_ = false;
  bool cloneSingletons_ = false;
  bool sharedMemoryAndAtomics_ = false;
  bool streams_ = false;
  bool fields_ = false;
  bool awaitFix_ = false;
  bool secureContext_ = false;
  bool clampAndJitterTime_ = true;
};

/**
 * RealmBehaviors specifies behaviors of a realm that can be changed after the
 * realm's been created.
 */
class JS_PUBLIC_API RealmBehaviors {
 public:
  RealmBehaviors() = default;

  // For certain globals, we know enough about the code that will run in them
  // that we can discard script source entirely.
  bool discardSource() const { return discardSource_; }
  RealmBehaviors& setDiscardSource(bool flag) {
    discardSource_ = flag;
    return *this;
  }

  bool disableLazyParsing() const { return disableLazyParsing_; }
  RealmBehaviors& setDisableLazyParsing(bool flag) {
    disableLazyParsing_ = flag;
    return *this;
  }

  class Override {
   public:
    Override() : mode_(Default) {}

    bool get(bool defaultValue) const {
      if (mode_ == Default) {
        return defaultValue;
      }
      return mode_ == ForceTrue;
    }

    void set(bool overrideValue) {
      mode_ = overrideValue ? ForceTrue : ForceFalse;
    }

    void reset() { mode_ = Default; }

   private:
    enum Mode { Default, ForceTrue, ForceFalse };

    Mode mode_;
  };

  bool extraWarnings(JSContext* cx) const;
  Override& extraWarningsOverride() { return extraWarningsOverride_; }

  bool getSingletonsAsTemplates() const { return singletonsAsTemplates_; }
  RealmBehaviors& setSingletonsAsValues() {
    singletonsAsTemplates_ = false;
    return *this;
  }

  // A Realm can stop being "live" in all the ways that matter before its global
  // is actually GCed.  Consumers that tear down parts of a Realm or its global
  // before that point should set isNonLive accordingly.
  bool isNonLive() const { return isNonLive_; }
  RealmBehaviors& setNonLive() {
    isNonLive_ = true;
    return *this;
  }

 private:
  bool discardSource_ = false;
  bool disableLazyParsing_ = false;
  Override extraWarningsOverride_ = {};

  // To XDR singletons, we need to ensure that all singletons are all used as
  // templates, by making JSOP_OBJECT return a clone of the JSScript
  // singleton, instead of returning the value which is baked in the JSScript.
  bool singletonsAsTemplates_ = true;
  bool isNonLive_ = false;
};

/**
 * RealmOptions specifies realm characteristics: both those that can't be
 * changed on a realm once it's been created (RealmCreationOptions), and those
 * that can be changed on an existing realm (RealmBehaviors).
 */
class JS_PUBLIC_API RealmOptions {
 public:
  explicit RealmOptions() : creationOptions_(), behaviors_() {}

  RealmOptions(const RealmCreationOptions& realmCreation,
               const RealmBehaviors& realmBehaviors)
      : creationOptions_(realmCreation), behaviors_(realmBehaviors) {}

  // RealmCreationOptions specify fundamental realm characteristics that must
  // be specified when the realm is created, that can't be changed after the
  // realm is created.
  RealmCreationOptions& creationOptions() { return creationOptions_; }
  const RealmCreationOptions& creationOptions() const {
    return creationOptions_;
  }

  // RealmBehaviors specify realm characteristics that can be changed after
  // the realm is created.
  RealmBehaviors& behaviors() { return behaviors_; }
  const RealmBehaviors& behaviors() const { return behaviors_; }

 private:
  RealmCreationOptions creationOptions_;
  RealmBehaviors behaviors_;
};

extern JS_PUBLIC_API const RealmCreationOptions& RealmCreationOptionsRef(
    Realm* realm);

extern JS_PUBLIC_API const RealmCreationOptions& RealmCreationOptionsRef(
    JSContext* cx);

extern JS_PUBLIC_API RealmBehaviors& RealmBehaviorsRef(Realm* realm);

extern JS_PUBLIC_API RealmBehaviors& RealmBehaviorsRef(JSContext* cx);

}  // namespace JS

#endif  // js_RealmOptions_h
