/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLFieldSetElement_h
#define mozilla_dom_HTMLFieldSetElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLFieldSetElement.h"
#include "nsIConstraintValidation.h"
#include "mozilla/dom/HTMLFormElement.h"
#include "mozilla/dom/ValidityState.h"

namespace mozilla {
class EventChainPreVisitor;
namespace dom {

class HTMLFieldSetElement MOZ_FINAL : public nsGenericHTMLFormElement,
                                      public nsIDOMHTMLFieldSetElement,
                                      public nsIConstraintValidation
{
public:
  using nsGenericHTMLFormElement::GetForm;
  using nsIConstraintValidation::Validity;
  using nsIConstraintValidation::CheckValidity;
  using nsIConstraintValidation::GetValidationMessage;

  HTMLFieldSetElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  virtual ~HTMLFieldSetElement();

  NS_IMPL_FROMCONTENT_HTML_WITH_TAG(HTMLFieldSetElement, fieldset)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMHTMLFieldSetElement
  NS_DECL_NSIDOMHTMLFIELDSETELEMENT

  // nsIContent
  virtual nsresult PreHandleEvent(EventChainPreVisitor& aVisitor) MOZ_OVERRIDE;
  virtual nsresult AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify) MOZ_OVERRIDE;

  virtual nsresult InsertChildAt(nsIContent* aChild, uint32_t aIndex,
                                     bool aNotify) MOZ_OVERRIDE;
  virtual void RemoveChildAt(uint32_t aIndex, bool aNotify) MOZ_OVERRIDE;

  // nsIFormControl
  NS_IMETHOD_(uint32_t) GetType() const MOZ_OVERRIDE { return NS_FORM_FIELDSET; }
  NS_IMETHOD Reset() MOZ_OVERRIDE;
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission) MOZ_OVERRIDE;
  virtual bool IsDisabledForEvents(uint32_t aMessage) MOZ_OVERRIDE;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const MOZ_OVERRIDE;

  const nsIContent* GetFirstLegend() const { return mFirstLegend; }

  void AddElement(nsGenericHTMLFormElement* aElement);

  void RemoveElement(nsGenericHTMLFormElement* aElement);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLFieldSetElement,
                                           nsGenericHTMLFormElement)

  // WebIDL
  bool Disabled() const
  {
    return GetBoolAttr(nsGkAtoms::disabled);
  }
  void SetDisabled(bool aValue, ErrorResult& aRv)
  {
    SetHTMLBoolAttr(nsGkAtoms::disabled, aValue, aRv);
  }

  // XPCOM GetName is OK for us

  void SetName(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::name, aValue, aRv);
  }

  // XPCOM GetType is OK for us

  nsIHTMLCollection* Elements();

  // XPCOM WillValidate is OK for us

  // XPCOM Validity is OK for us

  // XPCOM GetValidationMessage is OK for us

  // XPCOM CheckValidity is OK for us

  // XPCOM SetCustomValidity is OK for us

  virtual EventStates IntrinsicState() const;


  /*
   * This method will update the fieldset's validity.  This method has to be
   * called by fieldset elements whenever their validity state or status regarding
   * constraint validation changes.
   *
   * @note If an element becomes barred from constraint validation, it has to
   * be considered as valid.
   *
   * @param aElementValidityState the new validity state of the element
   */
  void UpdateValidity(bool aElementValidityState);

protected:
  virtual JSObject* WrapNode(JSContext* aCx) MOZ_OVERRIDE;

private:

  /**
   * Notify all elements (in mElements) that the first legend of the fieldset
   * has now changed.
   */
  void NotifyElementsForFirstLegendChange(bool aNotify);

  // This function is used to generate the nsContentList (listed form elements).
  static bool MatchListedElements(nsIContent* aContent, int32_t aNamespaceID,
                                    nsIAtom* aAtom, void* aData);

  // listed form controls elements.
  nsRefPtr<nsContentList> mElements;

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

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLFieldSetElement_h */
