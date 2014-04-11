/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//////////////////////////////////////////////////////////////////////////////////////////

#ifndef nsXBLService_h_
#define nsXBLService_h_

#include "nsString.h"
#include "nsWeakReference.h"
#include "nsTArray.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"

class nsXBLBinding;
class nsXBLDocumentInfo;
class nsIContent;
class nsIDocument;
class nsString;
class nsIURI;
class nsIPrincipal;

namespace mozilla {
namespace dom {
class EventTarget;
}
}

class nsXBLService MOZ_FINAL : public nsSupportsWeakReference
{
  NS_DECL_ISUPPORTS

  static nsXBLService* gInstance;

  static void Init();

  static void Shutdown() {
    NS_IF_RELEASE(gInstance);
  }

  static nsXBLService* GetInstance() { return gInstance; }

  static bool IsChromeOrResourceURI(nsIURI* aURI);

  // This function loads a particular XBL file and installs all of the bindings
  // onto the element.  aOriginPrincipal must not be null here.
  nsresult LoadBindings(nsIContent* aContent, nsIURI* aURL,
                        nsIPrincipal* aOriginPrincipal,
                        nsXBLBinding** aBinding, bool* aResolveStyle);

  // Indicates whether or not a binding is fully loaded.
  nsresult BindingReady(nsIContent* aBoundElement, nsIURI* aURI, bool* aIsReady);

  // This method checks the hashtable and then calls FetchBindingDocument on a
  // miss.  aOriginPrincipal or aBoundDocument may be null to bypass security
  // checks.
  nsresult LoadBindingDocumentInfo(nsIContent* aBoundElement,
                                   nsIDocument* aBoundDocument,
                                   nsIURI* aBindingURI,
                                   nsIPrincipal* aOriginPrincipal,
                                   bool aForceSyncLoad,
                                   nsXBLDocumentInfo** aResult);

  // Used by XUL key bindings and for window XBL.
  static nsresult AttachGlobalKeyHandler(mozilla::dom::EventTarget* aTarget);
  static nsresult DetachGlobalKeyHandler(mozilla::dom::EventTarget* aTarget);

private:
  nsXBLService();
  virtual ~nsXBLService();

protected:
  // This function clears out the bindings on a given content node.
  nsresult FlushStyleBindings(nsIContent* aContent);

  // This method synchronously loads and parses an XBL file.
  nsresult FetchBindingDocument(nsIContent* aBoundElement, nsIDocument* aBoundDocument,
                                nsIURI* aDocumentURI, nsIURI* aBindingURI,
                                bool aForceSyncLoad, nsIDocument** aResult);

  /**
   * This method calls the one below with an empty |aDontExtendURIs| array.
   */
  nsresult GetBinding(nsIContent* aBoundElement, nsIURI* aURI,
                      bool aPeekFlag, nsIPrincipal* aOriginPrincipal,
                      bool* aIsReady, nsXBLBinding** aResult);

  /**
   * This method loads a binding doc and then builds the specific binding
   * required. It can also peek without building.
   * @param aBoundElement the element to get a binding for
   * @param aURI the binding URI
   * @param aPeekFlag if true then just peek to see if the binding is ready
   * @param aIsReady [out] if the binding is ready or not
   * @param aResult [out] where to store the resulting binding (not used if
   *                      aPeekFlag is true, otherwise it must be non-null)
   * @param aDontExtendURIs a set of URIs that are already bound to this
   *        element. If a binding extends any of these then further loading
   *        is aborted (because it would lead to the binding extending itself)
   *        and NS_ERROR_ILLEGAL_VALUE is returned.
   *
   * @note This method always calls LoadBindingDocumentInfo(), so it's
   *       enough to funnel all security checks through that function.
   */
  nsresult GetBinding(nsIContent* aBoundElement, nsIURI* aURI,
                      bool aPeekFlag, nsIPrincipal* aOriginPrincipal,
                      bool* aIsReady, nsXBLBinding** aResult,
                      nsTArray<nsIURI*>& aDontExtendURIs);

// MEMBER VARIABLES
public:
  static bool gDisableChromeCache;
  static bool     gAllowDataURIs;            // Whether we should allow data
                                             // urls in -moz-binding. Needed for
                                             // testing.
};

#endif
