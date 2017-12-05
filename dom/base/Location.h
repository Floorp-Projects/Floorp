/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Location_h
#define mozilla_dom_Location_h

#include "js/TypeDecls.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIWeakReferenceUtils.h"
#include "nsPIDOMWindow.h"
#include "nsString.h"
#include "nsWrapperCache.h"

class nsIDocShell;
class nsIDocShellLoadInfo;
class nsIURI;

namespace mozilla {
namespace dom {

//*****************************************************************************
// Location: Script "location" object
//*****************************************************************************

class Location final : public nsISupports
                     , public nsWrapperCache
{
public:
  Location(nsPIDOMWindowInner* aWindow, nsIDocShell *aDocShell);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Location)

  // WebIDL API:
  void Assign(const nsAString& aUrl,
              nsIPrincipal& aSubjectPrincipal,
              ErrorResult& aError);

  void Replace(const nsAString& aUrl,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void Reload(bool aForceget,
              nsIPrincipal& aSubjectPrincipal,
              ErrorResult& aError)
  {
    if (!CallerSubsumes(&aSubjectPrincipal)) {
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }

    aError = Reload(aForceget);
  }

  void GetHref(nsAString& aHref,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError)
  {
    if (!CallerSubsumes(&aSubjectPrincipal)) {
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }

    aError = GetHref(aHref);
  }

  void SetHref(const nsAString& aHref,
               ErrorResult& aError);

  void GetOrigin(nsAString& aOrigin,
                 nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aError);

  void GetProtocol(nsAString& aProtocol,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError);

  void SetProtocol(const nsAString& aProtocol,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError);

  void GetHost(nsAString& aHost,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void SetHost(const nsAString& aHost,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void GetHostname(nsAString& aHostname,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError);

  void SetHostname(const nsAString& aHostname,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError);

  void GetPort(nsAString& aPort,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void SetPort(const nsAString& aPort,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void GetPathname(nsAString& aPathname,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError);

  void SetPathname(const nsAString& aPathname,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError);

  void GetSearch(nsAString& aSeach,
                 nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aError);

  void SetSearch(const nsAString& aSeach,
                 nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aError);

  void GetHash(nsAString& aHash,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void SetHash(const nsAString& aHash,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void Stringify(nsAString& aRetval,
                 nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aError)
  {
    // GetHref checks CallerSubsumes.
    GetHref(aRetval, aSubjectPrincipal, aError);
  }

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mInnerWindow;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // Non WebIDL methods:

  nsresult GetHref(nsAString& aHref);

  nsresult ToString(nsAString& aString)
  {
    return GetHref(aString);
  }

  nsresult Reload(bool aForceget);

protected:
  virtual ~Location();

  // In the case of jar: uris, we sometimes want the place the jar was
  // fetched from as the URI instead of the jar: uri itself.  Pass in
  // true for aGetInnermostURI when that's the case.
  // Note, this method can return NS_OK with a null value for aURL. This happens
  // if the docShell is null.
  nsresult GetURI(nsIURI** aURL, bool aGetInnermostURI = false);
  // Note, this method can return NS_OK with a null value for aURL. This happens
  // if the docShell is null.
  nsresult GetWritableURI(nsIURI** aURL,
                          // If not null, give it the new ref
                          const nsACString* aNewRef = nullptr);
  nsresult SetURI(nsIURI* aURL, bool aReplace = false);
  nsresult SetHrefWithBase(const nsAString& aHref, nsIURI* aBase,
                           bool aReplace);
  nsresult SetHrefWithContext(JSContext* cx, const nsAString& aHref,
                              bool aReplace);

  nsresult GetSourceBaseURL(JSContext* cx, nsIURI** sourceURL);
  nsresult CheckURL(nsIURI *url, nsIDocShellLoadInfo** aLoadInfo);
  bool CallerSubsumes(nsIPrincipal* aSubjectPrincipal);

  nsString mCachedHash;
  nsCOMPtr<nsPIDOMWindowInner> mInnerWindow;
  nsWeakPtr mDocShell;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_Location_h
