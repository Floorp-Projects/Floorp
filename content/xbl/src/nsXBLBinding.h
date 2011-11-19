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

#ifndef nsXBLBinding_h_
#define nsXBLBinding_h_

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsINodeList.h"
#include "nsIStyleRuleProcessor.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"

class nsXBLPrototypeBinding;
class nsIContent;
class nsIAtom;
class nsIDocument;
class nsIScriptContext;
class nsObjectHashtable;
class nsXBLInsertionPoint;
typedef nsTArray<nsRefPtr<nsXBLInsertionPoint> > nsInsertionPointList;
struct JSContext;
struct JSObject;

// *********************************************************************/
// The XBLBinding class

class nsXBLBinding
{
public:
  nsXBLBinding(nsXBLPrototypeBinding* aProtoBinding);
  ~nsXBLBinding();

  /**
   * XBLBindings are refcounted.  They are held onto in 3 ways:
   * 1. The binding manager's binding table holds onto all bindings that are
   *    currently attached to a content node.
   * 2. Bindings hold onto their base binding.  This is important since
   *    the base binding itself may not be attached to anything.
   * 3. The binding manager holds an additional reference to bindings
   *    which are queued to fire their constructors.
   */

  NS_INLINE_DECL_REFCOUNTING(nsXBLBinding)

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsXBLBinding)

  nsXBLPrototypeBinding* PrototypeBinding() { return mPrototypeBinding; }
  nsIContent* GetAnonymousContent() { return mContent.get(); }

  nsXBLBinding* GetBaseBinding() { return mNextBinding; }
  void SetBaseBinding(nsXBLBinding *aBinding);

  nsIContent* GetBoundElement() { return mBoundElement; }
  void SetBoundElement(nsIContent *aElement);

  bool IsStyleBinding() const { return mIsStyleBinding; }
  void SetIsStyleBinding(bool aIsStyle) { mIsStyleBinding = aIsStyle; }

  void MarkForDeath();
  bool MarkedForDeath() const { return mMarkedForDeath; }

  bool HasStyleSheets() const;
  bool InheritsStyle() const;
  bool ImplementsInterface(REFNSIID aIID) const;

  void GenerateAnonymousContent();
  void InstallAnonymousContent(nsIContent* aAnonParent, nsIContent* aElement);
  static void UninstallAnonymousContent(nsIDocument* aDocument,
                                        nsIContent* aAnonParent);
  void InstallEventHandlers();
  nsresult InstallImplementation();

  void ExecuteAttachedHandler();
  void ExecuteDetachedHandler();
  void UnhookEventHandlers();

  nsIAtom* GetBaseTag(PRInt32* aNameSpaceID);
  nsXBLBinding* RootBinding();
  nsXBLBinding* GetFirstStyleBinding();

  // Resolve all the fields for this binding and all ancestor bindings on the
  // object |obj|.  False return means a JS exception was set.
  bool ResolveAllFields(JSContext *cx, JSObject *obj) const;

  // Get the list of insertion points for aParent. The nsInsertionPointList
  // is owned by the binding, you should not delete it.
  nsresult GetInsertionPointsFor(nsIContent* aParent,
                                 nsInsertionPointList** aResult);

  nsInsertionPointList* GetExistingInsertionPointsFor(nsIContent* aParent);

  // XXXbz this aIndex has nothing to do with an index into the child
  // list of the insertion parent or anything.
  nsIContent* GetInsertionPoint(const nsIContent* aChild, PRUint32* aIndex);

  nsIContent* GetSingleInsertionPoint(PRUint32* aIndex,
                                      bool* aMultipleInsertionPoints);

  void AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID,
                        bool aRemoveFlag, bool aNotify);

  void ChangeDocument(nsIDocument* aOldDocument, nsIDocument* aNewDocument);

  void WalkRules(nsIStyleRuleProcessor::EnumFunc aFunc, void* aData);

  nsINodeList* GetAnonymousNodes();

  static nsresult DoInitJSClass(JSContext *cx, JSObject *global, JSObject *obj,
                                const nsAFlatCString& aClassName,
                                nsXBLPrototypeBinding* aProtoBinding,
                                JSObject** aClassObject);

  bool AllowScripts();  // XXX make const

  void RemoveInsertionParent(nsIContent* aParent);
  bool HasInsertionParent(nsIContent* aParent);

// MEMBER VARIABLES
protected:

  nsXBLPrototypeBinding* mPrototypeBinding; // Weak, but we're holding a ref to the docinfo
  nsCOMPtr<nsIContent> mContent; // Strong. Our anonymous content stays around with us.
  nsRefPtr<nsXBLBinding> mNextBinding; // Strong. The derived binding owns the base class bindings.
  
  nsIContent* mBoundElement; // [WEAK] We have a reference, but we don't own it.
  
  // A hash from nsIContent* -> (a sorted array of nsXBLInsertionPoint)
  nsClassHashtable<nsISupportsHashKey, nsInsertionPointList>* mInsertionPointTable;

  bool mIsStyleBinding;
  bool mMarkedForDeath;
};

#endif // nsXBLBinding_h_
