/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSWindowActorProtocol_h
#define mozilla_dom_JSWindowActorProtocol_h

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/ErrorResult.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIObserver.h"
#include "nsIDOMEventListener.h"
#include "mozilla/extensions/WebExtensionContentScript.h"

namespace mozilla {
namespace dom {

struct WindowActorOptions;
class JSWindowActorInfo;
class EventTarget;

/**
 * Object corresponding to a single window actor protocol. This object acts as
 * an Event listener for the actor which is called for events which would
 * trigger actor creation.
 *
 * This object also can act as a carrier for methods and other state related to
 * a single protocol managed by the JSActorService.
 */
class JSWindowActorProtocol final : public nsIObserver,
                                    public nsIDOMEventListener {
 public:
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(JSWindowActorProtocol, nsIObserver)

  static already_AddRefed<JSWindowActorProtocol> FromIPC(
      const JSWindowActorInfo& aInfo);
  JSWindowActorInfo ToIPC();

  static already_AddRefed<JSWindowActorProtocol> FromWebIDLOptions(
      const nsACString& aName, const WindowActorOptions& aOptions,
      ErrorResult& aRv);

  struct Sided {
    Maybe<nsCString> mModuleURI;
  };

  struct ParentSide : public Sided {};

  struct EventDecl {
    nsString mName;
    EventListenerFlags mFlags;
    Optional<bool> mPassive;
  };

  struct ChildSide : public Sided {
    nsTArray<EventDecl> mEvents;
    nsTArray<nsCString> mObservers;
  };

  const ParentSide& Parent() const { return mParent; }
  const ChildSide& Child() const { return mChild; }

  void RegisterListenersFor(EventTarget* aTarget);
  void UnregisterListenersFor(EventTarget* aTarget);
  void AddObservers();
  void RemoveObservers();
  bool Matches(BrowsingContext* aBrowsingContext, nsIURI* aURI,
               const nsAString& aRemoteType);

 private:
  explicit JSWindowActorProtocol(const nsACString& aName) : mName(aName) {}
  extensions::MatchPatternSet* GetURIMatcher();
  bool RemoteTypePrefixMatches(const nsDependentSubstring& aRemoteType);
  bool MessageManagerGroupMatches(BrowsingContext* aBrowsingContext);
  ~JSWindowActorProtocol() = default;

  nsCString mName;
  bool mAllFrames = false;
  bool mIncludeChrome = false;
  nsTArray<nsString> mMatches;
  nsTArray<nsString> mRemoteTypes;
  nsTArray<nsString> mMessageManagerGroups;

  ParentSide mParent;
  ChildSide mChild;

  RefPtr<extensions::MatchPatternSet> mURIMatcher;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSWindowActorProtocol_h
