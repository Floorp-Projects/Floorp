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
#include "nsICSSStyleSheet.h"
#include "nsICSSLoaderObserver.h"

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

class nsXBLPrototypeBinding: public nsIXBLPrototypeBinding, public nsICSSLoaderObserver
{
  NS_DECL_ISUPPORTS

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aNotify);

  // nsIXBLPrototypeBinding
  NS_IMETHOD GetBindingElement(nsIContent** aResult);
  NS_IMETHOD SetBindingElement(nsIContent* aElement);

  NS_IMETHOD GetBindingURI(nsCString& aResult);
  NS_IMETHOD GetDocURI(nsCString& aResult);
  NS_IMETHOD GetID(nsCString& aResult);

  NS_IMETHOD GetAllowScripts(PRBool* aResult);

  NS_IMETHOD BindingAttached(nsIDOMEventReceiver* aRec);
  NS_IMETHOD BindingDetached(nsIDOMEventReceiver* aRec);

  NS_IMETHOD LoadResources(PRBool* aResult);

  NS_IMETHOD InheritsStyle(PRBool* aResult);

  NS_IMETHOD GetPrototypeHandlers(nsIXBLPrototypeHandler** aHandler);
  NS_IMETHOD SetPrototypeHandlers(nsIXBLPrototypeHandler* aHandler);
  
  NS_IMETHOD AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID, PRBool aRemoveFlag, 
                              nsIContent* aChangedElement, nsIContent* aAnonymousContent);

  NS_IMETHOD SetBasePrototype(nsIXBLPrototypeBinding* aBinding);
  NS_IMETHOD GetBasePrototype(nsIXBLPrototypeBinding** aResult);

  NS_IMETHOD GetXBLDocumentInfo(nsIContent* aBoundElement, nsIXBLDocumentInfo** aResult);
  
  NS_IMETHOD HasBasePrototype(PRBool* aResult);
  NS_IMETHOD SetHasBasePrototype(PRBool aHasBase);

  NS_IMETHOD SetInitialAttributes(nsIContent* aBoundElement, nsIContent* aAnonymousContent);

  NS_IMETHOD GetRuleProcessors(nsISupportsArray** aResult);
  NS_IMETHOD GetStyleSheets(nsISupportsArray** aResult);

  NS_IMETHOD HasInsertionPoints(PRBool* aResult) { *aResult = (mInsertionPointTable != nsnull); return NS_OK; };
  NS_IMETHOD HasStyleSheets(PRBool* aResult) { *aResult = (mStyleSheetList != nsnull); return NS_OK; };

  NS_IMETHOD InstantiateInsertionPoints(nsIXBLBinding* aBinding);

  NS_IMETHOD GetInsertionPoint(nsIContent* aBoundElement, nsIContent* aCopyRoot,
                               nsIContent* aChild, nsIContent** aResult, PRUint32* aIndex,
                               nsIContent** aDefaultContent);

  NS_IMETHOD GetSingleInsertionPoint(nsIContent* aBoundElement, nsIContent* aCopyRoot,
                                     nsIContent** aResult, PRUint32* aIndex, PRBool* aMultiple,
                                     nsIContent** aDefaultContent);

  NS_IMETHOD GetBaseTag(PRInt32* aNamespaceID, nsIAtom** aTag);
  NS_IMETHOD SetBaseTag(PRInt32 aNamespaceID, nsIAtom* aTag);

  NS_IMETHOD ImplementsInterface(REFNSIID aIID, PRBool* aResult);

  NS_IMETHOD ShouldBuildChildFrames(PRBool* aResult);

  NS_IMETHOD AddResourceListener(nsIContent* aBoundElement);

  NS_IMETHOD GetConstructor(nsIXBLPrototypeHandler** aResult) { *aResult = mConstructor; NS_IF_ADDREF(*aResult); return NS_OK; };

public:
  nsXBLPrototypeBinding(const nsAReadableCString& aRef, nsIContent* aElement, 
                        nsIXBLDocumentInfo* aInfo);
  virtual ~nsXBLPrototypeBinding();

// Static members
  static PRUint32 gRefCnt;
  static nsIAtom* kInheritStyleAtom;
  static nsIAtom* kHandlersAtom;
  static nsIAtom* kChildrenAtom;
  static nsIAtom* kIncludesAtom;
  static nsIAtom* kContentAtom;
  static nsIAtom* kResourcesAtom;
  static nsIAtom* kResourceAtom;
  static nsIAtom* kStyleSheetAtom;
  static nsIAtom* kSrcAtom;
  static nsIAtom* kInheritsAtom;
  static nsIAtom* kHTMLAtom;
  static nsIAtom* kValueAtom;
  static nsIAtom* kXBLTextAtom;
  static nsIAtom* kConstructorAtom;
  static nsIAtom* kDestructorAtom;
  static nsIAtom* kImplementationAtom;
  static nsIAtom* kImplementsAtom;

  static nsFixedSizeAllocator* kAttrPool;
  static nsFixedSizeAllocator* kInsPool;

// Internal member functions
public:
  void GetImmediateChild(nsIAtom* aTag, nsIContent** aResult);
  void LocateInstance(nsIContent* aBoundElt, nsIContent* aTemplRoot, nsIContent* aCopyRoot,
                      nsIContent* aTemplChild, nsIContent** aCopyResult);

protected:  
  void ConstructHandlers();
  void ConstructAttributeTable(nsIContent* aElement);
  void ConstructInsertionTable(nsIContent* aElement);
  void ConstructInterfaceTable(nsIContent* aElement);
  void GetNestedChildren(nsIAtom* aTag, nsIContent* aContent, nsISupportsArray** aList);
  void NotifyBoundElements();

protected:
  // Internal helper class for managing our IID table.
  class nsIIDKey : public nsHashKey {
    protected:
      nsIID mKey;
  
    public:
      nsIIDKey(REFNSIID key) : mKey(key) {}
      ~nsIIDKey(void) {}

      PRUint32 HashCode(void) const {
        // Just use the 32-bit m0 field.
        return mKey.m0;
      }

      PRBool Equals(const nsHashKey *aKey) const {
        return mKey.Equals( ((nsIIDKey*) aKey)->mKey);
      }

      nsHashKey *Clone(void) const {
        return new nsIIDKey(mKey);
      }
  };

// MEMBER VARIABLES
protected:
  nsCString mID;

  nsCOMPtr<nsIContent> mBinding; // Strong. We own a ref to our content element in the binding doc.
  nsCOMPtr<nsIXBLPrototypeHandler> mPrototypeHandler; // Strong. DocInfo owns us, and we own the handlers.
  nsCOMPtr<nsIXBLPrototypeHandler> mConstructor; // Strong.  Our constructor.
  nsCOMPtr<nsIXBLPrototypeHandler> mDestructor; // Strong. Our destructor.

  nsCOMPtr<nsIXBLPrototypeBinding> mBaseBinding; // Strong. We own the base binding in our explicit inheritance chain.
  PRPackedBool mInheritStyle;
  PRPackedBool mHasBaseProto;
  PRPackedBool mLoadingResources;
  PRPackedBool mInLoadResourcesFunc;

  PRInt32 mPendingSheets; // The number of stylesheets that have yet to load.
  nsCOMPtr<nsISupportsArray> mBoundElements; // Bound elements that are waiting on the stylesheets and scripts.

  nsWeakPtr mXBLDocInfoWeak; // A pointer back to our doc info.  Weak, since it owns us.

  nsSupportsHashtable* mAttributeTable; // A table for attribute entries.  Used to efficiently
                                        // handle attribute changes.

  nsSupportsHashtable* mInsertionPointTable; // A table of insertion points for placing explicit content
                                             // underneath anonymous content.

  nsSupportsHashtable* mInterfaceTable; // A table of cached interfaces that we support.

  nsCOMPtr<nsISupportsArray> mStyleSheetList; // A list of loaded stylesheets for this binding.
  nsCOMPtr<nsISupportsArray> mRuleProcessors; // The list of stylesheets converted to rule processors.

  PRInt32 mBaseNameSpaceID;    // If we extend a tagname/namespace, then that information will
  nsCOMPtr<nsIAtom> mBaseTag;  // be stored in here.
};
