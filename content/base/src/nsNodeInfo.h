/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Class that represents a prefix/namespace/localName triple; a single
 * nodeinfo is shared by all elements in a document that have that
 * prefix, namespace, and localName.
 */

#ifndef nsNodeInfo_h___
#define nsNodeInfo_h___

#include "mozilla/Attributes.h"
#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
#include "plhash.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsGkAtoms.h"

class nsNodeInfo : public nsINodeInfo
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_CLASS(nsNodeInfo)

  // nsINodeInfo
  virtual void GetNamespaceURI(nsAString& aNameSpaceURI) const;
  virtual bool NamespaceEquals(const nsAString& aNamespaceURI) const MOZ_OVERRIDE;

  // nsNodeInfo
public:
  /*
   * aName and aOwnerManager may not be null.
   */
  nsNodeInfo(nsIAtom *aName, nsIAtom *aPrefix, int32_t aNamespaceID,
             uint16_t aNodeType, nsIAtom *aExtraName,
             nsNodeInfoManager *aOwnerManager);

private:
  nsNodeInfo(); // Unimplemented
  nsNodeInfo(const nsNodeInfo& aOther); // Unimplemented
protected:
  virtual ~nsNodeInfo();

public:
  bool CanSkip();

private:
  /**
   * This method gets called by Release() when it's time to delete
   * this object.
   */
  void LastRelease();
};

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

#endif /* nsNodeInfo_h___ */
