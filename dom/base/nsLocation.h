/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLocation_h__
#define nsLocation_h__

#include "nsIDOMLocation.h"
#include "nsString.h"
#include "nsIWeakReferenceUtils.h"
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "js/TypeDecls.h"
#include "mozilla/ErrorResult.h"
#include "nsPIDOMWindow.h"

class nsIURI;
class nsIDocShell;
class nsIDocShellLoadInfo;

//*****************************************************************************
// nsLocation: Script "location" object
//*****************************************************************************

class nsLocation final : public nsIDOMLocation
                       , public nsWrapperCache
{
  typedef mozilla::ErrorResult ErrorResult;

public:
  nsLocation(nsPIDOMWindowInner* aWindow, nsIDocShell *aDocShell);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsLocation,
                                                         nsIDOMLocation)

  void SetDocShell(nsIDocShell *aDocShell);
  nsIDocShell *GetDocShell();

  // nsIDOMLocation
  NS_DECL_NSIDOMLOCATION

  #define THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME() { \
    if (!CallerSubsumes()) { \
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR); \
      return; \
    } \
  }

  // WebIDL API:
  void Assign(const nsAString& aUrl, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = Assign(aUrl);
  }

  void Replace(const nsAString& aUrl, ErrorResult& aError)
  {
    aError = Replace(aUrl);
  }

  void Reload(bool aForceget, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = Reload(aForceget);
  }
  void GetHref(nsAString& aHref, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetHref(aHref);
  }
  void SetHref(const nsAString& aHref, ErrorResult& aError)
  {
    aError = SetHref(aHref);
  }
  void GetOrigin(nsAString& aOrigin, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetOrigin(aOrigin);
  }
  void GetProtocol(nsAString& aProtocol, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetProtocol(aProtocol);
  }
  void SetProtocol(const nsAString& aProtocol, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetProtocol(aProtocol);
  }
  void GetUsername(nsAString& aUsername, ErrorResult& aError);
  void SetUsername(const nsAString& aUsername, ErrorResult& aError);
  void GetPassword(nsAString& aPassword, ErrorResult& aError);
  void SetPassword(const nsAString& aPassword, ErrorResult& aError);
  void GetHost(nsAString& aHost, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetHost(aHost);
  }
  void SetHost(const nsAString& aHost, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetHost(aHost);
  }
  void GetHostname(nsAString& aHostname, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetHostname(aHostname);
  }
  void SetHostname(const nsAString& aHostname, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetHostname(aHostname);
  }
  void GetPort(nsAString& aPort, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetPort(aPort);
  }
  void SetPort(const nsAString& aPort, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetPort(aPort);
  }
  void GetPathname(nsAString& aPathname, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetPathname(aPathname);
  }
  void SetPathname(const nsAString& aPathname, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetPathname(aPathname);
  }
  void GetSearch(nsAString& aSeach, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetSearch(aSeach);
  }
  void SetSearch(const nsAString& aSeach, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetSearch(aSeach);
  }

  void GetHash(nsAString& aHash, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = GetHash(aHash);
  }
  void SetHash(const nsAString& aHash, ErrorResult& aError)
  {
    THROW_AND_RETURN_IF_CALLER_DOESNT_SUBSUME();
    aError = SetHash(aHash);
  }
  void Stringify(nsAString& aRetval, ErrorResult& aError)
  {
    // GetHref checks CallerSubsumes.
    GetHref(aRetval, aError);
  }
  nsPIDOMWindowInner* GetParentObject() const
  {
    return mInnerWindow;
  }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

protected:
  virtual ~nsLocation();

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
  bool CallerSubsumes();

  nsString mCachedHash;
  nsCOMPtr<nsPIDOMWindowInner> mInnerWindow;
  nsWeakPtr mDocShell;
};

#endif // nsLocation_h__
