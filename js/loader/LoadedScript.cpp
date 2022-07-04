/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoadedScript.h"

#include "mozilla/HoldDropJSObjects.h"

#include "jsfriendapi.h"
#include "js/Modules.h"  // JS::{Get,Set}ModulePrivate

namespace JS::loader {

//////////////////////////////////////////////////////////////
// LoadedScript
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LoadedScript)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(LoadedScript)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(LoadedScript)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFetchOptions)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBaseURL)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(LoadedScript)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFetchOptions)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(LoadedScript)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(LoadedScript)
NS_IMPL_CYCLE_COLLECTING_RELEASE(LoadedScript)

LoadedScript::LoadedScript(ScriptKind aKind, ScriptFetchOptions* aFetchOptions,
                           nsIURI* aBaseURL)
    : mKind(aKind), mFetchOptions(aFetchOptions), mBaseURL(aBaseURL) {
  MOZ_ASSERT(mFetchOptions);
  MOZ_ASSERT(mBaseURL);
}

LoadedScript::~LoadedScript() { mozilla::DropJSObjects(this); }

void LoadedScript::AssociateWithScript(JSScript* aScript) {
  // Set a JSScript's private value to point to this object. The JS engine will
  // increment our reference count by calling HostAddRefTopLevelScript(). This
  // is decremented by HostReleaseTopLevelScript() below when the JSScript dies.

  MOZ_ASSERT(JS::GetScriptPrivate(aScript).isUndefined());
  JS::SetScriptPrivate(aScript, JS::PrivateValue(this));
}

inline void CheckModuleScriptPrivate(LoadedScript* script,
                                     const JS::Value& aPrivate) {
#ifdef DEBUG
  if (script->IsModuleScript()) {
    JSObject* module = script->AsModuleScript()->mModuleRecord.unbarrieredGet();
    MOZ_ASSERT_IF(module, JS::GetModulePrivate(module) == aPrivate);
  }
#endif
}

void HostAddRefTopLevelScript(const JS::Value& aPrivate) {
  // Increment the reference count of a LoadedScript object that is now pointed
  // to by a JSScript. The reference count is decremented by
  // HostReleaseTopLevelScript() below.

  auto script = static_cast<LoadedScript*>(aPrivate.toPrivate());
  CheckModuleScriptPrivate(script, aPrivate);
  script->AddRef();
}

void HostReleaseTopLevelScript(const JS::Value& aPrivate) {
  // Decrement the reference count of a LoadedScript object that was pointed to
  // by a JSScript. The reference count was originally incremented by
  // HostAddRefTopLevelScript() above.

  auto script = static_cast<LoadedScript*>(aPrivate.toPrivate());
  CheckModuleScriptPrivate(script, aPrivate);
  script->Release();
}

//////////////////////////////////////////////////////////////
// EventScript
//////////////////////////////////////////////////////////////

EventScript::EventScript(ScriptFetchOptions* aFetchOptions, nsIURI* aBaseURL)
    : LoadedScript(ScriptKind::eEvent, aFetchOptions, aBaseURL) {}

//////////////////////////////////////////////////////////////
// ClassicScript
//////////////////////////////////////////////////////////////

ClassicScript::ClassicScript(ScriptFetchOptions* aFetchOptions,
                             nsIURI* aBaseURL)
    : LoadedScript(ScriptKind::eClassic, aFetchOptions, aBaseURL) {}

//////////////////////////////////////////////////////////////
// ModuleScript
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ModuleScript)
NS_INTERFACE_MAP_END_INHERITING(LoadedScript)

NS_IMPL_CYCLE_COLLECTION_CLASS(ModuleScript)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ModuleScript, LoadedScript)
  tmp->UnlinkModuleRecord();
  tmp->mParseError.setUndefined();
  tmp->mErrorToRethrow.setUndefined();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ModuleScript, LoadedScript)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ModuleScript, LoadedScript)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mModuleRecord)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mParseError)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mErrorToRethrow)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(ModuleScript, LoadedScript)
NS_IMPL_RELEASE_INHERITED(ModuleScript, LoadedScript)

ModuleScript::ModuleScript(ScriptFetchOptions* aFetchOptions, nsIURI* aBaseURL)
    : LoadedScript(ScriptKind::eModule, aFetchOptions, aBaseURL),
      mDebuggerDataInitialized(false) {
  MOZ_ASSERT(!ModuleRecord());
  MOZ_ASSERT(!HasParseError());
  MOZ_ASSERT(!HasErrorToRethrow());
}

void ModuleScript::Shutdown() {
  if (mModuleRecord) {
    JS::ClearModuleEnvironment(mModuleRecord);
  }

  UnlinkModuleRecord();
}

void ModuleScript::UnlinkModuleRecord() {
  // Remove the module record's pointer to this object if present and
  // decrement our reference count. The reference is added by
  // SetModuleRecord() below.
  if (mModuleRecord) {
    MOZ_ASSERT(JS::GetModulePrivate(mModuleRecord).toPrivate() == this);
    JS::SetModulePrivate(mModuleRecord, JS::UndefinedValue());
    mModuleRecord = nullptr;
  }
}

ModuleScript::~ModuleScript() {
  // The object may be destroyed without being unlinked first.
  UnlinkModuleRecord();
}

void ModuleScript::SetModuleRecord(JS::Handle<JSObject*> aModuleRecord) {
  MOZ_ASSERT(!mModuleRecord);
  MOZ_ASSERT_IF(IsModuleScript(), !AsModuleScript()->HasParseError());
  MOZ_ASSERT_IF(IsModuleScript(), !AsModuleScript()->HasErrorToRethrow());

  mModuleRecord = aModuleRecord;

  // Make module's host defined field point to this object. The JS engine will
  // increment our reference count by calling HostAddRefTopLevelScript(). This
  // is decremented when the field is cleared in UnlinkModuleRecord() above or
  // when the module record dies.
  MOZ_ASSERT(JS::GetModulePrivate(mModuleRecord).isUndefined());
  JS::SetModulePrivate(mModuleRecord, JS::PrivateValue(this));

  mozilla::HoldJSObjects(this);
}

void ModuleScript::SetParseError(const JS::Value& aError) {
  MOZ_ASSERT(!aError.isUndefined());
  MOZ_ASSERT(!HasParseError());
  MOZ_ASSERT(!HasErrorToRethrow());

  UnlinkModuleRecord();
  mParseError = aError;
  mozilla::HoldJSObjects(this);
}

void ModuleScript::SetErrorToRethrow(const JS::Value& aError) {
  MOZ_ASSERT(!aError.isUndefined());

  // This is only called after SetModuleRecord() or SetParseError() so we don't
  // need to call HoldJSObjects() here.
  MOZ_ASSERT(ModuleRecord() || HasParseError());

  mErrorToRethrow = aError;
}

void ModuleScript::SetDebuggerDataInitialized() {
  MOZ_ASSERT(ModuleRecord());
  MOZ_ASSERT(!mDebuggerDataInitialized);

  mDebuggerDataInitialized = true;
}

}  // namespace JS::loader
