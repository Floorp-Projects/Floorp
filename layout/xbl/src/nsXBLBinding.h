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
#include "nsIXBLBinding.h"

class nsIContent;
class nsIAtom;
class nsIDocument;
class nsIScriptContext;
class nsISupportsArray;
class nsSupportsHashtable;
class nsIXBLService;
class nsFixedSizeAllocator;

// *********************************************************************/
// The XBLBinding class

class nsXBLBinding: public nsIXBLBinding
{
  NS_DECL_ISUPPORTS

  // nsIXBLBinding
  NS_IMETHOD GetBaseBinding(nsIXBLBinding** aResult);
  NS_IMETHOD SetBaseBinding(nsIXBLBinding* aBinding);

  NS_IMETHOD GetAnonymousContent(nsIContent** aParent);
  NS_IMETHOD SetAnonymousContent(nsIContent* aParent);

  NS_IMETHOD GetBindingElement(nsIContent** aResult);
  NS_IMETHOD SetBindingElement(nsIContent* aElement);

  NS_IMETHOD GetBoundElement(nsIContent** aResult);
  NS_IMETHOD SetBoundElement(nsIContent* aElement);

  NS_IMETHOD GenerateAnonymousContent(nsIContent* aBoundElement);
  NS_IMETHOD InstallEventHandlers(nsIContent* aBoundElement, nsIXBLBinding** aBinding);
  NS_IMETHOD InstallProperties(nsIContent* aBoundElement);

  NS_IMETHOD GetBaseTag(PRInt32* aNameSpaceID, nsIAtom** aResult);

  NS_IMETHOD AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID, PRBool aRemoveFlag);

  NS_IMETHOD ExecuteAttachedHandler();
  NS_IMETHOD ExecuteDetachedHandler();

  NS_IMETHOD UnhookEventHandlers();
  NS_IMETHOD ChangeDocument(nsIDocument* aOldDocument, nsIDocument* aNewDocument);

  NS_IMETHOD GetBindingURI(nsCString& aResult);
  NS_IMETHOD GetDocURI(nsCString& aResult);
  NS_IMETHOD GetID(nsCString& aResult);

  NS_IMETHOD GetInsertionPoint(nsIContent* aChild, nsIContent** aResult);
  NS_IMETHOD GetSingleInsertionPoint(nsIContent** aResult, PRBool* aMultipleInsertionPoints);

  NS_IMETHOD IsStyleBinding(PRBool* aResult) { *aResult = mIsStyleBinding; return NS_OK; };
  NS_IMETHOD SetIsStyleBinding(PRBool aIsStyle) { mIsStyleBinding = aIsStyle; return NS_OK; };

  NS_IMETHOD GetRootBinding(nsIXBLBinding** aResult);
  NS_IMETHOD GetFirstStyleBinding(nsIXBLBinding** aResult);

  NS_IMETHOD InheritsStyle(PRBool* aResult);
  NS_IMETHOD WalkRules(nsISupportsArrayEnumFunc aFunc, void* aData);

  NS_IMETHOD SetAllowScripts(PRBool aFlag) { mAllowScripts = aFlag; return NS_OK; };

  NS_IMETHOD MarkForDeath();
  NS_IMETHOD MarkedForDeath(PRBool* aResult);

public:
  nsXBLBinding(const nsCString& aDocURI, const nsCString& aRef);
  virtual ~nsXBLBinding();

  NS_IMETHOD AddScriptEventListener(nsIContent* aElement, nsIAtom* aName, const nsString& aValue, REFNSIID aIID);

  PRBool AllowScripts();
  
  static nsresult GetTextData(nsIContent *aParent, nsString& aResult);
  
// Static members
  static PRUint32 gRefCnt;
  
  static nsIAtom* kContentAtom;
  static nsIAtom* kInterfaceAtom;
  static nsIAtom* kHandlersAtom;
  static nsIAtom* kExcludesAtom;
  static nsIAtom* kIncludesAtom;
  static nsIAtom* kInheritsAtom;
  static nsIAtom* kTypeAtom;
  static nsIAtom* kCapturerAtom;
  static nsIAtom* kExtendsAtom;
  static nsIAtom* kChildrenAtom;
  static nsIAtom* kMethodAtom;
  static nsIAtom* kArgumentAtom;
  static nsIAtom* kBodyAtom;
  static nsIAtom* kPropertyAtom;
  static nsIAtom* kOnSetAtom;
  static nsIAtom* kOnGetAtom;
  static nsIAtom* kGetterAtom;
  static nsIAtom* kSetterAtom;
  static nsIAtom* kHTMLAtom;
  static nsIAtom* kValueAtom;
  static nsIAtom* kNameAtom;
  static nsIAtom* kReadOnlyAtom;
  static nsIAtom* kAttachToAtom;
  static nsIAtom* kBindingAttachedAtom;
  static nsIAtom* kInheritStyleAtom;

  static nsIXBLService* gXBLService;
  
  // Used to easily obtain the correct IID for an event.
  struct EventHandlerMapEntry {
    const char*  mAttributeName;
    nsIAtom*     mAttributeAtom;
    const nsIID* mHandlerIID;
  };

  static EventHandlerMapEntry kEventHandlerMap[];

  static PRBool IsMouseHandler(const nsString& aName);
  static PRBool IsKeyHandler(const nsString& aName);
  static PRBool IsFocusHandler(const nsString& aName);
  static PRBool IsXULHandler(const nsString& aName);
  static PRBool IsScrollHandler(const nsString& aName);

  static nsFixedSizeAllocator kPool;

// Internal member functions
protected:
  NS_IMETHOD InitClass(const nsCString& aClassName,
                       nsIScriptContext* aContext, nsIDocument* aDocument,
                       void** aScriptObject, void** aClassObject);

  void GetImmediateChild(nsIAtom* aTag, nsIContent** aResult);
  void GetNestedChild(nsIAtom* aTag, nsIContent* aContent, nsIContent** aResult);
  void GetNestedChildren(nsIAtom* aTag, nsIContent* aContent, nsISupportsArray* aList);
  void BuildInsertionTable();
  void GetNestedChildren();
  PRBool IsInExcludesList(nsIAtom* aTag, const nsString& aList);

  NS_IMETHOD ConstructAttributeTable(nsIContent* aElement); 

  static void GetEventHandlerIID(nsIAtom* aName, nsIID* aIID, PRBool* aFound);

// MEMBER VARIABLES
protected:
  nsCString mDocURI;
  nsCString mID;

  nsCOMPtr<nsIContent> mBinding; // Strong. As long as we're around, the binding can't go away.
  nsCOMPtr<nsIContent> mContent; // Strong. Our anonymous content stays around with us.
  nsCOMPtr<nsIXBLBinding> mNextBinding; // Strong. The derived binding owns the base class bindings.
  
  nsXBLEventHandler* mFirstHandler; // Weak. Our bound element owns the handler 
                                    // through the event listener manager.

  nsIContent* mBoundElement; // [WEAK] We have a reference, but we don't own it.
  
  PRPackedBool mIsStyleBinding;
  PRPackedBool mAllowScripts;
  PRPackedBool mInheritStyle;
  PRPackedBool mMarkedForDeath;

  nsSupportsHashtable* mAttributeTable; // A table for attribute entries.
  nsSupportsHashtable* mInsertionPointTable; // A table of insertion points.
};
