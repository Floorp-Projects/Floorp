/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NodeInfoInlines_h___
#define mozilla_dom_NodeInfoInlines_h___

#include "nsAtom.h"
#include "nsINode.h"
#include "nsDOMString.h"
#include "nsGkAtoms.h"

namespace mozilla::dom {

inline bool NodeInfo::Equals(NodeInfo* aNodeInfo) const {
  return aNodeInfo == this ||
         aNodeInfo->Equals(mInner.mName, mInner.mPrefix, mInner.mNamespaceID);
}

inline bool NodeInfo::NameAndNamespaceEquals(NodeInfo* aNodeInfo) const {
  return aNodeInfo == this ||
         aNodeInfo->Equals(mInner.mName, mInner.mNamespaceID);
}

inline bool NodeInfo::Equals(const nsAString& aName) const {
  return mInner.mName->Equals(aName);
}

inline bool NodeInfo::Equals(const nsAString& aName,
                             const nsAString& aPrefix) const {
  return mInner.mName->Equals(aName) &&
         (mInner.mPrefix ? mInner.mPrefix->Equals(aPrefix) : aPrefix.IsEmpty());
}

inline bool NodeInfo::Equals(const nsAString& aName,
                             int32_t aNamespaceID) const {
  return mInner.mNamespaceID == aNamespaceID && mInner.mName->Equals(aName);
}

inline bool NodeInfo::Equals(const nsAString& aName, const nsAString& aPrefix,
                             int32_t aNamespaceID) const {
  return mInner.mName->Equals(aName) && mInner.mNamespaceID == aNamespaceID &&
         (mInner.mPrefix ? mInner.mPrefix->Equals(aPrefix) : aPrefix.IsEmpty());
}

inline bool NodeInfo::QualifiedNameEquals(const nsAtom* aNameAtom) const {
  MOZ_ASSERT(aNameAtom, "Must have name atom");
  if (!GetPrefixAtom()) {
    return Equals(aNameAtom);
  }

  return aNameAtom->Equals(mQualifiedName);
}

}  // namespace mozilla::dom

inline void CheckValidNodeInfo(uint16_t aNodeType, const nsAtom* aName,
                               int32_t aNamespaceID, const nsAtom* aExtraName) {
  MOZ_ASSERT(
      aNodeType == nsINode::ELEMENT_NODE ||
          aNodeType == nsINode::ATTRIBUTE_NODE ||
          aNodeType == nsINode::TEXT_NODE ||
          aNodeType == nsINode::CDATA_SECTION_NODE ||
          aNodeType == nsINode::PROCESSING_INSTRUCTION_NODE ||
          aNodeType == nsINode::COMMENT_NODE ||
          aNodeType == nsINode::DOCUMENT_NODE ||
          aNodeType == nsINode::DOCUMENT_TYPE_NODE ||
          aNodeType == nsINode::DOCUMENT_FRAGMENT_NODE ||
          aNodeType == UINT16_MAX,
      // If a new type is added here, please update nsINode::MAX_NODE_TYPE and
      // NodeTypeStrings in nsINode.cpp accordingly.  Note that UINT16_MAX is
      // only used for XUL prototype nodeinfos, which are never going to show up
      // where NodeTypeStrings is used.
      "Invalid nodeType");
  MOZ_ASSERT((aNodeType == nsINode::PROCESSING_INSTRUCTION_NODE ||
              aNodeType == nsINode::DOCUMENT_TYPE_NODE) == !!aExtraName,
             "Supply aExtraName for and only for PIs and doctypes");
  MOZ_ASSERT(aNodeType == nsINode::ELEMENT_NODE ||
                 aNodeType == nsINode::ATTRIBUTE_NODE ||
                 aNodeType == UINT16_MAX || aNamespaceID == kNameSpaceID_None,
             "Only attributes and elements can be in a namespace");
  MOZ_ASSERT(aName && aName != nsGkAtoms::_empty, "Invalid localName");
  MOZ_ASSERT(((aNodeType == nsINode::TEXT_NODE) ==
              (aName == nsGkAtoms::textTagName)) &&
                 ((aNodeType == nsINode::CDATA_SECTION_NODE) ==
                  (aName == nsGkAtoms::cdataTagName)) &&
                 ((aNodeType == nsINode::COMMENT_NODE) ==
                  (aName == nsGkAtoms::commentTagName)) &&
                 ((aNodeType == nsINode::DOCUMENT_NODE) ==
                  (aName == nsGkAtoms::documentNodeName)) &&
                 ((aNodeType == nsINode::DOCUMENT_FRAGMENT_NODE) ==
                  (aName == nsGkAtoms::documentFragmentNodeName)) &&
                 ((aNodeType == nsINode::DOCUMENT_TYPE_NODE) ==
                  (aName == nsGkAtoms::documentTypeNodeName)) &&
                 ((aNodeType == nsINode::PROCESSING_INSTRUCTION_NODE) ==
                  (aName == nsGkAtoms::processingInstructionTagName)),
             "Wrong localName for nodeType");
}

#endif /* mozilla_dom_NodeInfoInlines_h___ */
