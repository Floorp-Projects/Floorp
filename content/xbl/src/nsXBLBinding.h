/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLBinding_h_
#define nsXBLBinding_h_

#include "nsXBLService.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsINodeList.h"
#include "nsIStyleRuleProcessor.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "jsapi.h"

class nsXBLPrototypeBinding;
class nsIContent;
class nsIAtom;
class nsIDocument;
class nsIScriptContext;
class nsXBLChildrenElement;
class nsAnonymousContentList;
struct JSContext;
class JSObject;

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

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsXBLBinding)

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsXBLBinding)

  nsXBLPrototypeBinding* PrototypeBinding() { return mPrototypeBinding; }
  nsIContent* GetAnonymousContent() { return mContent.get(); }
  nsXBLBinding* GetBindingWithContent();

  nsXBLBinding* GetBaseBinding() { return mNextBinding; }
  void SetBaseBinding(nsXBLBinding *aBinding);

  nsIContent* GetBoundElement() { return mBoundElement; }
  void SetBoundElement(nsIContent *aElement);

  void SetJSClass(nsXBLJSClass *aClass) {
    MOZ_ASSERT(!mJSClass && aClass);
    mJSClass = aClass;
  }

  bool IsStyleBinding() const { return mIsStyleBinding; }
  void SetIsStyleBinding(bool aIsStyle) { mIsStyleBinding = aIsStyle; }

  /*
   * Does a lookup for a method or attribute provided by one of the bindings'
   * prototype implementation. If found, |desc| will be set up appropriately,
   * and wrapped into cx->compartment.
   *
   * May only be called when XBL code is being run in a separate scope, because
   * otherwise we don't have untainted data with which to do a proper lookup.
   */
  bool LookupMember(JSContext* aCx, JS::HandleId aId, JSPropertyDescriptor* aDesc);

  /*
   * Determines whether the binding has a field with the given name.
   */
  bool HasField(nsString& aName);

protected:

  /*
   * Internal version. Requires that aCx is in appropriate xbl scope.
   */
  bool LookupMemberInternal(JSContext* aCx, nsString& aName, JS::HandleId aNameAsId,
                            JSPropertyDescriptor* aDesc, JS::Handle<JSObject*> aXBLScope);

public:

  void MarkForDeath();
  bool MarkedForDeath() const { return mMarkedForDeath; }

  bool HasStyleSheets() const;
  bool InheritsStyle() const;
  bool ImplementsInterface(REFNSIID aIID) const;

  void GenerateAnonymousContent();
  void InstallAnonymousContent(nsIContent* aAnonParent, nsIContent* aElement,
                               bool aNativeAnon);
  static void UninstallAnonymousContent(nsIDocument* aDocument,
                                        nsIContent* aAnonParent);
  void InstallEventHandlers();
  nsresult InstallImplementation();

  void ExecuteAttachedHandler();
  void ExecuteDetachedHandler();
  void UnhookEventHandlers();

  nsIAtom* GetBaseTag(int32_t* aNameSpaceID);
  nsXBLBinding* RootBinding();
  nsXBLBinding* GetFirstStyleBinding();

  // Resolve all the fields for this binding and all ancestor bindings on the
  // object |obj|.  False return means a JS exception was set.
  bool ResolveAllFields(JSContext *cx, JS::Handle<JSObject*> obj) const;

  void AttributeChanged(nsIAtom* aAttribute, int32_t aNameSpaceID,
                        bool aRemoveFlag, bool aNotify);

  void ChangeDocument(nsIDocument* aOldDocument, nsIDocument* aNewDocument);

  void WalkRules(nsIStyleRuleProcessor::EnumFunc aFunc, void* aData);

  static nsresult DoInitJSClass(JSContext *cx, JS::Handle<JSObject*> global,
                                JS::Handle<JSObject*> obj,
                                const nsAFlatCString& aClassName,
                                nsXBLPrototypeBinding* aProtoBinding,
                                JS::MutableHandle<JSObject*> aClassObject,
                                bool* aNew);

  bool AllowScripts();  // XXX make const

  nsXBLChildrenElement* FindInsertionPointFor(nsIContent* aChild);

  bool HasFilteredInsertionPoints()
  {
    return !mInsertionPoints.IsEmpty();
  }

  nsXBLChildrenElement* GetDefaultInsertionPoint()
  {
    return mDefaultInsertionPoint;
  }

  // Removes all inserted node from <xbl:children> insertion points under us.
  void ClearInsertionPoints();

  // Returns a live node list that iterates over the anonymous nodes generated
  // by this binding.
  nsAnonymousContentList* GetAnonymousNodeList();

// MEMBER VARIABLES
protected:

  bool mIsStyleBinding;
  bool mMarkedForDeath;

  nsXBLPrototypeBinding* mPrototypeBinding; // Weak, but we're holding a ref to the docinfo
  nsCOMPtr<nsIContent> mContent; // Strong. Our anonymous content stays around with us.
  nsRefPtr<nsXBLBinding> mNextBinding; // Strong. The derived binding owns the base class bindings.
  nsRefPtr<nsXBLJSClass> mJSClass; // Strong. The class object also holds a strong reference,
                                   // which might be somewhat redundant, but be safe to avoid
                                   // worrying about edge cases.

  nsIContent* mBoundElement; // [WEAK] We have a reference, but we don't own it.

  // The <xbl:children> elements that we found in our <xbl:content> when we
  // processed this binding. The default insertion point has no includes
  // attribute and all other insertion points must have at least one includes
  // attribute. These points must be up-to-date with respect to their parent's
  // children, even if their parent has another binding attached to it,
  // preventing us from rendering their contents directly.
  nsRefPtr<nsXBLChildrenElement> mDefaultInsertionPoint;
  nsTArray<nsRefPtr<nsXBLChildrenElement> > mInsertionPoints;
  nsRefPtr<nsAnonymousContentList> mAnonymousContentList;

  nsXBLChildrenElement* FindInsertionPointForInternal(nsIContent* aChild);
};

#endif // nsXBLBinding_h_
