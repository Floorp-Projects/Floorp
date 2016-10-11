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
#include "nsIDOMLocation.h"
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

class Location final : public nsIDOMLocation
                     , public nsWrapperCache
{
public:
  Location(nsPIDOMWindowInner* aWindow, nsIDocShell *aDocShell);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Location,
                                                         nsIDOMLocation)

  void SetDocShell(nsIDocShell *aDocShell);
  nsIDocShell *GetDocShell();

  // nsIDOMLocation
  NS_DECL_NSIDOMLOCATION

  #define THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME() { \
    if (!CallerSubsumes(&aSubjectPrincipal)) { \
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR); \
      return; \
    } \
  }

  // WebIDL API:
  void Assign(const nsAString& aUrl,
              nsIPrincipal& aSubjectPrincipal,
              ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = Assign(aUrl);
  }

  void Replace(const nsAString& aUrl,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError)
  {
    aError = Replace(aUrl);
  }

  void Reload(bool aForceget,
              nsIPrincipal& aSubjectPrincipal,
              ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = Reload(aForceget);
  }

  void GetHref(nsAString& aHref,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetHref(aHref);
  }

  void SetHref(const nsAString& aHref,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError)
  {
    aError = SetHref(aHref);
  }

  void GetOrigin(nsAString& aOrigin,
                 nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetOrigin(aOrigin);
  }

  void GetProtocol(nsAString& aProtocol,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetProtocol(aProtocol);
  }

  void SetProtocol(const nsAString& aProtocol,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetProtocol(aProtocol);
  }

  void GetHost(nsAString& aHost,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetHost(aHost);
  }

  void SetHost(const nsAString& aHost,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetHost(aHost);
  }

  void GetHostname(nsAString& aHostname,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetHostname(aHostname);
  }

  void SetHostname(const nsAString& aHostname,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetHostname(aHostname);
  }

  void GetPort(nsAString& aPort,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetPort(aPort);
  }

  void SetPort(const nsAString& aPort,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetPort(aPort);
  }

  void GetPathname(nsAString& aPathname,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetPathname(aPathname);
  }

  void SetPathname(const nsAString& aPathname,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetPathname(aPathname);
  }

  void GetSearch(nsAString& aSeach,
                 nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetSearch(aSeach);
  }

  void SetSearch(const nsAString& aSeach,
                 nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetSearch(aSeach);
  }

  void GetHash(nsAString& aHash,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetHash(aHash);
  }

  void SetHash(const nsAString& aHash,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetHash(aHash);
  }

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

protected:
  virtual ~Location();

  nsresult SetSearchInternal(const nsAString& aSearch);

  // In the case of jar: uris, we sometimes want the place the jar was
  // fetched from as the URI instead of the jar: uri itself.  Pass in
  // true for aGetInnermostURI when that's the case.
  nsresult GetURI(nsIURI** aURL, bool aGetInnermostURI = false);
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
