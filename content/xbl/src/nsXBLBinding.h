/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsIXBLBinding.h"
#include "nsIXBLPrototypeBinding.h"

class nsIContent;
class nsIAtom;
class nsIDocument;
class nsIScriptContext;
class nsISupportsArray;
class nsSupportsHashtable;
class nsIXBLService;
class nsFixedSizeAllocator;
class nsXBLEventHandler;

// *********************************************************************/
// The XBLBinding class

class nsXBLBinding: public nsIXBLBinding
{
  NS_DECL_ISUPPORTS

  // nsIXBLBinding
  NS_IMETHOD GetPrototypeBinding(nsIXBLPrototypeBinding** aResult);
  NS_IMETHOD SetPrototypeBinding(nsIXBLPrototypeBinding* aProtoBinding);

  NS_IMETHOD GetBaseBinding(nsIXBLBinding** aResult);
  NS_IMETHOD SetBaseBinding(nsIXBLBinding* aBinding);

  NS_IMETHOD GetAnonymousContent(nsIContent** aParent);
  NS_IMETHOD SetAnonymousContent(nsIContent* aParent);

  NS_IMETHOD GetBindingElement(nsIContent** aResult);
  NS_IMETHOD SetBindingElement(nsIContent* aElement);

  NS_IMETHOD GetBoundElement(nsIContent** aResult);
  NS_IMETHOD SetBoundElement(nsIContent* aElement);

  NS_IMETHOD GenerateAnonymousContent();
  NS_IMETHOD InstallEventHandlers();
  NS_IMETHOD InstallImplementation();
  
  NS_IMETHOD HasStyleSheets(PRBool* aResolveStyle);
  
  NS_IMETHOD GetFirstBindingWithConstructor(nsIXBLBinding** aResult);

  NS_IMETHOD GetBaseTag(PRInt32* aNameSpaceID, nsIAtom** aResult);

  NS_IMETHOD AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID, PRBool aRemoveFlag);

  NS_IMETHOD ExecuteAttachedHandler();
  NS_IMETHOD ExecuteDetachedHandler();

  NS_IMETHOD UnhookEventHandlers();
  NS_IMETHOD ChangeDocument(nsIDocument* aOldDocument, nsIDocument* aNewDocument);

  NS_IMETHOD GetBindingURI(nsCString& aResult);
  NS_IMETHOD GetDocURI(nsCString& aResult);
  NS_IMETHOD GetID(nsCString& aResult);

  NS_IMETHOD GetInsertionPointsFor(nsIContent* aParent, nsISupportsArray** aResult);

  NS_IMETHOD GetInsertionPoint(nsIContent* aChild, nsIContent** aResult, PRUint32* aIndex, nsIContent** aDefaultContent);
  NS_IMETHOD GetSingleInsertionPoint(nsIContent** aResult, PRUint32* aIndex, 
                                     PRBool* aMultipleInsertionPoints, nsIContent** aDefaultContent);

  NS_IMETHOD IsStyleBinding(PRBool* aResult) { *aResult = mIsStyleBinding; return NS_OK; };
  NS_IMETHOD SetIsStyleBinding(PRBool aIsStyle) { mIsStyleBinding = aIsStyle; return NS_OK; };

  NS_IMETHOD GetRootBinding(nsIXBLBinding** aResult);
  NS_IMETHOD GetFirstStyleBinding(nsIXBLBinding** aResult);

  NS_IMETHOD InheritsStyle(PRBool* aResult);
  NS_IMETHOD WalkRules(nsISupportsArrayEnumFunc aFunc, void* aData);
  NS_IMETHOD AttributeAffectsStyle(nsISupportsArrayEnumFunc aFunc, void* aData, PRBool* aAffects);

  NS_IMETHOD MarkForDeath();
  NS_IMETHOD MarkedForDeath(PRBool* aResult);

  NS_IMETHOD ImplementsInterface(REFNSIID aIID, PRBool* aResult);

  NS_IMETHOD GetAnonymousNodes(nsIDOMNodeList** aResult);

  NS_IMETHOD ShouldBuildChildFrames(PRBool* aResult);

public:
  nsXBLBinding(nsIXBLPrototypeBinding* aProtoBinding);
  virtual ~nsXBLBinding();

  NS_IMETHOD AddScriptEventListener(nsIContent* aElement, nsIAtom* aName, const nsString& aValue);

  PRBool AllowScripts();
  void InstallAnonymousContent(nsIContent* aAnonParent, nsIContent* aElement);

  static nsresult GetTextData(nsIContent *aParent, nsString& aResult);
  
// Static members
  static PRUint32 gRefCnt;
  
  // Used to easily obtain the correct IID for an event.
  struct EventHandlerMapEntry {
    const char*  mAttributeName;
    nsIAtom*     mAttributeAtom;
    const nsIID* mHandlerIID;
  };

  static EventHandlerMapEntry kEventHandlerMap[];

  static PRBool IsSupportedHandler(const nsIID* aIID);
  
  static void GetEventHandlerIID(nsIAtom* aName, nsIID* aIID, PRBool* aFound);

// Internal member functions
protected:
  NS_IMETHOD InitClass(const nsCString& aClassName,
                       nsIScriptContext* aContext, nsIDocument* aDocument,
                       void** aScriptObject, void** aClassObject);

  void GetImmediateChild(nsIAtom* aTag, nsIContent** aResult);
  PRBool IsInExcludesList(nsIAtom* aTag, const nsString& aList);

  
// MEMBER VARIABLES
protected:
  nsCOMPtr<nsIXBLPrototypeBinding> mPrototypeBinding; // Strong. As long as we're around, the binding can't go away.
  nsCOMPtr<nsIContent> mContent; // Strong. Our anonymous content stays around with us.
  nsCOMPtr<nsIXBLBinding> mNextBinding; // Strong. The derived binding owns the base class bindings.
  
  nsXBLEventHandler* mFirstHandler; // Weak. Our bound element owns the handler 
                                    // through the event listener manager.

  nsIContent* mBoundElement; // [WEAK] We have a reference, but we don't own it.
  
  nsSupportsHashtable* mInsertionPointTable;    // A hash from nsIContent* -> (a sorted array of nsIXBLInsertionPoint*)

  PRPackedBool mIsStyleBinding;
  PRPackedBool mMarkedForDeath;
};
