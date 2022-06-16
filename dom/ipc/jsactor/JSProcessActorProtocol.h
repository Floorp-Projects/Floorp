/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSProcessActorProtocol_h
#define mozilla_dom_JSProcessActorProtocol_h

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/JSActorService.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIObserver.h"

namespace mozilla {
class ErrorResult;

namespace dom {

struct ProcessActorOptions;
class JSProcessActorInfo;
class EventTarget;
class JSActorProtocolUtils;

/**
 * Object corresponding to a single process actor protocol
 *
 * This object also can act as a carrier for methods and other state related to
 * a single protocol managed by the JSActorService.
 */
class JSProcessActorProtocol final : public JSActorProtocol,
                                     public nsIObserver {
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

  struct ParentSide : public Sided {};

  struct ChildSide : public Sided {
    nsTArray<nsCString> mObservers;
  };

  const ParentSide& Parent() const override { return mParent; }
  const ChildSide& Child() const override { return mChild; }

  void AddObservers();
  void RemoveObservers();
  bool Matches(const nsACString& aRemoteType, ErrorResult& aRv);

 private:
  explicit JSProcessActorProtocol(const nsACString& aName) : mName(aName) {}
  bool RemoteTypePrefixMatches(const nsDependentCSubstring& aRemoteType);
  ~JSProcessActorProtocol() = default;

  nsCString mName;
  nsTArray<nsCString> mRemoteTypes;
  bool mIncludeParent = false;

  friend class JSActorProtocolUtils;

  ParentSide mParent;
  ChildSide mChild;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSProcessActorProtocol_h
