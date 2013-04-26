/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set et sw=2 sts=2 cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLSharedObjectElement_h
#define mozilla_dom_HTMLSharedObjectElement_h

#include "nsGenericHTMLElement.h"
#include "nsObjectLoadingContent.h"
#include "nsGkAtoms.h"
#include "nsError.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsIDOMGetSVGDocument.h"

namespace mozilla {
namespace dom {

class HTMLSharedObjectElement : public nsGenericHTMLElement
                              , public nsObjectLoadingContent
                              , public nsIDOMHTMLAppletElement
                              , public nsIDOMHTMLEmbedElement
                              , public nsIDOMGetSVGDocument
{
public:
  HTMLSharedObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                          mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);
  virtual ~HTMLSharedObjectElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;

  // nsIDOMHTMLAppletElement
  NS_DECL_NSIDOMHTMLAPPLETELEMENT

  // Can't use macro for nsIDOMHTMLEmbedElement because it has conflicts with
  // NS_DECL_NSIDOMHTMLAPPLETELEMENT.

  // nsIDOMHTMLEmbedElement
  NS_IMETHOD GetSrc(nsAString &aSrc);
  NS_IMETHOD SetSrc(const nsAString &aSrc);
  NS_IMETHOD GetType(nsAString &aType);
  NS_IMETHOD SetType(const nsAString &aType);

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

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex);
  virtual IMEState GetDesiredIMEState();

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

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(HTMLSharedObjectElement,
                                                     nsGenericHTMLElement)

  virtual nsIDOMNode* AsDOMNode()
  {
    return static_cast<nsIDOMHTMLAppletElement*>(this);
  }

  // WebIDL API for <applet>
  void GetAlign(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::align, aValue);
  }
  void SetAlign(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::align, aValue, aRv);
  }
  void GetAlt(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::alt, aValue);
  }
  void SetAlt(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::alt, aValue, aRv);
  }
  void GetArchive(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::archive, aValue);
  }
  void SetArchive(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::archive, aValue, aRv);
  }
  void GetCode(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::code, aValue);
  }
  void SetCode(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::code, aValue, aRv);
  }
  // XPCOM GetCodebase is ok; note that it's a URI attribute
  void SetCodeBase(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::codebase, aValue, aRv);
  }
  void GetHeight(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::height, aValue);
  }
  void SetHeight(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::height, aValue, aRv);
  }
  uint32_t Hspace()
  {
    return GetUnsignedIntAttr(nsGkAtoms::hspace, 0);
  }
  void SetHspace(uint32_t aValue, ErrorResult& aRv)
  {
    SetUnsignedIntAttr(nsGkAtoms::hspace, aValue, aRv);
  }
  void GetName(DOMString& aValue)
  {
    GetHTMLAttr(nsGkAtoms::name, aValue);
  }
  void SetName(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::name, aValue, aRv);
  }
  // XPCOM GetObject is ok; note that it's a URI attribute with a weird base URI
  void SetObject(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::object, aValue, aRv);
  }
  uint32_t Vspace()
  {
    return GetUnsignedIntAttr(nsGkAtoms::vspace, 0);
  }
  void SetVspace(uint32_t aValue, ErrorResult& aRv)
  {
    SetUnsignedIntAttr(nsGkAtoms::vspace, aValue, aRv);
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
  // XPCOM GetSrc is ok; note that it's a URI attribute
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
  // width covered by <applet>
  // height covered by <applet>
  // align covered by <applet>
  // name covered by <applet>
  nsIDocument* GetSVGDocument()
  {
    return GetContentDocument();
  }

private:
  /**
   * Calls LoadObject with the correct arguments to start the plugin load.
   */
  NS_HIDDEN_(void) StartObjectLoad(bool aNotify);

  void GetTypeAttrValue(nsCString &aValue) const
  {
    if (mNodeInfo->Equals(nsGkAtoms::applet)) {
      aValue.AppendLiteral("application/x-java-vm");
    }
    else {
      nsAutoString type;
      GetAttr(kNameSpaceID_None, nsGkAtoms::type, type);

      CopyUTF16toUTF8(type, aValue);
    }
  }

  nsIAtom *URIAttrName() const
  {
    return mNodeInfo->Equals(nsGkAtoms::applet) ?
           nsGkAtoms::code :
           nsGkAtoms::src;
  }

  // mIsDoneAddingChildren is only really used for <applet>.  This boolean is
  // always true for <embed>, per the documentation in nsIContent.h.
  bool mIsDoneAddingChildren;

  virtual void GetItemValueText(nsAString& text);
  virtual void SetItemValueText(const nsAString& text);

  virtual JSObject* WrapNode(JSContext *aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLSharedObjectElement_h
