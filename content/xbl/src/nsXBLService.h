/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//////////////////////////////////////////////////////////////////////////////////////////

#ifndef nsXBLService_h_
#define nsXBLService_h_

#include "mozilla/LinkedList.h"
#include "nsString.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "js/Class.h"           // nsXBLJSClass derives from JSClass
#include "nsTArray.h"

class nsCStringKey;
class nsXBLBinding;
class nsXBLDocumentInfo;
class nsXBLJSClass;
class nsIContent;
class nsIDocument;
class nsString;
class nsIURI;
class nsIPrincipal;
class nsSupportsHashtable;
class nsHashtable;

namespace mozilla {
namespace dom {
class EventTarget;
}
}

class nsXBLService : public nsIObserver,
                     public nsSupportsWeakReference
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

  NS_DECL_NSIOBSERVER

private:
  nsXBLService();
  virtual ~nsXBLService();

protected:
  // This function clears out the bindings on a given content node.
  nsresult FlushStyleBindings(nsIContent* aContent);

  // Release any memory that we can
  nsresult FlushMemory();
  
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

  static nsHashtable* gClassTable;           // A table of nsXBLJSClass objects.

  static mozilla::LinkedList<nsXBLJSClass>* gClassLRUList;
                                             // LRU list of cached classes.
  static uint32_t gClassLRUListLength;       // Number of classes on LRU list.
  static uint32_t gClassLRUListQuota;        // Quota on class LRU list.
  static bool     gAllowDataURIs;            // Whether we should allow data
                                             // urls in -moz-binding. Needed for
                                             // testing.

  // Look up the class by key in gClassTable.
  static nsXBLJSClass *getClass(const nsCString &key);
  static nsXBLJSClass *getClass(nsCStringKey *key);
};

class nsXBLJSClass : public mozilla::LinkedListElement<nsXBLJSClass>
                   , public JSClass
{
private:
  nsrefcnt mRefCnt;
  nsCString mKey;
  static uint64_t sIdCount;
  nsrefcnt Destroy();

public:
  nsXBLJSClass(const nsAFlatCString& aClassName, const nsCString& aKey);
  ~nsXBLJSClass() { nsMemory::Free((void*) name); }

  static uint64_t NewId() { return ++sIdCount; }

  nsCString& Key() { return mKey; }
  void SetKey(const nsCString& aKey) { mKey = aKey; }

  nsrefcnt Hold() { return ++mRefCnt; }
  nsrefcnt Drop() { return --mRefCnt ? mRefCnt : Destroy(); }
  nsrefcnt AddRef() { return Hold(); }
  nsrefcnt Release() { return Drop(); }

  // Downcast from a pointer to const JSClass to a pointer to non-const
  // nsXBLJSClass.
  //
  // The const_cast is safe because nsXBLJSClass instances are never actually
  // const. It's necessary because we pass pointers to nsXBLJSClass to code
  // which uses pointers to const JSClass, and returns them back to us that
  // way, and we need to convert them back to pointers to non-const
  // nsXBLJSClass so that we can modify the reference count and add them to
  // the gClassLRUList list.
  static nsXBLJSClass*
  fromJSClass(const JSClass* c)
  {
    nsXBLJSClass* x = const_cast<nsXBLJSClass*>(static_cast<const nsXBLJSClass*>(c));
    MOZ_ASSERT(nsXBLService::getClass(x->mKey) == x);
    return x;
  }
};

#endif
