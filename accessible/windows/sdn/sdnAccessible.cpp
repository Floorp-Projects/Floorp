/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdnAccessible-inl.h"
#include "ISimpleDOMNode_i.c"

#include "DocAccessibleWrap.h"

#include "nsAttrName.h"
#include "nsCoreUtils.h"
#include "nsIAccessibleTypes.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsNameSpaceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsWinUtils.h"
#include "nsRange.h"

#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::a11y;

STDMETHODIMP
sdnAccessible::QueryInterface(REFIID aREFIID, void** aInstancePtr)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aInstancePtr)
    return E_FAIL;
  *aInstancePtr = nullptr;

  if (aREFIID == IID_ISimpleDOMNode) {
    *aInstancePtr = static_cast<ISimpleDOMNode*>(this);
    AddRef();
    return S_OK;
  }

  AccessibleWrap* accessible = static_cast<AccessibleWrap*>(GetAccessible());
  if (accessible)
    return accessible->QueryInterface(aREFIID, aInstancePtr);

  // IUnknown* is the canonical one if and only if this accessible doesn't have
  // an accessible.
  if (aREFIID == IID_IUnknown) {
    *aInstancePtr = static_cast<ISimpleDOMNode*>(this);
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_nodeInfo(BSTR __RPC_FAR* aNodeName,
                            short __RPC_FAR* aNameSpaceID,
                            BSTR __RPC_FAR* aNodeValue,
                            unsigned int __RPC_FAR* aNumChildren,
                            unsigned int __RPC_FAR* aUniqueID,
                            unsigned short __RPC_FAR* aNodeType)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNodeName || !aNameSpaceID || !aNodeValue || !aNumChildren ||
      !aUniqueID || !aNodeType)
    return E_INVALIDARG;

  *aNodeName = nullptr;
  *aNameSpaceID = 0;
  *aNodeValue = nullptr;
  *aNumChildren = 0;
  *aUniqueID = 0;
  *aNodeType = 0;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mNode));

  uint16_t nodeType = 0;
  DOMNode->GetNodeType(&nodeType);
  *aNodeType = static_cast<unsigned short>(nodeType);

  if (*aNodeType !=  NODETYPE_TEXT) {
    nsAutoString nodeName;
    DOMNode->GetNodeName(nodeName);
    *aNodeName = ::SysAllocString(nodeName.get());
  }

  nsAutoString nodeValue;
  DOMNode->GetNodeValue(nodeValue);
  *aNodeValue = ::SysAllocString(nodeValue.get());

  *aNameSpaceID = mNode->IsNodeOfType(nsINode::eCONTENT) ?
    static_cast<short>(mNode->AsContent()->GetNameSpaceID()) : 0;

  // This is a unique ID for every content node. The 3rd party accessibility
  // application can compare this to the childID we return for events such as
  // focus events, to correlate back to data nodes in their internal object
  // model.
  Accessible* accessible = GetAccessible();
  if (accessible) {
    *aUniqueID = AccessibleWrap::GetChildIDFor(accessible);
  } else {
    *aUniqueID = - NS_PTR_TO_INT32(static_cast<void*>(this));
  }

  *aNumChildren = mNode->GetChildCount();

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_attributes(unsigned  short aMaxAttribs,
                              BSTR __RPC_FAR* aAttribNames,
                              short __RPC_FAR* aNameSpaceIDs,
                              BSTR __RPC_FAR* aAttribValues,
                              unsigned short __RPC_FAR* aNumAttribs)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAttribNames || !aNameSpaceIDs || !aAttribValues || !aNumAttribs)
    return E_INVALIDARG;

  *aNumAttribs = 0;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!mNode->IsElement())
    return S_FALSE;

  dom::Element* elm = mNode->AsElement();
  uint32_t numAttribs = elm->GetAttrCount();
  if (numAttribs > aMaxAttribs)
    numAttribs = aMaxAttribs;

  *aNumAttribs = static_cast<unsigned short>(numAttribs);

  for (uint32_t index = 0; index < numAttribs; index++) {
    aNameSpaceIDs[index] = 0;
    aAttribValues[index] = aAttribNames[index] = nullptr;
    nsAutoString attributeValue;

    const nsAttrName* name = elm->GetAttrNameAt(index);
    aNameSpaceIDs[index] = static_cast<short>(name->NamespaceID());
    aAttribNames[index] = ::SysAllocString(name->LocalName()->GetUTF16String());
    elm->GetAttr(name->NamespaceID(), name->LocalName(), attributeValue);
    aAttribValues[index] = ::SysAllocString(attributeValue.get());
  }

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_attributesForNames(unsigned short aMaxAttribs,
                                      BSTR __RPC_FAR* aAttribNames,
                                      short __RPC_FAR* aNameSpaceID,
                                      BSTR __RPC_FAR* aAttribValues)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aAttribNames || !aNameSpaceID || !aAttribValues)
    return E_INVALIDARG;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!mNode->IsElement())
    return S_FALSE;

  nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(mNode));
  nsNameSpaceManager* nameSpaceManager = nsNameSpaceManager::GetInstance();

  int32_t index = 0;
  for (index = 0; index < aMaxAttribs; index++) {
    aAttribValues[index] = nullptr;
    if (aAttribNames[index]) {
      nsAutoString attributeValue, nameSpaceURI;
      nsAutoString attributeName(nsDependentString(
        static_cast<const wchar_t*>(aAttribNames[index])));

      nsresult rv = NS_OK;
      if (aNameSpaceID[index]>0 &&
        NS_SUCCEEDED(nameSpaceManager->GetNameSpaceURI(aNameSpaceID[index],
                                                       nameSpaceURI))) {
          rv = domElement->GetAttributeNS(nameSpaceURI, attributeName,
                                          attributeValue);
      } else {
        rv = domElement->GetAttribute(attributeName, attributeValue);
      }

      if (NS_SUCCEEDED(rv))
        aAttribValues[index] = ::SysAllocString(attributeValue.get());
    }
  }

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_computedStyle(unsigned short aMaxStyleProperties,
                                 boolean aUseAlternateView,
                                 BSTR __RPC_FAR* aStyleProperties,
                                 BSTR __RPC_FAR* aStyleValues,
                                 unsigned short __RPC_FAR* aNumStyleProperties)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aStyleProperties || aStyleValues || !aNumStyleProperties)
    return E_INVALIDARG;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  *aNumStyleProperties = 0;

  if (mNode->IsNodeOfType(nsINode::eDOCUMENT))
    return S_FALSE;

  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl =
    nsWinUtils::GetComputedStyleDeclaration(mNode->AsContent());
  NS_ENSURE_TRUE(cssDecl, E_FAIL);

  uint32_t length = 0;
  cssDecl->GetLength(&length);

  uint32_t index = 0, realIndex = 0;
  for (index = realIndex = 0; index < length && realIndex < aMaxStyleProperties;
       index ++) {
    nsAutoString property, value;

    // Ignore -moz-* properties.
    if (NS_SUCCEEDED(cssDecl->Item(index, property)) && property.CharAt(0) != '-')
      cssDecl->GetPropertyValue(property, value);  // Get property value

    if (!value.IsEmpty()) {
      aStyleProperties[realIndex] = ::SysAllocString(property.get());
      aStyleValues[realIndex] = ::SysAllocString(value.get());
      ++realIndex;
    }
  }

  *aNumStyleProperties = static_cast<unsigned short>(realIndex);

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_computedStyleForProperties(unsigned short aNumStyleProperties,
                                              boolean aUseAlternateView,
                                              BSTR __RPC_FAR* aStyleProperties,
                                              BSTR __RPC_FAR* aStyleValues)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aStyleProperties || !aStyleValues)
    return E_INVALIDARG;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (mNode->IsNodeOfType(nsINode::eDOCUMENT))
    return S_FALSE;

  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl =
    nsWinUtils::GetComputedStyleDeclaration(mNode->AsContent());
  NS_ENSURE_TRUE(cssDecl, E_FAIL);

  uint32_t index = 0;
  for (index = 0; index < aNumStyleProperties; index++) {
    nsAutoString value;
    if (aStyleProperties[index])
      cssDecl->GetPropertyValue(nsDependentString(aStyleProperties[index]), value);  // Get property value
    aStyleValues[index] = ::SysAllocString(value.get());
  }

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::scrollTo(boolean aScrollTopLeft)
{
  A11Y_TRYBLOCK_BEGIN

  DocAccessible* document = GetDocument();
  if (!document) // that's IsDefunct check
    return CO_E_OBJNOTCONNECTED;

  if (!mNode->IsContent())
    return S_FALSE;

  uint32_t scrollType =
    aScrollTopLeft ? nsIAccessibleScrollType::SCROLL_TYPE_TOP_LEFT :
                     nsIAccessibleScrollType::SCROLL_TYPE_BOTTOM_RIGHT;

  nsCoreUtils::ScrollTo(document->PresShell(), mNode->AsContent(), scrollType);
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_parentNode(ISimpleDOMNode __RPC_FAR *__RPC_FAR* aNode)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNode)
    return E_INVALIDARG;
  *aNode = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsINode* resultNode = mNode->GetParentNode();
  if (resultNode) {
    *aNode = static_cast<ISimpleDOMNode*>(new sdnAccessible(resultNode));
    (*aNode)->AddRef();
  }

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_firstChild(ISimpleDOMNode __RPC_FAR *__RPC_FAR* aNode)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNode)
    return E_INVALIDARG;
  *aNode = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsINode* resultNode = mNode->GetFirstChild();
  if (resultNode) {
    *aNode = static_cast<ISimpleDOMNode*>(new sdnAccessible(resultNode));
    (*aNode)->AddRef();
  }

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_lastChild(ISimpleDOMNode __RPC_FAR *__RPC_FAR* aNode)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNode)
    return E_INVALIDARG;
  *aNode = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsINode* resultNode = mNode->GetLastChild();
  if (resultNode) {
    *aNode = static_cast<ISimpleDOMNode*>(new sdnAccessible(resultNode));
    (*aNode)->AddRef();
  }

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_previousSibling(ISimpleDOMNode __RPC_FAR *__RPC_FAR* aNode)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNode)
    return E_INVALIDARG;
  *aNode = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsINode* resultNode = mNode->GetPreviousSibling();
  if (resultNode) {
    *aNode = static_cast<ISimpleDOMNode*>(new sdnAccessible(resultNode));
    (*aNode)->AddRef();
  }

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_nextSibling(ISimpleDOMNode __RPC_FAR *__RPC_FAR* aNode)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNode)
    return E_INVALIDARG;
  *aNode = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsINode* resultNode = mNode->GetNextSibling();
  if (resultNode) {
    *aNode = static_cast<ISimpleDOMNode*>(new sdnAccessible(resultNode));
    (*aNode)->AddRef();
  }

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_childAt(unsigned aChildIndex,
                           ISimpleDOMNode __RPC_FAR *__RPC_FAR* aNode)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aNode)
    return E_INVALIDARG;
  *aNode = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsINode* resultNode = mNode->GetChildAt(aChildIndex);
  if (resultNode) {
    *aNode = static_cast<ISimpleDOMNode*>(new sdnAccessible(resultNode));
    (*aNode)->AddRef();
  }


  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_innerHTML(BSTR __RPC_FAR* aInnerHTML)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aInnerHTML)
    return E_INVALIDARG;
  *aInnerHTML = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  if (!mNode->IsElement())
    return S_FALSE;

  nsAutoString innerHTML;
  mNode->AsElement()->GetInnerHTML(innerHTML);
  if (innerHTML.IsEmpty())
    return S_FALSE;

  *aInnerHTML = ::SysAllocStringLen(innerHTML.get(), innerHTML.Length());
  if (!*aInnerHTML)
    return E_OUTOFMEMORY;

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_localInterface(void __RPC_FAR *__RPC_FAR* aLocalInterface)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aLocalInterface)
    return E_INVALIDARG;
  *aLocalInterface = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  *aLocalInterface = this;
  AddRef();

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
sdnAccessible::get_language(BSTR __RPC_FAR* aLanguage)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aLanguage)
    return E_INVALIDARG;
  *aLanguage = nullptr;

  if (IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsAutoString language;
  if (mNode->IsContent())
    nsCoreUtils::GetLanguageFor(mNode->AsContent(), nullptr, language);
  if (language.IsEmpty()) { // Nothing found, so use document's language
    mNode->OwnerDoc()->GetHeaderData(nsGkAtoms::headerContentLanguage,
                                     language);
  }

  if (language.IsEmpty())
    return S_FALSE;

  *aLanguage = ::SysAllocStringLen(language.get(), language.Length());
  if (!*aLanguage)
   return E_OUTOFMEMORY;

  return S_OK;

  A11Y_TRYBLOCK_END
}
