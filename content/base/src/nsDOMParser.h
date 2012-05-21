/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMParser_h__
#define nsDOMParser_h__

#include "nsIDOMParser.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsWeakReference.h"
#include "nsIJSNativeInitializer.h"
#include "nsIDocument.h"

class nsDOMParser : public nsIDOMParser,
                    public nsIDOMParserJS,
                    public nsIJSNativeInitializer,
                    public nsSupportsWeakReference
{
public: 
  nsDOMParser();
  virtual ~nsDOMParser();

  NS_DECL_ISUPPORTS

  // nsIDOMParser
  NS_DECL_NSIDOMPARSER

  // nsIDOMParserJS
  NS_DECL_NSIDOMPARSERJS

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* cx, JSObject* obj,
                        PRUint32 argc, jsval *argv);

private:
  nsresult SetUpDocument(DocumentFlavor aFlavor, nsIDOMDocument** aResult);

  class AttemptedInitMarker {
  public:
    AttemptedInitMarker(bool* aAttemptedInit) :
      mAttemptedInit(aAttemptedInit)
    {}

    ~AttemptedInitMarker() {
      *mAttemptedInit = true;
    }

  private:
    bool* mAttemptedInit;
  };
  
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIPrincipal> mOriginalPrincipal;
  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mBaseURI;
  nsWeakPtr mScriptHandlingObject;
  
  bool mLoopingForSyncLoad;
  bool mAttemptedInit;
};

#endif
