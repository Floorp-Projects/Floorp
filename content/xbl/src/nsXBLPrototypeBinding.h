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
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsIXBLPrototypeBinding.h"
#include "nsIXBLPrototypeHandler.h"

class nsIContent;
class nsIAtom;
class nsIDocument;
class nsIScriptContext;
class nsISupportsArray;
class nsSupportsHashtable;
class nsIXBLService;
class nsFixedSizeAllocator;

// *********************************************************************/
// The XBLPrototypeBinding class

class nsXBLPrototypeBinding: public nsIXBLPrototypeBinding
{
  NS_DECL_ISUPPORTS

  // nsIXBLPrototypeBinding
  NS_IMETHOD GetBindingElement(nsIContent** aResult);
  NS_IMETHOD SetBindingElement(nsIContent* aElement);

  NS_IMETHOD GetBindingURI(nsCString& aResult);
  NS_IMETHOD GetDocURI(nsCString& aResult);
  NS_IMETHOD GetID(nsCString& aResult);

  NS_IMETHOD GetAllowScripts(PRBool* aResult);

  NS_IMETHOD InheritsStyle(PRBool* aResult);

  NS_IMETHOD GetPrototypeHandler(nsIXBLPrototypeHandler** aHandler);
  NS_IMETHOD SetPrototypeHandler(nsIXBLPrototypeHandler* aHandler);
  
  NS_IMETHOD AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID, PRBool aRemoveFlag, 
                              nsIContent* aChangedElement, nsIContent* aAnonymousContent);

  NS_IMETHOD SetBasePrototype(nsIXBLPrototypeBinding* aBinding);
  NS_IMETHOD GetBasePrototype(nsIXBLPrototypeBinding** aResult);

  NS_IMETHOD GetXBLDocumentInfo(nsIContent* aBoundElement, nsIXBLDocumentInfo** aResult);
  
  NS_IMETHOD HasBasePrototype(PRBool* aResult);
  NS_IMETHOD SetHasBasePrototype(PRBool aHasBase);

  NS_IMETHOD SetInitialAttributes(nsIContent* aBoundElement, nsIContent* aAnonymousContent);

  NS_IMETHOD HasInsertionPoints(PRBool* aResult) { *aResult = (mInsertionPointTable != nsnull); return NS_OK; };

  NS_IMETHOD GetInsertionPoint(nsIContent* aBoundElement, nsIContent* aCopyRoot,
                               nsIContent* aChild, nsIContent** aResult);

  NS_IMETHOD GetSingleInsertionPoint(nsIContent* aBoundElement, nsIContent* aCopyRoot,
                                     nsIContent** aResult, PRBool* aMultiple);

  NS_IMETHOD GetBaseTag(PRInt32* aNamespaceID, nsIAtom** aTag);
  NS_IMETHOD SetBaseTag(PRInt32 aNamespaceID, nsIAtom* aTag);

public:
  nsXBLPrototypeBinding(const nsCString& aRef, nsIContent* aElement, 
                        nsIXBLDocumentInfo* aInfo);
  virtual ~nsXBLPrototypeBinding();

// Static members
  static PRUint32 gRefCnt;
  static nsIXBLService* gXBLService;
  static nsIAtom* kInheritStyleAtom;
  static nsIAtom* kHandlersAtom;
  static nsIAtom* kChildrenAtom;
  static nsIAtom* kIncludesAtom;
  static nsIAtom* kContentAtom;
  static nsIAtom* kInheritsAtom;
  static nsIAtom* kHTMLAtom;
  static nsIAtom* kValueAtom;
  
  static nsFixedSizeAllocator kPool;

// Internal member functions
public:
  void GetImmediateChild(nsIAtom* aTag, nsIContent** aResult);
  void LocateInstance(nsIContent* aTemplRoot, nsIContent* aCopyRoot,
                      nsIContent* aTemplChild, nsIContent** aCopyResult);

protected:  
  void ConstructHandlers();
  void ConstructAttributeTable(nsIContent* aElement);
  void ConstructInsertionTable(nsIContent* aElement);
  void GetNestedChildren(nsIAtom* aTag, nsIContent* aContent, nsISupportsArray** aList);
  
// MEMBER VARIABLES
protected:
  nsCString mID;

  nsCOMPtr<nsIContent> mBinding; // Strong. We own a ref to our content element in the binding doc.
  nsCOMPtr<nsIXBLPrototypeHandler> mPrototypeHandler; // Strong. DocInfo owns us, and we own the handlers.
  nsCOMPtr<nsIXBLPrototypeBinding> mBaseBinding; // Strong. We own the base binding in our explicit inheritance chain.
  PRPackedBool mInheritStyle;
  PRPackedBool mHasBaseProto;

  nsWeakPtr mXBLDocInfoWeak; // A pointer back to our doc info.  Weak, since it owns us.

  nsSupportsHashtable* mAttributeTable; // A table for attribute entries.  Used to efficiently
                                        // handle attribute changes.

  nsSupportsHashtable* mInsertionPointTable; // A table of insertion points for placing explicit content
                                             // underneath anonymous content.

  PRInt32 mBaseNameSpaceID;    // If we extend a tagname/namespace, then that information will
  nsCOMPtr<nsIAtom> mBaseTag;  // be stored in here.
};
