/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PrecompiledScript_h
#define mozilla_dom_PrecompiledScript_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/PrecompiledScriptBinding.h"

#include "jsapi.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {
class PrecompiledScript : public nsISupports
                        , public nsWrapperCache
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(PrecompiledScript)

  explicit PrecompiledScript(nsISupports* aParent, JS::Handle<JSScript*> aScript, JS::ReadOnlyCompileOptions& aOptions);

  void ExecuteInGlobal(JSContext* aCx, JS::HandleObject aGlobal,
                       JS::MutableHandleValue aRval,
                       ErrorResult& aRv);

  void GetUrl(nsAString& aUrl);

  bool HasReturnValue();

  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

protected:
  virtual ~PrecompiledScript();

private:
  bool IsBlackForCC(bool aTracingNeeded);

  nsCOMPtr<nsISupports> mParent;

  JS::Heap<JSScript*> mScript;
  nsCString mURL;
  const bool mHasReturnValue;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PrecompiledScript_h
