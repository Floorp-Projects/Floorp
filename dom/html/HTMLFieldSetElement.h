/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLFieldSetElement_h
#define mozilla_dom_HTMLFieldSetElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/ConstraintValidation.h"
#include "mozilla/dom/ValidityState.h"
#include "nsGenericHTMLElement.h"

namespace mozilla {
class ErrorResult;
class EventChainPreVisitor;
namespace dom {
class FormData;

class HTMLFieldSetElement final : public nsGenericHTMLFormControlElement,
                                  public ConstraintValidation {
 public:
  using ConstraintValidation::GetValidationMessage;
  using ConstraintValidation::SetCustomValidity;

  explicit HTMLFieldSetElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLFieldSetElement, fieldset)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIContent
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  virtual void InsertChildBefore(nsIContent* aChild, nsIContent* aBeforeThis,
                                 bool aNotify, ErrorResult& aRv) override;
  virtual void RemoveChildNode(nsIContent* aKid, bool aNotify) override;

  // nsGenericHTMLElement
  virtual bool IsDisabledForEvents(WidgetEvent* aEvent) override;

  // nsIFormControl
  NS_IMETHOD Reset() override;
  NS_IMETHOD SubmitNamesValues(FormData* aFormData) override { return NS_OK; }
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  const nsIContent* GetFirstLegend() const { return mFirstLegend; }

  void AddElement(nsGenericHTMLFormElement* aElement);

  void RemoveElement(nsGenericHTMLFormElement* aElement);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLFieldSetElement,
                                           nsGenericHTMLFormControlElement)

  // WebIDL
  bool Disabled() const { return GetBoolAttr(nsGkAtoms::disabled); }
  void SetDisabled(bool aValue, ErrorResult& aRv) {
    SetHTMLBoolAttr(nsGkAtoms::disabled, aValue, aRv);
  }

  void GetName(nsAString& aValue) { GetHTMLAttr(nsGkAtoms::name, aValue); }

  void SetName(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::name, aValue, aRv);
  }

  void GetType(nsAString& aType) const;

  nsIHTMLCollection* Elements();

  // XPCOM WillValidate is OK for us

  // XPCOM Validity is OK for us

  // XPCOM GetValidationMessage is OK for us

  // XPCOM CheckValidity is OK for us

  // XPCOM SetCustomValidity is OK for us

  virtual EventStates IntrinsicState() const override;

  /*
   * This method will update the fieldset's validity.  This method has to be
   * called by fieldset elements whenever their validity state or status
   * regarding constraint validation changes.
   *
   * @note If an element becomes barred from constraint validation, it has to
   * be considered as valid.
   *
   * @param aElementValidityState the new validity state of the element
   */
  void UpdateValidity(bool aElementValidityState);

 protected:
  virtual ~HTMLFieldSetElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

 private:
  /**
   * Notify all elements (in mElements) that the first legend of the fieldset
   * has now changed.
   */
  void NotifyElementsForFirstLegendChange(bool aNotify);

  // This function is used to generate the nsContentList (listed form elements).
  static bool MatchListedElements(Element* aElement, int32_t aNamespaceID,
                                  nsAtom* aAtom, void* aData);

  // listed form controls elements.
  RefPtr<nsContentList> mElements;

  // List of elements which have this fieldset as first fieldset ancestor.
  nsTArray<nsGenericHTMLFormElement*> mDependentElements;

  nsIContent* mFirstLegend;

  /**
   * Number of invalid and candidate for constraint validation
   * elements in the fieldSet the last time UpdateValidity has been called.
   *
   * @note Should only be used by UpdateValidity() and IntrinsicState()!
   */
  int32_t mInvalidElementsCount;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_HTMLFieldSetElement_h */
