/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
                  (_extraName != nsnull),                                   \
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
