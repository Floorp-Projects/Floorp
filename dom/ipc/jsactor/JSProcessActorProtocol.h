/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSProcessActorProtocol_h
#define mozilla_dom_JSProcessActorProtocol_h

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/ErrorResult.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIObserver.h"

namespace mozilla {
namespace dom {

struct ProcessActorOptions;
class JSProcessActorInfo;
class EventTarget;

/**
 * Object corresponding to a single process actor protocol
 *
 * This object also can act as a carrier for methods and other state related to
 * a single protocol managed by the JSActorService.
 */
class JSProcessActorProtocol final : public nsIObserver {
 public:
  NS_DECL_NSIOBSERVER
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(JSProcessActorProtocol, nsIObserver)

  static already_AddRefed<JSProcessActorProtocol> FromIPC(
      const JSProcessActorInfo& aInfo);
  JSProcessActorInfo ToIPC();

  static already_AddRefed<JSProcessActorProtocol> FromWebIDLOptions(
      const nsACString& aName, const ProcessActorOptions& aOptions,
      ErrorResult& aRv);

  struct Sided {
    Maybe<nsCString> mModuleURI;
  };

  struct ParentSide : public Sided {};

  struct ChildSide : public Sided {
    nsTArray<nsCString> mObservers;
  };

  const ParentSide& Parent() const { return mParent; }
  const ChildSide& Child() const { return mChild; }

  void AddObservers();
  void RemoveObservers();
  bool Matches(const nsAString& aRemoteType);

 private:
  explicit JSProcessActorProtocol(const nsACString& aName) : mName(aName) {}
  bool RemoteTypePrefixMatches(const nsDependentSubstring& aRemoteType);
  ~JSProcessActorProtocol() = default;

  nsCString mName;
  nsTArray<nsString> mRemoteTypes;

  ParentSide mParent;
  ChildSide mChild;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSProcessActorProtocol_h
