/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLEmbedElement_h
#define mozilla_dom_HTMLEmbedElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsObjectLoadingContent.h"
#include "nsGkAtoms.h"
#include "nsError.h"

namespace mozilla {
namespace dom {

class HTMLEmbedElement final : public nsGenericHTMLElement
                             , public nsObjectLoadingContent
{
public:
  explicit HTMLEmbedElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                            mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLEmbedElement, embed)
  virtual int32_t TabIndexDefault() override;

#ifdef XP_MACOSX
  // EventTarget
  NS_IMETHOD PostHandleEvent(EventChainPostVisitor& aVisitor) override;
#endif

  // EventTarget
  virtual void AsyncEventRunning(AsyncEventDispatcher* aEvent) override;

  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex) override;
  virtual IMEState GetDesiredIMEState() override;

  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsAtom *aAttribute,
                              const nsAString &aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue &aResult) override;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const override;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsAtom *aAttribute) const override;
  virtual EventStates IntrinsicState() const override;
  virtual void DestroyContent() override;

  // nsObjectLoadingContent
  virtual uint32_t GetCapabilities() const override;

  virtual nsresult Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                         bool aPreallocateChildren) const override;

  nsresult CopyInnerTo(Element* aDest, bool aPreallocateChildren);

  void StartObjectLoad() { StartObjectLoad(true, false); }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(HTMLEmbedElement,
                                                     nsGenericHTMLElement)

  // WebIDL <embed> api
  void GetAlign(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::align, aValue);
  }
  void SetAlign(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::align, aValue, aRv);
  }
  void GetHeight(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::height, aValue);
  }
  void SetHeight(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::height, aValue, aRv);
  }
  void GetName(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::name, aValue);
  }
  void SetName(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::name, aValue, aRv);
  }
  void GetWidth(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::width, aValue);
  }
  void SetWidth(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::width, aValue, aRv);
  }
  // WebIDL <embed> api
  void GetSrc(DOMString& aValue)
  {
    GetURIAttr(nsGkAtoms::src, nullptr, aValue);
  }
  void SetSrc(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::src, aValue, aRv);
  }
  void GetType(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::type, aValue);
  }
  void SetType(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::type, aValue, aRv);
  }
  nsIDocument*
  GetSVGDocument(nsIPrincipal& aSubjectPrincipal)
  {
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
  ~HTMLEmbedElement();

  nsContentPolicyType GetContentPolicyType() const override;

  virtual JSObject* WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override;

  static void MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                    MappedDeclarations&);

  /**
   * This function is called by AfterSetAttr and OnAttrSetButNotChanged.
   * It will not be called if the value is being unset.
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aNotify Whether we plan to notify document observers.
   */
  nsresult AfterMaybeChangeAttr(int32_t aNamespaceID, nsAtom* aName,
                                bool aNotify);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLEmbedElement_h
