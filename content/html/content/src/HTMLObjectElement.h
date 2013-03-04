/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set et sw=2 sts=2 cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLObjectElement_h
#define mozilla_dom_HTMLObjectElement_h

#include "nsGenericHTMLElement.h"
#include "nsObjectLoadingContent.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIConstraintValidation.h"
#include "nsIDOMGetSVGDocument.h"

namespace mozilla {
namespace dom {

class HTMLObjectElement MOZ_FINAL : public nsGenericHTMLFormElement
                                  , public nsObjectLoadingContent
                                  , public nsIDOMHTMLObjectElement
                                  , public nsIConstraintValidation
                                  , public nsIDOMGetSVGDocument
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  HTMLObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                    FromParser aFromParser = NOT_FROM_PARSER);
  virtual ~HTMLObjectElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;

  // nsIDOMHTMLObjectElement
  NS_DECL_NSIDOMHTMLOBJECTELEMENT

  // nsIDOMGetSVGDocument
  NS_DECL_NSIDOMGETSVGDOCUMENT

  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom *aName,
                           nsIAtom *aPrefix, const nsAString &aValue,
                           bool aNotify);
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex);
  virtual IMEState GetDesiredIMEState();

  // Overriden nsIFormControl methods
  NS_IMETHOD_(uint32_t) GetType() const
  {
    return NS_FORM_OBJECT;
  }

  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission *aFormSubmission);

  virtual bool IsDisabled() const { return false; }

  virtual void DoneAddingChildren(bool aHaveNotified);
  virtual bool IsDoneAddingChildren();

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom *aAttribute,
                                const nsAString &aValue,
                                nsAttrValue &aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom *aAttribute) const;
  virtual nsEventStates IntrinsicState() const;
  virtual void DestroyContent();

  // nsObjectLoadingContent
  virtual uint32_t GetCapabilities() const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  nsresult CopyInnerTo(Element* aDest);

  void StartObjectLoad() { StartObjectLoad(true); }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLObjectElement,
                                           nsGenericHTMLFormElement)

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
private:
  /**
   * Calls LoadObject with the correct arguments to start the plugin load.
   */
  NS_HIDDEN_(void) StartObjectLoad(bool aNotify);

  /**
   * Returns if the element is currently focusable regardless of it's tabindex
   * value. This is used to know the default tabindex value.
   */
  bool IsFocusableForTabIndex();
  
  virtual void GetItemValueText(nsAString& text);
  virtual void SetItemValueText(const nsAString& text);

  bool mIsDoneAddingChildren;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLObjectElement_h
