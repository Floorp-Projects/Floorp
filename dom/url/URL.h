/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_URL_h
#define mozilla_dom_URL_h

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/URLSearchParams.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "nsWrapperCache.h"

class nsISupports;
class nsIURI;

namespace mozilla {

class ErrorResult;
class DOMMediaStream;

namespace dom {

class Blob;
class MediaSource;
class GlobalObject;

class URL : public URLSearchParamsObserver
          , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(URL)

  URL(nsISupports* aParent)
    : mParent(aParent)
  {}

  // WebIDL methods
  nsISupports* GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<URL>
  Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
              const Optional<nsAString>& aBase, ErrorResult& aRv);

  // Helper for Fetch API
  static already_AddRefed<URL>
  WorkerConstructor(const GlobalObject& aGlobal, const nsAString& aURL,
                    const nsAString& aBase, ErrorResult& aRv);


  static void
  CreateObjectURL(const GlobalObject& aGlobal, Blob& aBlob,
                  nsAString& aResult, ErrorResult& aRv);

  static void
  CreateObjectURL(const GlobalObject& aGlobal, DOMMediaStream& aStream,
                  nsAString& aResult, ErrorResult& aRv);

  static void
  CreateObjectURL(const GlobalObject& aGlobal, MediaSource& aSource,
                  nsAString& aResult, ErrorResult& aRv);

  static void
  RevokeObjectURL(const GlobalObject& aGlobal, const nsAString& aURL,
                  ErrorResult& aRv);

  static bool
  IsValidURL(const GlobalObject& aGlobal, const nsAString& aURL,
             ErrorResult& aRv);

  virtual void
  GetHref(nsAString& aHref) const = 0;

  virtual void
  SetHref(const nsAString& aHref, ErrorResult& aRv) = 0;

  virtual void
  GetOrigin(nsAString& aOrigin, ErrorResult& aRv) const = 0;

  virtual void
  GetProtocol(nsAString& aProtocol) const = 0;

  virtual void
  SetProtocol(const nsAString& aProtocol, ErrorResult& aRv) = 0;

  virtual void
  GetUsername(nsAString& aUsername) const = 0;

  virtual void
  SetUsername(const nsAString& aUsername) = 0;

  virtual void
  GetPassword(nsAString& aPassword) const = 0;

  virtual void
  SetPassword(const nsAString& aPassword) = 0;

  virtual void
  GetHost(nsAString& aHost) const = 0;

  virtual void
  SetHost(const nsAString& aHost) = 0;

  virtual void
  GetHostname(nsAString& aHostname) const = 0;

  virtual void
  SetHostname(const nsAString& aHostname) = 0;

  virtual void
  GetPort(nsAString& aPort) const = 0;

  virtual void
  SetPort(const nsAString& aPort) = 0;

  virtual void
  GetPathname(nsAString& aPathname) const = 0;

  virtual void
  SetPathname(const nsAString& aPathname) = 0;

  virtual void
  GetSearch(nsAString& aSearch) const = 0;

  virtual void
  SetSearch(const nsAString& aSearch);

  URLSearchParams* SearchParams();

  virtual void
  GetHash(nsAString& aHost) const = 0;

  virtual void
  SetHash(const nsAString& aHash) = 0;

  void Stringify(nsAString& aRetval) const
  {
    GetHref(aRetval);
  }

  void
  ToJSON(nsAString& aResult) const
  {
    GetHref(aResult);
  }

  // URLSearchParamsObserver
  void
  URLSearchParamsUpdated(URLSearchParams* aSearchParams) override;

protected:
  virtual ~URL()
  {}

  virtual void
  UpdateURLSearchParams() = 0;

  virtual void
  SetSearchInternal(const nsAString& aSearch) = 0;

  void CreateSearchParamsIfNeeded();

  nsCOMPtr<nsISupports> mParent;
  RefPtr<URLSearchParams> mSearchParams;
};

bool IsChromeURI(nsIURI* aURI);

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_URL_h */
