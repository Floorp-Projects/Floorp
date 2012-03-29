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

#include "nsNodeInfoManager.h"
#include "plhash.h"
#include "nsIAtom.h"
#include "nsINameSpaceManager.h"
#include "nsCOMPtr.h"
#include "nsDOMString.h"

class nsFixedSizeAllocator;

class nsNodeInfo
{
public:
  NS_INLINE_DECL_REFCOUNTING_WITH_DESTROY(nsNodeInfo, LastRelease())
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsNodeInfo)

  // nsNodeInfo
  // Create objects with Create
  /*
   * aName and aOwnerManager may not be null.
   */
  static nsNodeInfo *Create(nsIAtom *aName, nsIAtom *aPrefix,
                            PRInt32 aNamespaceID, PRUint16 aNodeType,
                            nsIAtom *aExtraName,
                            nsNodeInfoManager *aOwnerManager);

  /*
   * Get the name from this node as a string, this does not include the prefix.
   *
   * For the HTML element "<body>" this will return "body" and for the XML
   * element "<html:body>" this will return "body".
   */
  void GetName(nsAString& aName) const
  {
    mInner.mName->ToString(aName);
  }

  /*
   * Get the name from this node as an atom, this does not include the prefix.
   * This function never returns a null atom.
   *
   * For the HTML element "<body>" this will return the "body" atom and for
   * the XML element "<html:body>" this will return the "body" atom.
   */
  nsIAtom* NameAtom() const
  {
    return mInner.mName;
  }

  /*
   * Get the qualified name from this node as a string, the qualified name
   * includes the prefix, if one exists.
   *
   * For the HTML element "<body>" this will return "body" and for the XML
   * element "<html:body>" this will return "html:body".
   */
  const nsString& QualifiedName() const {
    return mQualifiedName;
  }

  /*
   * Returns the node's nodeName as defined in DOM Core
   */
  const nsString& NodeName() const {
    return mNodeName;
  }

  /*
   * Returns the node's localName as defined in DOM Core
   */
  const nsString& LocalName() const {
    return mLocalName;
  }

  /*
   * Get the prefix from this node as a string.
   *
   * For the HTML element "<body>" this will return a null string and for
   * the XML element "<html:body>" this will return the string "html".
   */
  void GetPrefix(nsAString& aPrefix) const
  {
    if (mInner.mPrefix) {
      mInner.mPrefix->ToString(aPrefix);
    } else {
      SetDOMStringToNull(aPrefix);
    }
  }

  /*
   * Get the prefix from this node as an atom.
   *
   * For the HTML element "<body>" this will return a null atom and for
   * the XML element "<html:body>" this will return the "html" atom.
   */
  nsIAtom* GetPrefixAtom() const
  {
    return mInner.mPrefix;
  }

  /*
   * Get the namespace URI for a node, if the node has a namespace URI.
   */
  nsresult GetNamespaceURI(nsAString& aNameSpaceURI) const;

  /*
   * Get the namespace ID for a node if the node has a namespace, if not this
   * returns kNameSpaceID_None.
   */
  PRInt32 NamespaceID() const
  {
    return mInner.mNamespaceID;
  }

  /*
   * Get the nodetype for the node. Returns the values specified in nsIDOMNode
   * for nsIDOMNode.nodeType
   */
  PRUint16 NodeType() const
  {
    return mInner.mNodeType;
  }

  /*
   * Get the extra name, used by PIs and DocTypes, for the node.
   */
  nsIAtom* GetExtraName() const
  {
    return mInner.mExtraName;
  }

  /*
   * Get and set the ID attribute atom for this node.
   * See http://www.w3.org/TR/1998/REC-xml-19980210#sec-attribute-types
   * for the definition of an ID attribute.
   *
   */
  nsIAtom* GetIDAttributeAtom() const
  {
    return mIDAttributeAtom;
  }

  void SetIDAttributeAtom(nsIAtom* aID)
  {
    mIDAttributeAtom = aID;
  }

  /**
   * Get the owning node info manager. Only to be used inside Gecko, you can't
   * really do anything with the pointer outside Gecko anyway.
   */
  nsNodeInfoManager *NodeInfoManager() const
  {
    return mOwnerManager;
  }

  /*
   * Utility functions that can be used to check if a nodeinfo holds a specific
   * name, name and prefix, name and prefix and namespace ID, or just
   * namespace ID.
   */
  bool Equals(nsNodeInfo *aNodeInfo) const
  {
    return aNodeInfo == this || aNodeInfo->Equals(mInner.mName, mInner.mPrefix,
                                                  mInner.mNamespaceID);
  }

  bool NameAndNamespaceEquals(nsNodeInfo *aNodeInfo) const
  {
    return aNodeInfo == this || aNodeInfo->Equals(mInner.mName,
                                                  mInner.mNamespaceID);
  }

  bool Equals(nsIAtom *aNameAtom) const
  {
    return mInner.mName == aNameAtom;
  }

  bool Equals(nsIAtom *aNameAtom, nsIAtom *aPrefixAtom) const
  {
    return (mInner.mName == aNameAtom) && (mInner.mPrefix == aPrefixAtom);
  }

  bool Equals(nsIAtom *aNameAtom, PRInt32 aNamespaceID) const
  {
    return ((mInner.mName == aNameAtom) &&
            (mInner.mNamespaceID == aNamespaceID));
  }

  bool Equals(nsIAtom *aNameAtom, nsIAtom *aPrefixAtom,
              PRInt32 aNamespaceID) const
  {
    return ((mInner.mName == aNameAtom) &&
            (mInner.mPrefix == aPrefixAtom) &&
            (mInner.mNamespaceID == aNamespaceID));
  }

  bool NamespaceEquals(PRInt32 aNamespaceID) const
  {
    return mInner.mNamespaceID == aNamespaceID;
  }

  bool Equals(const nsAString& aName) const
  {
    return mInner.mName->Equals(aName);
  }

  bool Equals(const nsAString& aName, const nsAString& aPrefix) const
  {
    return mInner.mName->Equals(aName) &&
      (mInner.mPrefix ? mInner.mPrefix->Equals(aPrefix) : aPrefix.IsEmpty());
  }

  bool Equals(const nsAString& aName, PRInt32 aNamespaceID) const
  {
    return mInner.mNamespaceID == aNamespaceID &&
      mInner.mName->Equals(aName);
  }

  bool Equals(const nsAString& aName, const nsAString& aPrefix,
                PRInt32 aNamespaceID) const
  {
    return mInner.mName->Equals(aName) && mInner.mNamespaceID == aNamespaceID &&
      (mInner.mPrefix ? mInner.mPrefix->Equals(aPrefix) : aPrefix.IsEmpty());
  }

  bool NamespaceEquals(const nsAString& aNamespaceURI) const;

  bool QualifiedNameEquals(nsIAtom* aNameAtom) const
  {
    NS_PRECONDITION(aNameAtom, "Must have name atom");
    if (!GetPrefixAtom())
      return Equals(aNameAtom);

    return aNameAtom->Equals(mQualifiedName);
  }

  bool QualifiedNameEquals(const nsAString& aQualifiedName) const
  {
    return mQualifiedName == aQualifiedName;
  }

  /*
   * Retrieve a pointer to the document that owns this node info.
   */
  nsIDocument* GetDocument() const
  {
    return mDocument;
  }

protected:
  /*
   * nsNodeInfoInner is used for two things:
   *
   *   1. as a member in nsNodeInfo for holding the name, prefix and
   *      namespace ID
   *   2. as the hash key in the hash table in nsNodeInfoManager
   *
   * nsNodeInfoInner does not do any kind of reference counting,
   * that's up to the user of this class. Since nsNodeInfoInner is
   * typically used as a member of nsNodeInfo, the hash table doesn't
   * need to delete the keys. When the value (nsNodeInfo) is deleted
   * the key is automatically deleted.
   */

  class nsNodeInfoInner
  {
  public:
    nsNodeInfoInner()
      : mName(nsnull), mPrefix(nsnull), mNamespaceID(kNameSpaceID_Unknown),
        mNodeType(0), mNameString(nsnull), mExtraName(nsnull)
    {
    }
    nsNodeInfoInner(nsIAtom *aName, nsIAtom *aPrefix, PRInt32 aNamespaceID,
                    PRUint16 aNodeType, nsIAtom* aExtraName)
      : mName(aName), mPrefix(aPrefix), mNamespaceID(aNamespaceID),
        mNodeType(aNodeType), mNameString(nsnull), mExtraName(aExtraName)
    {
    }
    nsNodeInfoInner(const nsAString& aTmpName, nsIAtom *aPrefix,
                    PRInt32 aNamespaceID, PRUint16 aNodeType)
      : mName(nsnull), mPrefix(aPrefix), mNamespaceID(aNamespaceID),
        mNodeType(aNodeType), mNameString(&aTmpName), mExtraName(nsnull)
    {
    }

    nsIAtom*            mName;
    nsIAtom*            mPrefix;
    PRInt32             mNamespaceID;
    PRUint16            mNodeType; // As defined by nsIDOMNode.nodeType
    const nsAString*    mNameString;
    nsIAtom*            mExtraName; // Only used by PIs and DocTypes
  };

  // nsNodeInfoManager needs to pass mInner to the hash table.
  friend class nsNodeInfoManager;

  nsIDocument* mDocument; // Weak. Cache of mOwnerManager->mDocument

  nsNodeInfoInner mInner;

  nsCOMPtr<nsIAtom> mIDAttributeAtom;
  nsNodeInfoManager* mOwnerManager; // Strong reference!

  /*
   * Members for various functions of mName+mPrefix that we can be
   * asked to compute.
   */

  // Qualified name
  nsString mQualifiedName;

  // nodeName for the node.
  nsString mNodeName;

  // localName for the node. This is either equal to mInner.mName, or a
  // void string, depending on mInner.mNodeType.
  nsString mLocalName;

private:
  nsNodeInfo(); // Unimplemented
  nsNodeInfo(const nsNodeInfo& aOther); // Unimplemented
  nsNodeInfo(nsIAtom *aName, nsIAtom *aPrefix, PRInt32 aNamespaceID,
             PRUint16 aNodeType, nsIAtom *aExtraName,
             nsNodeInfoManager *aOwnerManager);
protected:
  ~nsNodeInfo();

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
