/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLBinding_h_
#define nsXBLBinding_h_

#include "nsXBLService.h"
#include "nsCOMPtr.h"
#include "nsINodeList.h"
#include "nsClassHashtable.h"
#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupportsImpl.h"
#include "js/TypeDecls.h"

class nsXBLPrototypeBinding;
class nsIContent;
class nsAtom;
struct RawServoAuthorStyles;

namespace mozilla {
namespace dom {
class Document;
class XBLChildrenElement;
}  // namespace dom
}  // namespace mozilla

class nsAnonymousContentList;

// *********************************************************************/
// The XBLBinding class

class nsXBLBinding final {
 public:
  explicit nsXBLBinding(nsXBLPrototypeBinding* aProtoBinding);

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

  nsXBLPrototypeBinding* PrototypeBinding() const { return mPrototypeBinding; }
  nsIContent* GetAnonymousContent() { return mContent.get(); }
  nsXBLBinding* GetBindingWithContent();

  nsXBLBinding* GetBaseBinding() const { return mNextBinding; }
  void SetBaseBinding(nsXBLBinding* aBinding);

  mozilla::dom::Element* GetBoundElement() { return mBoundElement; }
  void SetBoundElement(mozilla::dom::Element* aElement);

  /*
   * Does a lookup for a method or attribute provided by one of the bindings'
   * prototype implementation. If found, |desc| will be set up appropriately,
   * and wrapped into cx->compartment.
   *
   * May only be called when XBL code is being run in a separate scope, because
   * otherwise we don't have untainted data with which to do a proper lookup.
   */
  bool LookupMember(JSContext* aCx, JS::Handle<jsid> aId,
                    JS::MutableHandle<JS::PropertyDescriptor> aDesc);

  /*
   * Determines whether the binding has a field with the given name.
   */
  bool HasField(nsString& aName);

 protected:
  ~nsXBLBinding();

  /*
   * Internal version. Requires that aCx is in appropriate xbl scope.
   */
  bool LookupMemberInternal(JSContext* aCx, nsString& aName,
                            JS::Handle<jsid> aNameAsId,
                            JS::MutableHandle<JS::PropertyDescriptor> aDesc,
                            JS::Handle<JSObject*> aXBLScope);

 public:
  void MarkForDeath();
  bool MarkedForDeath() const { return mMarkedForDeath; }

  bool ImplementsInterface(REFNSIID aIID) const;

  void GenerateAnonymousContent();
  void BindAnonymousContent(nsIContent* aAnonParent, nsIContent* aElement);
  static void UnbindAnonymousContent(mozilla::dom::Document* aDocument,
                                     nsIContent* aAnonParent,
                                     bool aNullParent = true);
  void InstallEventHandlers();
  nsresult InstallImplementation();

  void ExecuteAttachedHandler();
  void ExecuteDetachedHandler();
  void UnhookEventHandlers();

  nsXBLBinding* RootBinding();

  // Resolve all the fields for this binding and all ancestor bindings on the
  // object |obj|.  False return means a JS exception was set.
  bool ResolveAllFields(JSContext* cx, JS::Handle<JSObject*> obj) const;

  void AttributeChanged(nsAtom* aAttribute, int32_t aNameSpaceID,
                        bool aRemoveFlag, bool aNotify);

  void ChangeDocument(mozilla::dom::Document* aOldDocument,
                      mozilla::dom::Document* aNewDocument);

  const RawServoAuthorStyles* GetServoStyles() const;

  static nsresult DoInitJSClass(JSContext* cx, JS::Handle<JSObject*> obj,
                                const nsString& aClassName,
                                nsXBLPrototypeBinding* aProtoBinding,
                                JS::MutableHandle<JSObject*> aClassObject,
                                bool* aNew);

  bool AllowScripts();

  mozilla::dom::XBLChildrenElement* FindInsertionPointFor(nsIContent* aChild);

  bool HasFilteredInsertionPoints() { return !mInsertionPoints.IsEmpty(); }

  mozilla::dom::XBLChildrenElement* GetDefaultInsertionPoint() {
    return mDefaultInsertionPoint;
  }

  // Removes all inserted node from <xbl:children> insertion points under us.
  void ClearInsertionPoints();

  // Returns a live node list that iterates over the anonymous nodes generated
  // by this binding.
  nsAnonymousContentList* GetAnonymousNodeList();

  nsIURI* GetSourceDocURI();

  // MEMBER VARIABLES
 protected:
  bool mMarkedForDeath;

  nsXBLPrototypeBinding*
      mPrototypeBinding;  // Weak, but we're holding a ref to the docinfo
  nsCOMPtr<nsIContent>
      mContent;  // Strong. Our anonymous content stays around with us.
  RefPtr<nsXBLBinding> mNextBinding;  // Strong. The derived binding owns the
                                      // base class bindings.

  mozilla::dom::Element*
      mBoundElement;  // [WEAK] We have a reference, but we don't own it.

  // The <xbl:children> elements that we found in our <xbl:content> when we
  // processed this binding. The default insertion point has no includes
  // attribute and all other insertion points must have at least one includes
  // attribute. These points must be up-to-date with respect to their parent's
  // children, even if their parent has another binding attached to it,
  // preventing us from rendering their contents directly.
  RefPtr<mozilla::dom::XBLChildrenElement> mDefaultInsertionPoint;
  nsTArray<RefPtr<mozilla::dom::XBLChildrenElement> > mInsertionPoints;
  RefPtr<nsAnonymousContentList> mAnonymousContentList;

  mozilla::dom::XBLChildrenElement* FindInsertionPointForInternal(
      nsIContent* aChild);
};

#endif  // nsXBLBinding_h_
