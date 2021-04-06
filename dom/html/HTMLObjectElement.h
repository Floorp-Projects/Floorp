/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLObjectElement_h
#define mozilla_dom_HTMLObjectElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsObjectLoadingContent.h"
#include "nsIConstraintValidation.h"

namespace mozilla {
namespace dom {

class HTMLFormSubmission;
template <typename T>
struct Nullable;
class WindowProxyHolder;

class HTMLObjectElement final : public nsGenericHTMLFormElement,
                                public nsObjectLoadingContent,
                                public nsIConstraintValidation {
 public:
  explicit HTMLObjectElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
      FromParser aFromParser = NOT_FROM_PARSER);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLObjectElement, object)
  virtual int32_t TabIndexDefault() override;

  // Element
  virtual bool IsInteractiveHTMLContent() const override;

  // EventTarget
  virtual void AsyncEventRunning(AsyncEventDispatcher* aEvent) override;

  virtual nsresult BindToTree(BindContext&, nsINode& aParent) override;
  virtual void UnbindFromTree(bool aNullParent = true) override;

  virtual bool IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                               int32_t* aTabIndex) override;

  // Overriden nsIFormControl methods
  NS_IMETHOD Reset() override { return NS_OK; }

  NS_IMETHOD SubmitNamesValues(HTMLFormSubmission* aFormSubmission) override {
    return NS_OK;
  }

  virtual void DoneAddingChildren(bool aHaveNotified) override;
  virtual bool IsDoneAddingChildren() override;

  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction()
      const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom* aAttribute) const override;
  virtual EventStates IntrinsicState() const override;
  virtual void DestroyContent() override;

  // nsObjectLoadingContent
  virtual uint32_t GetCapabilities() const override;

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  nsresult CopyInnerTo(Element* aDest);

  void StartObjectLoad() { StartObjectLoad(true, false); }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLObjectElement,
                                           nsGenericHTMLFormElement)

  // Web IDL binding methods
  void GetData(DOMString& aValue) {
    GetURIAttr(nsGkAtoms::data, nsGkAtoms::codebase, aValue);
  }
  void SetData(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::data, aValue, aRv);
  }
  void GetType(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::type, aValue); }
  void SetType(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::type, aValue, aRv);
  }
  void GetName(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::name, aValue); }
  void SetName(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::name, aValue, aRv);
  }
  void GetUseMap(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::usemap, aValue); }
  void SetUseMap(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::usemap, aValue, aRv);
  }
  using nsGenericHTMLFormElement::GetForm;
  void GetWidth(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::width, aValue); }
  void SetWidth(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::width, aValue, aRv);
  }
  void GetHeight(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::height, aValue); }
  void SetHeight(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::height, aValue, aRv);
  }
  using nsObjectLoadingContent::GetContentDocument;

  Nullable<WindowProxyHolder> GetContentWindow(nsIPrincipal& aSubjectPrincipal);

  using nsIConstraintValidation::GetValidationMessage;
  using nsIConstraintValidation::SetCustomValidity;
  void GetAlign(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::align, aValue); }
  void SetAlign(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::align, aValue, aRv);
  }
  void GetArchive(DOMString& aValue) {
    GetHTMLAttr(nsGkAtoms::archive, aValue);
  }
  void SetArchive(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::archive, aValue, aRv);
  }
  void GetCode(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::code, aValue); }
  void SetCode(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::code, aValue, aRv);
  }
  bool Declare() { return GetBoolAttr(nsGkAtoms::declare); }
  void SetDeclare(bool aValue, ErrorResult& aRv) {
    SetHTMLBoolAttr(nsGkAtoms::declare, aValue, aRv);
  }
  uint32_t Hspace() {
    return GetDimensionAttrAsUnsignedInt(nsGkAtoms::hspace, 0);
  }
  void SetHspace(uint32_t aValue, ErrorResult& aRv) {
    SetUnsignedIntAttr(nsGkAtoms::hspace, aValue, 0, aRv);
  }
  void GetStandby(DOMString& aValue) {
    GetHTMLAttr(nsGkAtoms::standby, aValue);
  }
  void SetStandby(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::standby, aValue, aRv);
  }
  uint32_t Vspace() {
    return GetDimensionAttrAsUnsignedInt(nsGkAtoms::vspace, 0);
  }
  void SetVspace(uint32_t aValue, ErrorResult& aRv) {
    SetUnsignedIntAttr(nsGkAtoms::vspace, aValue, 0, aRv);
  }
  void GetCodeBase(DOMString& aValue) {
    GetURIAttr(nsGkAtoms::codebase, nullptr, aValue);
  }
  void SetCodeBase(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::codebase, aValue, aRv);
  }
  void GetCodeType(DOMString& aValue) {
    GetHTMLAttr(nsGkAtoms::codetype, aValue);
  }
  void SetCodeType(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::codetype, aValue, aRv);
  }
  void GetBorder(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::border, aValue); }
  void SetBorder(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::border, aValue, aRv);
  }

  Document* GetSVGDocument(nsIPrincipal& aSubjectPrincipal) {
    return GetContentDocument(aSubjectPrincipal);
  }

  /**
   * Calls LoadObject with the correct arguments to start the plugin load.
   */
  void StartObjectLoad(bool aNotify, bool aForceLoad);

 protected:
  // Override for nsImageLoadingContent.
  nsIContent* AsContent() override { return this; }

  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;
  virtual nsresult OnAttrSetButNotChanged(int32_t aNamespaceID, nsAtom* aName,
                                          const nsAttrValueOrString& aValue,
                                          bool aNotify) override;

 private:
  nsContentPolicyType GetContentPolicyType() const override {
    return nsIContentPolicy::TYPE_INTERNAL_OBJECT;
  }

  virtual ~HTMLObjectElement();

  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    MappedDeclarations&);

  /**
   * This function is called by AfterSetAttr and OnAttrSetButNotChanged.
   * This function will be called by AfterSetAttr whether the attribute is being
   * set or unset.
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aNotify Whether we plan to notify document observers.
   */
  nsresult AfterMaybeChangeAttr(int32_t aNamespaceID, nsAtom* aName,
                                bool aNotify);

  bool mIsDoneAddingChildren;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_HTMLObjectElement_h
