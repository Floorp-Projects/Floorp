/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsIDOMHTMLStyleElement.h"
#include "nsIDOMLinkStyle.h"
#include "nsIDOMEventTarget.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIDOMStyleSheet.h"
#include "nsIStyleSheet.h"
#include "nsStyleLinkElement.h"
#include "nsNetUtil.h"
#include "nsIDocument.h"
#include "nsUnicharUtils.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsStubMutationObserver.h"

using namespace mozilla;
using namespace mozilla::dom;

class nsHTMLStyleElement : public nsGenericHTMLElement,
                           public nsIDOMHTMLStyleElement,
                           public nsStyleLinkElement,
                           public nsStubMutationObserver
{
public:
  nsHTMLStyleElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsHTMLStyleElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // CC
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsHTMLStyleElement,
                                           nsGenericHTMLElement)

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  virtual void GetInnerHTML(nsAString& aInnerHTML,
                            mozilla::ErrorResult& aError) MOZ_OVERRIDE;
  virtual void SetInnerHTML(const nsAString& aInnerHTML,
                            mozilla::ErrorResult& aError) MOZ_OVERRIDE;

  // nsIDOMHTMLStyleElement
  NS_DECL_NSIDOMHTMLSTYLEELEMENT

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:
  already_AddRefed<nsIURI> GetStyleSheetURL(bool* aIsInline);
  void GetStyleSheetInfo(nsAString& aTitle,
                         nsAString& aType,
                         nsAString& aMedia,
                         bool* aIsScoped,
                         bool* aIsAlternate);
  /**
   * Common method to call from the various mutation observer methods.
   * aContent is a content node that's either the one that changed or its
   * parent; we should only respond to the change if aContent is non-anonymous.
   */
  void ContentChanged(nsIContent* aContent);
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Style)


nsHTMLStyleElement::nsHTMLStyleElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
  AddMutationObserver(this);
}

nsHTMLStyleElement::~nsHTMLStyleElement()
{
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLStyleElement,
                                                  nsGenericHTMLElement)
  tmp->nsStyleLinkElement::Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHTMLStyleElement,
                                                nsGenericHTMLElement)
  tmp->nsStyleLinkElement::Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(nsHTMLStyleElement, Element)
NS_IMPL_RELEASE_INHERITED(nsHTMLStyleElement, Element)


DOMCI_NODE_DATA(HTMLStyleElement, nsHTMLStyleElement)

// QueryInterface implementation for nsHTMLStyleElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLStyleElement)
  NS_HTML_CONTENT_INTERFACE_TABLE4(nsHTMLStyleElement,
                                   nsIDOMHTMLStyleElement,
                                   nsIDOMLinkStyle,
                                   nsIStyleSheetLinkingElement,
                                   nsIMutationObserver)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLStyleElement,
                                               nsGenericHTMLElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLStyleElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLStyleElement)


NS_IMETHODIMP
nsHTMLStyleElement::GetDisabled(bool* aDisabled)
{
  nsCOMPtr<nsIDOMStyleSheet> ss = do_QueryInterface(GetStyleSheet());
  if (!ss) {
    *aDisabled = false;
    return NS_OK;
  }

  return ss->GetDisabled(aDisabled);
}

NS_IMETHODIMP 
nsHTMLStyleElement::SetDisabled(bool aDisabled)
{
  nsCOMPtr<nsIDOMStyleSheet> ss = do_QueryInterface(GetStyleSheet());
  if (!ss) {
    return NS_OK;
  }

  return ss->SetDisabled(aDisabled);
}

NS_IMPL_STRING_ATTR(nsHTMLStyleElement, Media, media)
NS_IMPL_BOOL_ATTR(nsHTMLStyleElement, Scoped, scoped)
NS_IMPL_STRING_ATTR(nsHTMLStyleElement, Type, type)

void
nsHTMLStyleElement::CharacterDataChanged(nsIDocument* aDocument,
                                         nsIContent* aContent,
                                         CharacterDataChangeInfo* aInfo)
{
  ContentChanged(aContent);
}

void
nsHTMLStyleElement::ContentAppended(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aFirstNewContent,
                                    int32_t aNewIndexInContainer)
{
  ContentChanged(aContainer);
}

void
nsHTMLStyleElement::ContentInserted(nsIDocument* aDocument,
                                    nsIContent* aContainer,
                                    nsIContent* aChild,
                                    int32_t aIndexInContainer)
{
  ContentChanged(aChild);
}

void
nsHTMLStyleElement::ContentRemoved(nsIDocument* aDocument,
                                   nsIContent* aContainer,
                                   nsIContent* aChild,
                                   int32_t aIndexInContainer,
                                   nsIContent* aPreviousSibling)
{
  ContentChanged(aChild);
}

void
nsHTMLStyleElement::ContentChanged(nsIContent* aContent)
{
  if (nsContentUtils::IsInSameAnonymousTree(this, aContent)) {
    UpdateStyleSheetInternal(nullptr);
  }
}

nsresult
nsHTMLStyleElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  void (nsHTMLStyleElement::*update)() = &nsHTMLStyleElement::UpdateStyleSheetInternal;
  nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(this, update));

  return rv;  
}

void
nsHTMLStyleElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  nsCOMPtr<nsIDocument> oldDoc = GetCurrentDoc();

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
  UpdateStyleSheetInternal(oldDoc);
}

nsresult
nsHTMLStyleElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                            nsIAtom* aPrefix, const nsAString& aValue,
                            bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);
  if (NS_SUCCEEDED(rv) && aNameSpaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::title ||
        aName == nsGkAtoms::media ||
        aName == nsGkAtoms::type) {
      UpdateStyleSheetInternal(nullptr, true);
    } else if (aName == nsGkAtoms::scoped) {
      UpdateStyleSheetScopedness(true);
    }
  }

  return rv;
}

nsresult
nsHTMLStyleElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                              bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute,
                                                aNotify);
  if (NS_SUCCEEDED(rv) && aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::title ||
        aAttribute == nsGkAtoms::media ||
        aAttribute == nsGkAtoms::type) {
      UpdateStyleSheetInternal(nullptr, true);
    } else if (aAttribute == nsGkAtoms::scoped) {
      UpdateStyleSheetScopedness(false);
    }
  }

  return rv;
}

void
nsHTMLStyleElement::GetInnerHTML(nsAString& aInnerHTML, ErrorResult& aError)
{
  nsContentUtils::GetNodeTextContent(this, false, aInnerHTML);
}

void
nsHTMLStyleElement::SetInnerHTML(const nsAString& aInnerHTML,
                                 ErrorResult& aError)
{
  SetEnableUpdates(false);

  aError = nsContentUtils::SetNodeTextContent(this, aInnerHTML, true);

  SetEnableUpdates(true);

  UpdateStyleSheetInternal(nullptr);
}

already_AddRefed<nsIURI>
nsHTMLStyleElement::GetStyleSheetURL(bool* aIsInline)
{
  *aIsInline = true;
  return nullptr;
}

void
nsHTMLStyleElement::GetStyleSheetInfo(nsAString& aTitle,
                                      nsAString& aType,
                                      nsAString& aMedia,
                                      bool* aIsScoped,
                                      bool* aIsAlternate)
{
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsAlternate = false;

  nsAutoString title;
  GetAttr(kNameSpaceID_None, nsGkAtoms::title, title);
  title.CompressWhitespace();
  aTitle.Assign(title);

  GetAttr(kNameSpaceID_None, nsGkAtoms::media, aMedia);
  // The HTML5 spec is formulated in terms of the CSSOM spec, which specifies
  // that media queries should be ASCII lowercased during serialization.
  nsContentUtils::ASCIIToLower(aMedia);

  GetAttr(kNameSpaceID_None, nsGkAtoms::type, aType);

  *aIsScoped = HasAttr(kNameSpaceID_None, nsGkAtoms::scoped);

  nsAutoString mimeType;
  nsAutoString notUsed;
  nsContentUtils::SplitMimeType(aType, mimeType, notUsed);
  if (!mimeType.IsEmpty() && !mimeType.LowerCaseEqualsLiteral("text/css")) {
    return;
  }

  // If we get here we assume that we're loading a css file, so set the
  // type to 'text/css'
  aType.AssignLiteral("text/css");
}
