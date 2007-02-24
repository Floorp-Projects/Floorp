/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
 *   - Brendan Eich (brendan@mozilla.org)
 *   - Mike Pinkerton (pinkerton@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//////////////////////////////////////////////////////////////////////////////////////////

#include "nsIXBLService.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "jsapi.h"              // nsXBLJSClass derives from JSClass
#include "jsclist.h"            // nsXBLJSClass derives from JSCList
#include "nsFixedSizeAllocator.h"
#include "nsTArray.h"

class nsXBLBinding;
class nsIXBLDocumentInfo;
class nsIContent;
class nsIDocument;
class nsIAtom;
class nsString;
class nsIURI;
class nsSupportsHashtable;
class nsHashtable;
class nsIXULPrototypeCache;

class nsXBLService : public nsIXBLService,
                     public nsIObserver,
                     public nsSupportsWeakReference
{
  NS_DECL_ISUPPORTS

  // This function loads a particular XBL file and installs all of the bindings
  // onto the element.
  NS_IMETHOD LoadBindings(nsIContent* aContent, nsIURI* aURL, PRBool aAugmentFlag,
                          nsXBLBinding** aBinding, PRBool* aResolveStyle);

  // Indicates whether or not a binding is fully loaded.
  NS_IMETHOD BindingReady(nsIContent* aBoundElement, nsIURI* aURI, PRBool* aIsReady);

  // Gets the object's base class type.
  NS_IMETHOD ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult);

  // This method checks the hashtable and then calls FetchBindingDocument on a miss.
  NS_IMETHOD LoadBindingDocumentInfo(nsIContent* aBoundElement, nsIDocument* aBoundDocument,
                                     nsIURI* aBindingURI,
                                     PRBool aForceSyncLoad, nsIXBLDocumentInfo** aResult);

  // Used by XUL key bindings and for window XBL.
  NS_IMETHOD AttachGlobalKeyHandler(nsIDOMEventReceiver* aElement);

  NS_DECL_NSIOBSERVER

public:
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
                                PRBool aForceSyncLoad, nsIDocument** aResult);

  nsresult GetXBLDocumentInfo(nsIURI* aURI, nsIContent* aBoundElement, nsIXBLDocumentInfo** aResult);

  /**
   * This method calls the one below with an empty |aDontExtendURIs| array.
   */
  nsresult GetBinding(nsIContent* aBoundElement, nsIURI* aURI,
                      PRBool aPeekFlag, PRBool* aIsReady,
                      nsXBLBinding** aResult);

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
   */
  nsresult GetBinding(nsIContent* aBoundElement, nsIURI* aURI,
                      PRBool aPeekFlag, PRBool* aIsReady,
                      nsXBLBinding** aResult,
                      nsTArray<nsIURI*>& aDontExtendURIs);

// MEMBER VARIABLES
public:
#ifdef MOZ_XUL
  static nsIXULPrototypeCache* gXULCache;
#endif
    
  static PRUint32 gRefCnt;                   // A count of XBLservice instances.

  static PRBool gDisableChromeCache;

  static nsHashtable* gClassTable;           // A table of nsXBLJSClass objects.

  static JSCList  gClassLRUList;             // LRU list of cached classes.
  static PRUint32 gClassLRUListLength;       // Number of classes on LRU list.
  static PRUint32 gClassLRUListQuota;        // Quota on class LRU list.

  nsFixedSizeAllocator mPool;
};

class nsXBLJSClass : public JSCList, public JSClass
{
private:
  nsrefcnt mRefCnt;
  nsrefcnt Destroy();

public:
  nsXBLJSClass(const nsAFlatCString& aClassName);
  ~nsXBLJSClass() { nsMemory::Free((void*) name); }

  nsrefcnt Hold() { return ++mRefCnt; }
  nsrefcnt Drop() { return --mRefCnt ? mRefCnt : Destroy(); }
};

