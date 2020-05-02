/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_LoadedScript_h
#define mozilla_dom_LoadedScript_h

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "jsapi.h"
#include "ScriptLoadRequest.h"

class nsIURI;

namespace mozilla {
namespace dom {

class ScriptLoader;

void HostAddRefTopLevelScript(const JS::Value& aPrivate);
void HostReleaseTopLevelScript(const JS::Value& aPrivate);

class ClassicScript;
class ModuleScript;

class LoadedScript : public nsISupports {
  ScriptKind mKind;
  RefPtr<ScriptFetchOptions> mFetchOptions;
  nsCOMPtr<nsIURI> mBaseURL;

 protected:
  LoadedScript(ScriptKind aKind, ScriptFetchOptions* aFetchOptions,
               nsIURI* aBaseURL);

  virtual ~LoadedScript();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(LoadedScript)

  bool IsModuleScript() const { return mKind == ScriptKind::eModule; }

  inline ClassicScript* AsClassicScript();
  inline ModuleScript* AsModuleScript();

  ScriptFetchOptions* FetchOptions() const { return mFetchOptions; }
  nsIURI* BaseURL() const { return mBaseURL; }

  void AssociateWithScript(JSScript* aScript);
};

class ClassicScript final : public LoadedScript {
  ~ClassicScript() = default;

 public:
  ClassicScript(ScriptFetchOptions* aFetchOptions, nsIURI* aBaseURL);
};

// A single module script. May be used to satisfy multiple load requests.

class ModuleScript final : public LoadedScript {
  JS::Heap<JSObject*> mModuleRecord;
  JS::Heap<JS::Value> mParseError;
  JS::Heap<JS::Value> mErrorToRethrow;
  bool mDebuggerDataInitialized;

  ~ModuleScript();

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ModuleScript,
                                                         LoadedScript)

  ModuleScript(ScriptFetchOptions* aFetchOptions, nsIURI* aBaseURL);

  void SetModuleRecord(JS::Handle<JSObject*> aModuleRecord);
  void SetParseError(const JS::Value& aError);
  void SetErrorToRethrow(const JS::Value& aError);
  void SetDebuggerDataInitialized();

  JSObject* ModuleRecord() const { return mModuleRecord; }

  JS::Value ParseError() const { return mParseError; }
  JS::Value ErrorToRethrow() const { return mErrorToRethrow; }
  bool HasParseError() const { return !mParseError.isUndefined(); }
  bool HasErrorToRethrow() const { return !mErrorToRethrow.isUndefined(); }
  bool DebuggerDataInitialized() const { return mDebuggerDataInitialized; }

  void UnlinkModuleRecord();

  friend void CheckModuleScriptPrivate(LoadedScript*, const JS::Value&);
};

ClassicScript* LoadedScript::AsClassicScript() {
  MOZ_ASSERT(!IsModuleScript());
  return static_cast<ClassicScript*>(this);
}

ModuleScript* LoadedScript::AsModuleScript() {
  MOZ_ASSERT(IsModuleScript());
  return static_cast<ModuleScript*>(this);
}

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_LoadedScript_h
