/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

/*

  Private interface to the XBL Binding

*/

#ifndef nsIXBLBinding_h__
#define nsIXBLBinding_h__

#include "nsISupports.h"
#include "nsIStyleRuleProcessor.h"

class nsIContent;
class nsIDocument;
class nsIDOMNodeList;
class nsIScriptContext;
class nsXBLPrototypeBinding;
class nsVoidArray;
class nsIURI;
class nsACString;

// {DDDBAD20-C8DF-11d3-97FB-00400553EEF0}
#define NS_IXBLBINDING_IID \
{ 0xdddbad20, 0xc8df, 0x11d3, { 0x97, 0xfb, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }

class nsIXBLBinding : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXBLBINDING_IID)

  NS_IMETHOD GetPrototypeBinding(nsXBLPrototypeBinding** aResult)=0;
  NS_IMETHOD SetPrototypeBinding(nsXBLPrototypeBinding* aProtoBinding)=0;

  NS_IMETHOD GetBaseBinding(nsIXBLBinding** aResult) = 0;
  NS_IMETHOD SetBaseBinding(nsIXBLBinding* aBinding) = 0;

  NS_IMETHOD GetAnonymousContent(nsIContent** aParent) = 0;
  NS_IMETHOD SetAnonymousContent(nsIContent* aParent) = 0;

  NS_IMETHOD GetBindingElement(nsIContent** aResult) = 0;
  NS_IMETHOD SetBindingElement(nsIContent* aElement) = 0;

  NS_IMETHOD GetBoundElement(nsIContent** aResult) = 0;
  NS_IMETHOD SetBoundElement(nsIContent* aElement) = 0;

  NS_IMETHOD GenerateAnonymousContent() = 0;
  NS_IMETHOD InstallEventHandlers() = 0;
  NS_IMETHOD InstallImplementation() = 0;
  
  NS_IMETHOD HasStyleSheets(PRBool* aResolveStyle) = 0;

  NS_IMETHOD GetFirstBindingWithConstructor(nsIXBLBinding** aResult)=0;

  NS_IMETHOD GetBaseTag(PRInt32* aNameSpaceID, nsIAtom** aResult) = 0;

  // Called when an attribute changes on a binding.
  NS_IMETHOD AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID,
                              PRBool aRemoveFlag, PRBool aNotify) = 0;

  NS_IMETHOD ExecuteAttachedHandler()=0;
  NS_IMETHOD ExecuteDetachedHandler()=0;
  NS_IMETHOD UnhookEventHandlers() = 0;
  NS_IMETHOD ChangeDocument(nsIDocument* aOldDocument, nsIDocument* aNewDocument) = 0;

  NS_IMETHOD_(nsIURI*) BindingURI() const = 0;
  NS_IMETHOD_(nsIURI*) DocURI() const = 0;
  NS_IMETHOD GetID(nsACString& aResult) const = 0;

  // Get the list of insertion points for aParent.  The nsVoidArray is owned
  // by the binding, you should not delete it.
  NS_IMETHOD GetInsertionPointsFor(nsIContent* aParent, nsVoidArray** aResult)=0;

  NS_IMETHOD GetInsertionPoint(nsIContent* aChild, nsIContent** aResult, PRUint32* aIndex,
                               nsIContent** aDefaultContent) = 0;
  NS_IMETHOD GetSingleInsertionPoint(nsIContent** aResult, PRUint32* aIndex, 
                                     PRBool* aMultipleInsertionPoints, nsIContent** aDefaultContent) = 0;

  NS_IMETHOD IsStyleBinding(PRBool* aResult) = 0;
  NS_IMETHOD SetIsStyleBinding(PRBool aIsStyle) = 0;

  NS_IMETHOD GetRootBinding(nsIXBLBinding** aResult) = 0;
  NS_IMETHOD GetFirstStyleBinding(nsIXBLBinding** aResult) = 0;

  NS_IMETHOD InheritsStyle(PRBool* aResult)=0;
  NS_IMETHOD WalkRules(nsIStyleRuleProcessor::EnumFunc aFunc, void* aData)=0;

  NS_IMETHOD MarkForDeath()=0;
  NS_IMETHOD MarkedForDeath(PRBool* aResult)=0;

  NS_IMETHOD ImplementsInterface(REFNSIID aIID, PRBool* aResult)=0;

  NS_IMETHOD GetAnonymousNodes(nsIDOMNodeList** aResult)=0;

  NS_IMETHOD ShouldBuildChildFrames(PRBool* aResult)=0;
};

nsresult
NS_NewXBLBinding(nsXBLPrototypeBinding* aProtoBinding,
                 nsIXBLBinding** aResult);

#endif // nsIXBLBinding_h__
