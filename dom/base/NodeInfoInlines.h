/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NodeInfoInlines_h___
#define mozilla_dom_NodeInfoInlines_h___

#include "nsIAtom.h"
#include "nsIDOMNode.h"
#include "nsDOMString.h"
#include "nsGkAtoms.h"

namespace mozilla {
namespace dom {

inline bool
NodeInfo::Equals(NodeInfo *aNodeInfo) const
{
  return aNodeInfo == this || aNodeInfo->Equals(mInner.mName, mInner.mPrefix,
                                                mInner.mNamespaceID);
}

inline bool
NodeInfo::NameAndNamespaceEquals(NodeInfo *aNodeInfo) const
{
  return aNodeInfo == this || aNodeInfo->Equals(mInner.mName,
                                                mInner.mNamespaceID);
}

inline bool
NodeInfo::Equals(const nsAString& aName) const
{
  return mInner.mName->Equals(aName);
}

inline bool
NodeInfo::Equals(const nsAString& aName, const nsAString& aPrefix) const
{
  return mInner.mName->Equals(aName) &&
    (mInner.mPrefix ? mInner.mPrefix->Equals(aPrefix) : aPrefix.IsEmpty());
}

inline bool
NodeInfo::Equals(const nsAString& aName, int32_t aNamespaceID) const
{
  return mInner.mNamespaceID == aNamespaceID &&
    mInner.mName->Equals(aName);
}

inline bool
NodeInfo::Equals(const nsAString& aName, const nsAString& aPrefix,
                 int32_t aNamespaceID) const
{
  return mInner.mName->Equals(aName) && mInner.mNamespaceID == aNamespaceID &&
    (mInner.mPrefix ? mInner.mPrefix->Equals(aPrefix) : aPrefix.IsEmpty());
}

inline bool
NodeInfo::QualifiedNameEquals(nsIAtom* aNameAtom) const
{
  MOZ_ASSERT(aNameAtom, "Must have name atom");
  if (!GetPrefixAtom()) {
    return Equals(aNameAtom);
  }

  return aNameAtom->Equals(mQualifiedName);
}

} // namespace dom
} // namespace mozilla

inline void
CheckValidNodeInfo(uint16_t aNodeType, nsIAtom *aName, int32_t aNamespaceID,
                   nsIAtom* aExtraName)
{
  NS_ABORT_IF_FALSE(aNodeType == nsIDOMNode::ELEMENT_NODE ||
                    aNodeType == nsIDOMNode::ATTRIBUTE_NODE ||
                    aNodeType == nsIDOMNode::TEXT_NODE ||
                    aNodeType == nsIDOMNode::CDATA_SECTION_NODE ||
                    aNodeType == nsIDOMNode::PROCESSING_INSTRUCTION_NODE ||
                    aNodeType == nsIDOMNode::COMMENT_NODE ||
                    aNodeType == nsIDOMNode::DOCUMENT_NODE ||
                    aNodeType == nsIDOMNode::DOCUMENT_TYPE_NODE ||
                    aNodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE ||
                    aNodeType == UINT16_MAX,
                    "Invalid nodeType");
  NS_ABORT_IF_FALSE((aNodeType == nsIDOMNode::PROCESSING_INSTRUCTION_NODE ||
                     aNodeType == nsIDOMNode::DOCUMENT_TYPE_NODE) ==
                    !!aExtraName,
                    "Supply aExtraName for and only for PIs and doctypes");
  NS_ABORT_IF_FALSE(aNodeType == nsIDOMNode::ELEMENT_NODE ||
                    aNodeType == nsIDOMNode::ATTRIBUTE_NODE ||
                    aNodeType == UINT16_MAX ||
                    aNamespaceID == kNameSpaceID_None,
                    "Only attributes and elements can be in a namespace");
  NS_ABORT_IF_FALSE(aName && aName != nsGkAtoms::_empty, "Invalid localName");
  NS_ABORT_IF_FALSE(((aNodeType == nsIDOMNode::TEXT_NODE) ==
                     (aName == nsGkAtoms::textTagName)) &&
                    ((aNodeType == nsIDOMNode::CDATA_SECTION_NODE) ==
                     (aName == nsGkAtoms::cdataTagName)) &&
                    ((aNodeType == nsIDOMNode::COMMENT_NODE) ==
                     (aName == nsGkAtoms::commentTagName)) &&
                    ((aNodeType == nsIDOMNode::DOCUMENT_NODE) ==
                     (aName == nsGkAtoms::documentNodeName)) &&
                    ((aNodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) ==
                     (aName == nsGkAtoms::documentFragmentNodeName)) &&
                    ((aNodeType == nsIDOMNode::DOCUMENT_TYPE_NODE) ==
                     (aName == nsGkAtoms::documentTypeNodeName)) &&
                    ((aNodeType == nsIDOMNode::PROCESSING_INSTRUCTION_NODE) ==
                     (aName == nsGkAtoms::processingInstructionTagName)),
                    "Wrong localName for nodeType");
}

#endif /* mozilla_dom_NodeInfoInlines_h___ */