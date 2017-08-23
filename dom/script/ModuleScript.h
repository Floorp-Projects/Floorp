/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ModuleScript_h
#define mozilla_dom_ModuleScript_h

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "jsapi.h"

class nsIURI;

namespace mozilla {
namespace dom {

class ScriptLoader;

class ModuleScript final : public nsISupports
{
  RefPtr<ScriptLoader> mLoader;
  nsCOMPtr<nsIURI> mBaseURL;
  JS::Heap<JSObject*> mModuleRecord;
  JS::Heap<JS::Value> mError;

  ~ModuleScript();

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ModuleScript)

  ModuleScript(ScriptLoader* aLoader,
               nsIURI* aBaseURL);

  void SetModuleRecord(JS::Handle<JSObject*> aModuleRecord);
  void SetPreInstantiationError(const JS::Value& aError);

  ScriptLoader* Loader() const { return mLoader; }
  JSObject* ModuleRecord() const { return mModuleRecord; }
  nsIURI* BaseURL() const { return mBaseURL; }

  bool IsErrored() const;
  JS::Value Error() const;

  void UnlinkModuleRecord();
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_ModuleScript_h
