/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): Brendan Eich (brendan@mozilla.org)
 */

//////////////////////////////////////////////////////////////////////////////////////////

#include "nsIXBLService.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "jsapi.h"              // nsXBLJSClass derives from JSClass
#include "jsclist.h"            // nsXBLJSClass derives from JSCList
#include "nsFixedSizeAllocator.h"

class nsIXBLBinding;
class nsIXBLDocumentInfo;
class nsIXBLPrototypeHandler;
class nsINameSpaceManager;
class nsIContent;
class nsIDocument;
class nsIAtom;
class nsString;
class nsIURI;
class nsSupportsHashtable;
class nsHashtable;
class nsIXULPrototypeCache;
class nsIXULContentUtils;

class nsXBLService : public nsIXBLService, public nsIObserver, public nsSupportsWeakReference
{
  NS_DECL_ISUPPORTS

  // This function loads a particular XBL file and installs all of the bindings
  // onto the element.
  NS_IMETHOD LoadBindings(nsIContent* aContent, const nsAReadableString& aURL, PRBool aAugmentFlag,
                          nsIXBLBinding** aBinding, PRBool* aResolveStyle);

  // This method loads a binding doc and then builds the specific binding required.
  NS_IMETHOD GetBinding(nsIContent* aBoundElement, const nsCString& aURLStr, nsIXBLBinding** aResult);

  // Indicates whether or not a binding is fully loaded.
  NS_IMETHOD BindingReady(nsIContent* aBoundElement, const nsCString& aURLStr, PRBool* aIsReady);

  // This function clears out the bindings on a given content node.
  NS_IMETHOD FlushStyleBindings(nsIContent* aContent);

  // For a given element, returns a flat list of all the anonymous children that need
  // frames built.
  NS_IMETHOD GetContentList(nsIContent* aContent, nsISupportsArray** aResult, nsIContent** aChildElement,
                            PRBool* aMultipleInsertionPoints);

  // Gets the object's base class type.
  NS_IMETHOD ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult);

  NS_IMETHOD GetXBLDocumentInfo(const nsCString& aURLStr, nsIContent* aBoundElement, nsIXBLDocumentInfo** aResult);

  // This method checks the hashtable and then calls FetchBindingDocument on a miss.
  NS_IMETHOD LoadBindingDocumentInfo(nsIContent* aBoundElement, nsIDocument* aBoundDocument,
                                     const nsCString& aURI, const nsCString& aRef,
                                     PRBool aForceSyncLoad, nsIXBLDocumentInfo** aResult);

  // Used by XUL key bindings and for window XBL.
  NS_IMETHOD AttachGlobalKeyHandler(nsIDOMEventReceiver* aElement);

  NS_DECL_NSIOBSERVER

public:
  nsXBLService();
  virtual ~nsXBLService();

  // Release any memory that we can
  nsresult FlushMemory();
  
  // This method synchronously loads and parses an XBL file.
  NS_IMETHOD FetchBindingDocument(nsIContent* aBoundElement, nsIDocument* aBoundDocument,
                                  nsIURI* aURI, const nsCString& aRef, 
                                  PRBool aForceSyncLoad, nsIDocument** aResult);

  // This method loads a binding doc and then builds the specific binding required.  It
  // can also peek without building.
  NS_IMETHOD GetBindingInternal(nsIContent* aBoundElement, const nsCString& aURLStr, 
                                PRBool aPeekFlag, PRBool* aIsReady, nsIXBLBinding** aResult);

  // This method walks a binding document and removes any text nodes
  // that contain only whitespace.
  static nsresult StripWhitespaceNodes(nsIContent* aContent);
  static nsresult BuildHandlerChain(nsIContent* aContent, nsIXBLPrototypeHandler** aResult,
                                    nsIXBLPrototypeHandler** aSpecialResult);

// MEMBER VARIABLES
public:
  static nsIXULContentUtils* gXULUtils;
  static nsIXULPrototypeCache* gXULCache;
    
  static nsINameSpaceManager* gNameSpaceManager; // Used to register the XBL namespace
  static PRInt32  kNameSpaceID_XBL;          // Convenient cached XBL namespace.

  static PRUint32 gRefCnt;                   // A count of XBLservice instances.

  static PRBool gDisableChromeCache;

  static nsHashtable* gClassTable;           // A table of nsXBLJSClass objects.

  static JSCList  gClassLRUList;             // LRU list of cached classes.
  static PRUint32 gClassLRUListLength;       // Number of classes on LRU list.
  static PRUint32 gClassLRUListQuota;        // Quota on class LRU list.

  // XBL Atoms
  static nsIAtom* kExtendsAtom;
  static nsIAtom* kEventAtom;
  static nsIAtom* kScrollbarAtom;
  static nsIAtom* kInputAtom;

  nsFixedSizeAllocator mPool;
};

class nsXBLJSClass : public JSCList, public JSClass
{
private:
  nsrefcnt mRefCnt;
  nsrefcnt Destroy();

public:
  nsXBLJSClass(const nsCString& aClassName);
  ~nsXBLJSClass() { nsMemory::Free((void*) name); }

  nsrefcnt Hold() { return ++mRefCnt; }
  nsrefcnt Drop() { return --mRefCnt ? mRefCnt : Destroy(); }
};

