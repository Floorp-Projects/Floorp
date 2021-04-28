/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdnAccessible-inl.h"
#include "ISimpleDOM_i.c"

#include "DocAccessibleWrap.h"

#include "nsAttrName.h"
#include "nsCoreUtils.h"
#include "nsIAccessibleTypes.h"
#include "nsICSSDeclaration.h"
#include "nsNameSpaceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsWinUtils.h"
#include "nsRange.h"

#include "mozilla/dom/BorrowedAttrInfo.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/PresShell.h"

using namespace mozilla;
using namespace mozilla::a11y;

sdnAccessible::~sdnAccessible() {
  if (mUniqueId.isSome()) {
    MsaaAccessible::ReleaseChildID(WrapNotNull(this));
  }
}

STDMETHODIMP
sdnAccessible::QueryInterface(REFIID aREFIID, void** aInstancePtr) {
  if (!aInstancePtr) return E_FAIL;
  *aInstancePtr = nullptr;

  if (aREFIID == IID_IClientSecurity) {
    // Some code might QI(IID_IClientSecurity) to detect whether or not we are
    // a proxy. Right now that can potentially happen off the main thread, so we
    // look for this condition immediately so that we don't trigger other code
    // that might not be thread-safe.
    return E_NOINTERFACE;
  }

  if (aREFIID == IID_ISimpleDOMNode) {
    *aInstancePtr = static_cast<ISimpleDOMNode*>(this);
    AddRef();
    return S_OK;
  }

  AccessibleWrap* accessible = GetAccessible();
  if (accessible) return accessible->QueryInterface(aREFIID, aInstancePtr);

  // IUnknown* is the canonical one if and only if this accessible doesn't have
  // an accessible.
  if (aREFIID == IID_IUnknown) {
    *aInstancePtr = static_cast<ISimpleDOMNode*>(this);
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

STDMETHODIMP
sdnAccessible::get_nodeInfo(BSTR __RPC_FAR* aNodeName,
                            short __RPC_FAR* aNameSpaceID,
                            BSTR __RPC_FAR* aNodeValue,
                            unsigned int __RPC_FAR* aNumChildren,
                            unsigned int __RPC_FAR* aUniqueID,
                            unsigned short __RPC_FAR* aNodeType) {
  if (!aNodeName || !aNameSpaceID || !aNodeValue || !aNumChildren ||
      !aUniqueID || !aNodeType)
    return E_INVALIDARG;

  *aNodeName = nullptr;
  *aNameSpaceID = 0;
  *aNodeValue = nullptr;
  *aNumChildren = 0;
  *aUniqueID = 0;
  *aNodeType = 0;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  uint16_t nodeType = mNode->NodeType();
  *aNodeType = static_cast<unsigned short>(nodeType);

  if (*aNodeType != NODETYPE_TEXT) {
    *aNodeName = ::SysAllocString(mNode->NodeName().get());
  }

  nsAutoString nodeValue;
  mNode->GetNodeValue(nodeValue);
  *aNodeValue = ::SysAllocString(nodeValue.get());

  *aNameSpaceID = mNode->IsContent()
                      ? static_cast<short>(mNode->AsContent()->GetNameSpaceID())
                      : 0;

  // This is a unique ID for every content node. The 3rd party accessibility
  // application can compare this to the childID we return for events such as
  // focus events, to correlate back to data nodes in their internal object
  // model.
  AccessibleWrap* accessible = GetAccessible();
  if (accessible) {
    *aUniqueID = MsaaAccessible::GetChildIDFor(accessible);
  } else {
    if (mUniqueId.isNothing()) {
      MsaaAccessible::AssignChildIDTo(WrapNotNull(this));
    }
    MOZ_ASSERT(mUniqueId.isSome());
    *aUniqueID = mUniqueId.value();
  }

  *aNumChildren = mNode->GetChildCount();

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_attributes(unsigned short aMaxAttribs,
                              BSTR __RPC_FAR* aAttribNames,
                              short __RPC_FAR* aNameSpaceIDs,
                              BSTR __RPC_FAR* aAttribValues,
                              unsigned short __RPC_FAR* aNumAttribs) {
  if (!aAttribNames || !aNameSpaceIDs || !aAttribValues || !aNumAttribs)
    return E_INVALIDARG;

  *aNumAttribs = 0;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  if (!mNode->IsElement()) return S_FALSE;

  dom::Element* elm = mNode->AsElement();
  uint32_t numAttribs = elm->GetAttrCount();
  if (numAttribs > aMaxAttribs) numAttribs = aMaxAttribs;

  *aNumAttribs = static_cast<unsigned short>(numAttribs);

  for (uint32_t index = 0; index < numAttribs; index++) {
    aNameSpaceIDs[index] = 0;
    aAttribValues[index] = aAttribNames[index] = nullptr;
    nsAutoString attributeValue;

    dom::BorrowedAttrInfo attr = elm->GetAttrInfoAt(index);
    attr.mValue->ToString(attributeValue);

    aNameSpaceIDs[index] = static_cast<short>(attr.mName->NamespaceID());
    aAttribNames[index] =
        ::SysAllocString(attr.mName->LocalName()->GetUTF16String());
    aAttribValues[index] = ::SysAllocString(attributeValue.get());
  }

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_attributesForNames(unsigned short aMaxAttribs,
                                      BSTR __RPC_FAR* aAttribNames,
                                      short __RPC_FAR* aNameSpaceID,
                                      BSTR __RPC_FAR* aAttribValues) {
  if (!aAttribNames || !aNameSpaceID || !aAttribValues) return E_INVALIDARG;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  if (!mNode->IsElement()) return S_FALSE;

  dom::Element* domElement = mNode->AsElement();
  nsNameSpaceManager* nameSpaceManager = nsNameSpaceManager::GetInstance();

  int32_t index = 0;
  for (index = 0; index < aMaxAttribs; index++) {
    aAttribValues[index] = nullptr;
    if (aAttribNames[index]) {
      nsAutoString attributeValue, nameSpaceURI;
      nsAutoString attributeName(
          nsDependentString(static_cast<const wchar_t*>(aAttribNames[index])));

      if (aNameSpaceID[index] > 0 &&
          NS_SUCCEEDED(nameSpaceManager->GetNameSpaceURI(aNameSpaceID[index],
                                                         nameSpaceURI))) {
        domElement->GetAttributeNS(nameSpaceURI, attributeName, attributeValue);
      } else {
        domElement->GetAttribute(attributeName, attributeValue);
      }

      aAttribValues[index] = ::SysAllocString(attributeValue.get());
    }
  }

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_computedStyle(
    unsigned short aMaxStyleProperties, boolean aUseAlternateView,
    BSTR __RPC_FAR* aStyleProperties, BSTR __RPC_FAR* aStyleValues,
    unsigned short __RPC_FAR* aNumStyleProperties) {
  if (!aStyleProperties || aStyleValues || !aNumStyleProperties)
    return E_INVALIDARG;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  *aNumStyleProperties = 0;

  if (mNode->IsDocument()) return S_FALSE;

  nsCOMPtr<nsICSSDeclaration> cssDecl =
      nsWinUtils::GetComputedStyleDeclaration(mNode->AsContent());
  NS_ENSURE_TRUE(cssDecl, E_FAIL);

  uint32_t length = cssDecl->Length();

  uint32_t index = 0, realIndex = 0;
  for (index = realIndex = 0; index < length && realIndex < aMaxStyleProperties;
       index++) {
    nsAutoCString property;
    nsAutoCString value;

    // Ignore -moz-* properties.
    cssDecl->Item(index, property);
    if (property.CharAt(0) != '-')
      cssDecl->GetPropertyValue(property, value);  // Get property value

    if (!value.IsEmpty()) {
      aStyleProperties[realIndex] =
          ::SysAllocString(NS_ConvertUTF8toUTF16(property).get());
      aStyleValues[realIndex] =
          ::SysAllocString(NS_ConvertUTF8toUTF16(value).get());
      ++realIndex;
    }
  }

  *aNumStyleProperties = static_cast<unsigned short>(realIndex);

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_computedStyleForProperties(
    unsigned short aNumStyleProperties, boolean aUseAlternateView,
    BSTR __RPC_FAR* aStyleProperties, BSTR __RPC_FAR* aStyleValues) {
  if (!aStyleProperties || !aStyleValues) return E_INVALIDARG;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  if (mNode->IsDocument()) return S_FALSE;

  nsCOMPtr<nsICSSDeclaration> cssDecl =
      nsWinUtils::GetComputedStyleDeclaration(mNode->AsContent());
  NS_ENSURE_TRUE(cssDecl, E_FAIL);

  uint32_t index = 0;
  for (index = 0; index < aNumStyleProperties; index++) {
    nsAutoCString value;
    if (aStyleProperties[index])
      cssDecl->GetPropertyValue(
          NS_ConvertUTF16toUTF8(nsDependentString(aStyleProperties[index])),
          value);  // Get property value
    aStyleValues[index] = ::SysAllocString(NS_ConvertUTF8toUTF16(value).get());
  }

  return S_OK;
}

// XXX Use MOZ_CAN_RUN_SCRIPT_BOUNDARY for now due to bug 1543294.
MOZ_CAN_RUN_SCRIPT_BOUNDARY STDMETHODIMP
sdnAccessible::scrollTo(boolean aScrollTopLeft) {
  DocAccessible* document = GetDocument();
  if (!document)  // that's IsDefunct check
    return CO_E_OBJNOTCONNECTED;

  if (!mNode->IsContent()) return S_FALSE;

  uint32_t scrollType = aScrollTopLeft
                            ? nsIAccessibleScrollType::SCROLL_TYPE_TOP_LEFT
                            : nsIAccessibleScrollType::SCROLL_TYPE_BOTTOM_RIGHT;

  RefPtr<PresShell> presShell = document->PresShellPtr();
  nsCOMPtr<nsIContent> content = mNode->AsContent();
  nsCoreUtils::ScrollTo(presShell, content, scrollType);
  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_parentNode(ISimpleDOMNode __RPC_FAR* __RPC_FAR* aNode) {
  if (!aNode) return E_INVALIDARG;
  *aNode = nullptr;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  nsINode* resultNode = mNode->GetParentNode();
  if (resultNode) {
    *aNode = static_cast<ISimpleDOMNode*>(new sdnAccessible(resultNode));
    (*aNode)->AddRef();
  }

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_firstChild(ISimpleDOMNode __RPC_FAR* __RPC_FAR* aNode) {
  if (!aNode) return E_INVALIDARG;
  *aNode = nullptr;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  nsINode* resultNode = mNode->GetFirstChild();
  if (resultNode) {
    *aNode = static_cast<ISimpleDOMNode*>(new sdnAccessible(resultNode));
    (*aNode)->AddRef();
  }

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_lastChild(ISimpleDOMNode __RPC_FAR* __RPC_FAR* aNode) {
  if (!aNode) return E_INVALIDARG;
  *aNode = nullptr;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  nsINode* resultNode = mNode->GetLastChild();
  if (resultNode) {
    *aNode = static_cast<ISimpleDOMNode*>(new sdnAccessible(resultNode));
    (*aNode)->AddRef();
  }

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_previousSibling(ISimpleDOMNode __RPC_FAR* __RPC_FAR* aNode) {
  if (!aNode) return E_INVALIDARG;
  *aNode = nullptr;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  nsINode* resultNode = mNode->GetPreviousSibling();
  if (resultNode) {
    *aNode = static_cast<ISimpleDOMNode*>(new sdnAccessible(resultNode));
    (*aNode)->AddRef();
  }

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_nextSibling(ISimpleDOMNode __RPC_FAR* __RPC_FAR* aNode) {
  if (!aNode) return E_INVALIDARG;
  *aNode = nullptr;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  nsINode* resultNode = mNode->GetNextSibling();
  if (resultNode) {
    *aNode = static_cast<ISimpleDOMNode*>(new sdnAccessible(resultNode));
    (*aNode)->AddRef();
  }

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_childAt(unsigned aChildIndex,
                           ISimpleDOMNode __RPC_FAR* __RPC_FAR* aNode) {
  if (!aNode) return E_INVALIDARG;
  *aNode = nullptr;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  nsINode* resultNode = mNode->GetChildAt_Deprecated(aChildIndex);
  if (resultNode) {
    *aNode = static_cast<ISimpleDOMNode*>(new sdnAccessible(resultNode));
    (*aNode)->AddRef();
  }

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_innerHTML(BSTR __RPC_FAR* aInnerHTML) {
  if (!aInnerHTML) return E_INVALIDARG;
  *aInnerHTML = nullptr;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  if (!mNode->IsElement()) return S_FALSE;

  nsAutoString innerHTML;
  mNode->AsElement()->GetInnerHTML(innerHTML, IgnoreErrors());
  if (innerHTML.IsEmpty()) return S_FALSE;

  *aInnerHTML = ::SysAllocStringLen(innerHTML.get(), innerHTML.Length());
  if (!*aInnerHTML) return E_OUTOFMEMORY;

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_localInterface(void __RPC_FAR* __RPC_FAR* aLocalInterface) {
  if (!aLocalInterface) return E_INVALIDARG;
  *aLocalInterface = nullptr;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  *aLocalInterface = this;
  AddRef();

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_language(BSTR __RPC_FAR* aLanguage) {
  if (!aLanguage) return E_INVALIDARG;
  *aLanguage = nullptr;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  nsAutoString language;
  if (mNode->IsContent())
    nsCoreUtils::GetLanguageFor(mNode->AsContent(), nullptr, language);
  if (language.IsEmpty()) {  // Nothing found, so use document's language
    mNode->OwnerDoc()->GetHeaderData(nsGkAtoms::headerContentLanguage,
                                     language);
  }

  if (language.IsEmpty()) return S_FALSE;

  *aLanguage = ::SysAllocStringLen(language.get(), language.Length());
  if (!*aLanguage) return E_OUTOFMEMORY;

  return S_OK;
}
