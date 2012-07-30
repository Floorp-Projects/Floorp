/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
#include "plhash.h"
#include "nsIAtom.h"
#include "nsCOMPtr.h"

class nsFixedSizeAllocator;

class nsNodeInfo : public nsINodeInfo
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsNodeInfo)

  // nsINodeInfo
  virtual nsresult GetNamespaceURI(nsAString& aNameSpaceURI) const;
  virtual bool NamespaceEquals(const nsAString& aNamespaceURI) const;

  // nsNodeInfo
  // Create objects with Create
public:
  /*
   * aName and aOwnerManager may not be null.
   */
  static nsNodeInfo *Create(nsIAtom *aName, nsIAtom *aPrefix,
                            PRInt32 aNamespaceID, PRUint16 aNodeType,
                            nsIAtom *aExtraName,
                            nsNodeInfoManager *aOwnerManager);
private:
  nsNodeInfo(); // Unimplemented
  nsNodeInfo(const nsNodeInfo& aOther); // Unimplemented
  nsNodeInfo(nsIAtom *aName, nsIAtom *aPrefix, PRInt32 aNamespaceID,
             PRUint16 aNodeType, nsIAtom *aExtraName,
             nsNodeInfoManager *aOwnerManager);
protected:
  virtual ~nsNodeInfo();

public:
  /**
   * Call before shutdown to clear the cache and free memory for this class.
   */
  static void ClearCache();

private:
  static nsFixedSizeAllocator* sNodeInfoPool;

  /**
   * This method gets called by Release() when it's time to delete 
   * this object, instead of always deleting the object we'll put the
   * object in the cache unless the cache is already full.
   */
   void LastRelease();
};

#define CHECK_VALID_NODEINFO(_nodeType, _name, _namespaceID, _extraName)    \
NS_ABORT_IF_FALSE(_nodeType == nsIDOMNode::ELEMENT_NODE ||                  \
                  _nodeType == nsIDOMNode::ATTRIBUTE_NODE ||                \
                  _nodeType == nsIDOMNode::TEXT_NODE ||                     \
                  _nodeType == nsIDOMNode::CDATA_SECTION_NODE ||            \
                  _nodeType == nsIDOMNode::PROCESSING_INSTRUCTION_NODE ||   \
                  _nodeType == nsIDOMNode::COMMENT_NODE ||                  \
                  _nodeType == nsIDOMNode::DOCUMENT_NODE ||                 \
                  _nodeType == nsIDOMNode::DOCUMENT_TYPE_NODE ||            \
                  _nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE ||        \
                  _nodeType == PR_UINT16_MAX,                               \
                  "Invalid nodeType");                                      \
NS_ABORT_IF_FALSE((_nodeType == nsIDOMNode::PROCESSING_INSTRUCTION_NODE ||  \
                   _nodeType == nsIDOMNode::DOCUMENT_TYPE_NODE) ==          \
                  (_extraName != nullptr),                                   \
                  "Supply aExtraName for and only for PIs and doctypes");   \
NS_ABORT_IF_FALSE(_nodeType == nsIDOMNode::ELEMENT_NODE ||                  \
                  _nodeType == nsIDOMNode::ATTRIBUTE_NODE ||                \
                  _nodeType == PR_UINT16_MAX ||                             \
                  aNamespaceID == kNameSpaceID_None,                        \
                  "Only attributes and elements can be in a namespace");    \
NS_ABORT_IF_FALSE(_name && _name != nsGkAtoms::_empty, "Invalid localName");\
NS_ABORT_IF_FALSE(((_nodeType == nsIDOMNode::TEXT_NODE) ==                  \
                   (_name == nsGkAtoms::textTagName)) &&                    \
                  ((_nodeType == nsIDOMNode::CDATA_SECTION_NODE) ==         \
                   (_name == nsGkAtoms::cdataTagName)) &&                   \
                  ((_nodeType == nsIDOMNode::COMMENT_NODE) ==               \
                   (_name == nsGkAtoms::commentTagName)) &&                 \
                  ((_nodeType == nsIDOMNode::DOCUMENT_NODE) ==              \
                   (_name == nsGkAtoms::documentNodeName)) &&               \
                  ((_nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) ==     \
                   (_name == nsGkAtoms::documentFragmentNodeName)) &&       \
                  ((_nodeType == nsIDOMNode::DOCUMENT_TYPE_NODE) ==         \
                   (_name == nsGkAtoms::documentTypeNodeName)) &&           \
                  ((_nodeType == nsIDOMNode::PROCESSING_INSTRUCTION_NODE) ==\
                   (_name == nsGkAtoms::processingInstructionTagName)),     \
                  "Wrong localName for nodeType");

#endif /* nsNodeInfo_h___ */
