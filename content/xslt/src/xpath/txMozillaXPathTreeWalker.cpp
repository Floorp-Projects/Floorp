/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "txXPathTreeWalker.h"
#include "nsIAtom.h"
#include "nsIAttribute.h"
#include "nsIDOMAttr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsINodeInfo.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsTextFragment.h"
#include "txXMLUtils.h"
#include "txLog.h"
#include "nsUnicharUtils.h"
#include "nsAttrName.h"
#include "nsTArray.h"
#include "mozilla/dom/Element.h"

const PRUint32 kUnknownIndex = PRUint32(-1);

txXPathTreeWalker::txXPathTreeWalker(const txXPathTreeWalker& aOther)
    : mPosition(aOther.mPosition),
      mCurrentIndex(aOther.mCurrentIndex)
{
}

txXPathTreeWalker::txXPathTreeWalker(const txXPathNode& aNode)
    : mPosition(aNode),
      mCurrentIndex(kUnknownIndex)
{
}

void
txXPathTreeWalker::moveToRoot()
{
    if (mPosition.isDocument()) {
        return;
    }

    nsIDocument* root = mPosition.mNode->GetCurrentDoc();
    if (root) {
        mPosition.mIndex = txXPathNode::eDocument;
        mPosition.mNode = root;
    }
    else {
        nsINode *rootNode = mPosition.Root();

        NS_ASSERTION(rootNode->IsNodeOfType(nsINode::eCONTENT),
                     "root of subtree wasn't an nsIContent");

        mPosition.mIndex = txXPathNode::eContent;
        mPosition.mNode = rootNode;
    }

    mCurrentIndex = kUnknownIndex;
    mDescendants.Clear();
}

bool
txXPathTreeWalker::moveToElementById(const nsAString& aID)
{
    if (aID.IsEmpty()) {
        return false;
    }

    nsIDocument* doc = mPosition.mNode->GetCurrentDoc();

    nsCOMPtr<nsIContent> content;
    if (doc) {
        content = doc->GetElementById(aID);
    }
    else {
        // We're in a disconnected subtree, search only that subtree.
        nsINode *rootNode = mPosition.Root();

        NS_ASSERTION(rootNode->IsNodeOfType(nsINode::eCONTENT),
                     "root of subtree wasn't an nsIContent");

        content = nsContentUtils::MatchElementId(
            static_cast<nsIContent*>(rootNode), aID);
    }

    if (!content) {
        return false;
    }

    mPosition.mIndex = txXPathNode::eContent;
    mPosition.mNode = content;
    mCurrentIndex = kUnknownIndex;
    mDescendants.Clear();

    return true;
}

bool
txXPathTreeWalker::moveToFirstAttribute()
{
    if (!mPosition.isContent()) {
        return false;
    }

    return moveToValidAttribute(0);
}

bool
txXPathTreeWalker::moveToNextAttribute()
{
    // XXX an assertion should be enough here with the current code
    if (!mPosition.isAttribute()) {
        return false;
    }

    return moveToValidAttribute(mPosition.mIndex + 1);
}

bool
txXPathTreeWalker::moveToValidAttribute(PRUint32 aStartIndex)
{
    NS_ASSERTION(!mPosition.isDocument(), "documents doesn't have attrs");

    PRUint32 total = mPosition.Content()->GetAttrCount();
    if (aStartIndex >= total) {
        return false;
    }

    PRUint32 index;
    for (index = aStartIndex; index < total; ++index) {
        const nsAttrName* name = mPosition.Content()->GetAttrNameAt(index);

        // We need to ignore XMLNS attributes.
        if (name->NamespaceID() != kNameSpaceID_XMLNS) {
            mPosition.mIndex = index;

            return true;
        }
    }
    return false;
}

bool
txXPathTreeWalker::moveToNamedAttribute(nsIAtom* aLocalName, PRInt32 aNSID)
{
    if (!mPosition.isContent()) {
        return false;
    }

    const nsAttrName* name;
    PRUint32 i;
    for (i = 0; (name = mPosition.Content()->GetAttrNameAt(i)); ++i) {
        if (name->Equals(aLocalName, aNSID)) {
            mPosition.mIndex = i;

            return true;
        }
    }
    return false;
}

bool
txXPathTreeWalker::moveToFirstChild()
{
    if (mPosition.isAttribute()) {
        return false;
    }

    NS_ASSERTION(!mPosition.isDocument() ||
                 (mCurrentIndex == kUnknownIndex && mDescendants.IsEmpty()),
                 "we shouldn't have any position info at the document");
    NS_ASSERTION(mCurrentIndex != kUnknownIndex || mDescendants.IsEmpty(),
                 "Index should be known if parents index are");

    nsIContent* child = mPosition.mNode->GetFirstChild();
    if (!child) {
        return false;
    }
    mPosition.mIndex = txXPathNode::eContent;
    mPosition.mNode = child;

    if (mCurrentIndex != kUnknownIndex &&
        !mDescendants.AppendValue(mCurrentIndex)) {
        mDescendants.Clear();
    }
    mCurrentIndex = 0;

    return true;
}

bool
txXPathTreeWalker::moveToLastChild()
{
    if (mPosition.isAttribute()) {
        return false;
    }

    NS_ASSERTION(!mPosition.isDocument() ||
                 (mCurrentIndex == kUnknownIndex && mDescendants.IsEmpty()),
                 "we shouldn't have any position info at the document");
    NS_ASSERTION(mCurrentIndex != kUnknownIndex || mDescendants.IsEmpty(),
                 "Index should be known if parents index are");

    PRUint32 total = mPosition.mNode->GetChildCount();
    if (!total) {
        return false;
    }
    mPosition.mNode = mPosition.mNode->GetLastChild();

    if (mCurrentIndex != kUnknownIndex &&
        !mDescendants.AppendValue(mCurrentIndex)) {
        mDescendants.Clear();
    }
    mCurrentIndex = total - 1;

    return true;
}

bool
txXPathTreeWalker::moveToNextSibling()
{
    if (!mPosition.isContent()) {
        return false;
    }

    return moveToSibling(1);
}

bool
txXPathTreeWalker::moveToPreviousSibling()
{
    if (!mPosition.isContent()) {
        return false;
    }

    return moveToSibling(-1);
}

bool
txXPathTreeWalker::moveToParent()
{
    if (mPosition.isDocument()) {
        return false;
    }

    if (mPosition.isAttribute()) {
        mPosition.mIndex = txXPathNode::eContent;

        return true;
    }

    nsINode* parent = mPosition.mNode->GetNodeParent();
    if (!parent) {
        return false;
    }

    PRUint32 count = mDescendants.Length();
    if (count) {
        mCurrentIndex = mDescendants.ValueAt(--count);
        mDescendants.RemoveValueAt(count);
    }
    else {
        mCurrentIndex = kUnknownIndex;
    }

    mPosition.mIndex = mPosition.mNode->GetParent() ?
      txXPathNode::eContent : txXPathNode::eDocument;
    mPosition.mNode = parent;

    return true;
}

bool
txXPathTreeWalker::moveToSibling(PRInt32 aDir)
{
    NS_ASSERTION(mPosition.isContent(),
                 "moveToSibling should only be called for content");

    nsINode* parent = mPosition.mNode->GetNodeParent();
    if (!parent) {
        return false;
    }
    if (mCurrentIndex == kUnknownIndex) {
        mCurrentIndex = parent->IndexOf(mPosition.mNode);
    }

    // if mCurrentIndex is 0 we rely on GetChildAt returning null for an
    // index of PRUint32(-1).
    PRUint32 newIndex = mCurrentIndex + aDir;
    nsIContent* newChild = parent->GetChildAt(newIndex);
    if (!newChild) {
        return false;
    }

    mPosition.mNode = newChild;
    mCurrentIndex = newIndex;

    return true;
}

txXPathNode::txXPathNode(const txXPathNode& aNode)
  : mNode(aNode.mNode),
    mRefCountRoot(aNode.mRefCountRoot),
    mIndex(aNode.mIndex)
{
    MOZ_COUNT_CTOR(txXPathNode);
    if (mRefCountRoot) {
        NS_ADDREF(Root());
    }
}

txXPathNode::~txXPathNode()
{
    MOZ_COUNT_DTOR(txXPathNode);
    if (mRefCountRoot) {
        nsINode *root = Root();
        NS_RELEASE(root);
    }
}

/* static */
bool
txXPathNodeUtils::getAttr(const txXPathNode& aNode, nsIAtom* aLocalName,
                          PRInt32 aNSID, nsAString& aValue)
{
    if (aNode.isDocument() || aNode.isAttribute()) {
        return false;
    }

    return aNode.Content()->GetAttr(aNSID, aLocalName, aValue);
}

/* static */
already_AddRefed<nsIAtom>
txXPathNodeUtils::getLocalName(const txXPathNode& aNode)
{
    if (aNode.isDocument()) {
        return nsnull;
    }

    if (aNode.isContent()) {
        if (aNode.mNode->IsElement()) {
            nsIAtom* localName = aNode.Content()->Tag();
            NS_ADDREF(localName);

            return localName;
        }

        if (aNode.mNode->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION)) {
            nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aNode.mNode);
            nsAutoString target;
            node->GetNodeName(target);

            return NS_NewAtom(target);
        }

        return nsnull;
    }

    nsIAtom* localName = aNode.Content()->
        GetAttrNameAt(aNode.mIndex)->LocalName();
    NS_ADDREF(localName);

    return localName;
}

nsIAtom*
txXPathNodeUtils::getPrefix(const txXPathNode& aNode)
{
    if (aNode.isDocument()) {
        return nsnull;
    }

    if (aNode.isContent()) {
        // All other nsIContent node types but elements have a null prefix
        // which is what we want here.
        return aNode.Content()->NodeInfo()->GetPrefixAtom();
    }

    return aNode.Content()->GetAttrNameAt(aNode.mIndex)->GetPrefix();
}

/* static */
void
txXPathNodeUtils::getLocalName(const txXPathNode& aNode, nsAString& aLocalName)
{
    if (aNode.isDocument()) {
        aLocalName.Truncate();

        return;
    }

    if (aNode.isContent()) {
        if (aNode.mNode->IsElement()) {
            nsINodeInfo* nodeInfo = aNode.Content()->NodeInfo();
            nodeInfo->GetName(aLocalName);
            return;
        }

        if (aNode.mNode->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION)) {
            // PIs don't have a nodeinfo but do have a name
            nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aNode.mNode);
            node->GetNodeName(aLocalName);

            return;
        }

        aLocalName.Truncate();

        return;
    }

    aNode.Content()->GetAttrNameAt(aNode.mIndex)->LocalName()->
      ToString(aLocalName);

    // Check for html
    if (aNode.Content()->NodeInfo()->NamespaceEquals(kNameSpaceID_None) &&
        aNode.Content()->IsHTML()) {
        nsContentUtils::ASCIIToUpper(aLocalName);
    }
}

/* static */
void
txXPathNodeUtils::getNodeName(const txXPathNode& aNode, nsAString& aName)
{
    if (aNode.isDocument()) {
        aName.Truncate();

        return;
    }

    if (aNode.isContent()) {
        // Elements and PIs have a name
        if (aNode.mNode->IsElement() ||
            aNode.mNode->NodeType() ==
            nsIDOMNode::PROCESSING_INSTRUCTION_NODE) {
            aName = aNode.Content()->NodeName();
            return;
        }

        aName.Truncate();

        return;
    }

    aNode.Content()->GetAttrNameAt(aNode.mIndex)->GetQualifiedName(aName);
}

/* static */
PRInt32
txXPathNodeUtils::getNamespaceID(const txXPathNode& aNode)
{
    if (aNode.isDocument()) {
        return kNameSpaceID_None;
    }

    if (aNode.isContent()) {
        return aNode.Content()->GetNameSpaceID();
    }

    return aNode.Content()->GetAttrNameAt(aNode.mIndex)->NamespaceID();
}

/* static */
void
txXPathNodeUtils::getNamespaceURI(const txXPathNode& aNode, nsAString& aURI)
{
    nsContentUtils::NameSpaceManager()->GetNameSpaceURI(getNamespaceID(aNode), aURI);
}

/* static */
PRUint16
txXPathNodeUtils::getNodeType(const txXPathNode& aNode)
{
    if (aNode.isDocument()) {
        return txXPathNodeType::DOCUMENT_NODE;
    }

    if (aNode.isContent()) {
        return aNode.mNode->NodeType();
    }

    return txXPathNodeType::ATTRIBUTE_NODE;
}

/* static */
void
txXPathNodeUtils::appendNodeValue(const txXPathNode& aNode, nsAString& aResult)
{
    if (aNode.isAttribute()) {
        const nsAttrName* name = aNode.Content()->GetAttrNameAt(aNode.mIndex);

        if (aResult.IsEmpty()) {
            aNode.Content()->GetAttr(name->NamespaceID(), name->LocalName(),
                                     aResult);
        }
        else {
            nsAutoString result;
            aNode.Content()->GetAttr(name->NamespaceID(), name->LocalName(),
                                     result);
            aResult.Append(result);
        }

        return;
    }

    if (aNode.isDocument() ||
        aNode.mNode->IsElement() ||
        aNode.mNode->IsNodeOfType(nsINode::eDOCUMENT_FRAGMENT)) {
        nsContentUtils::AppendNodeTextContent(aNode.mNode, true, aResult);

        return;
    }

    aNode.Content()->AppendTextTo(aResult);
}

/* static */
bool
txXPathNodeUtils::isWhitespace(const txXPathNode& aNode)
{
    NS_ASSERTION(aNode.isContent() && isText(aNode), "Wrong type!");

    return aNode.Content()->TextIsOnlyWhitespace();
}

/* static */
txXPathNode*
txXPathNodeUtils::getOwnerDocument(const txXPathNode& aNode)
{
    return new txXPathNode(aNode.mNode->OwnerDoc());
}

#ifndef HAVE_64BIT_OS
#define kFmtSize 13
#define kFmtSizeAttr 24
const char gPrintfFmt[] = "id0x%08p";
const char gPrintfFmtAttr[] = "id0x%08p-%010i";
#else
#define kFmtSize 21
#define kFmtSizeAttr 32
const char gPrintfFmt[] = "id0x%016p";
const char gPrintfFmtAttr[] = "id0x%016p-%010i";
#endif

/* static */
nsresult
txXPathNodeUtils::getXSLTId(const txXPathNode& aNode,
                            const txXPathNode& aBase,
                            nsAString& aResult)
{
    PRUword nodeid = ((PRUword)aNode.mNode) - ((PRUword)aBase.mNode);
    if (!aNode.isAttribute()) {
        CopyASCIItoUTF16(nsPrintfCString(kFmtSize, gPrintfFmt, nodeid),
                         aResult);
    }
    else {
        CopyASCIItoUTF16(nsPrintfCString(kFmtSizeAttr, gPrintfFmtAttr,
                                         nodeid, aNode.mIndex), aResult);
    }

    return NS_OK;
}

/* static */
void
txXPathNodeUtils::getBaseURI(const txXPathNode& aNode, nsAString& aURI)
{
    aNode.mNode->GetDOMBaseURI(aURI);
}

/* static */
PRIntn
txXPathNodeUtils::comparePosition(const txXPathNode& aNode,
                                  const txXPathNode& aOtherNode)
{
    // First check for equal nodes or attribute-nodes on the same element.
    if (aNode.mNode == aOtherNode.mNode) {
        if (aNode.mIndex == aOtherNode.mIndex) {
            return 0;
        }

        NS_ASSERTION(!aNode.isDocument() && !aOtherNode.isDocument(),
                     "documents should always have a set index");

        if (aNode.isContent() || (!aOtherNode.isContent() &&
                                  aNode.mIndex < aOtherNode.mIndex)) {
            return -1;
        }

        return 1;
    }

    // Get document for both nodes.
    nsIDocument* document = aNode.mNode->GetCurrentDoc();
    nsIDocument* otherDocument = aOtherNode.mNode->GetCurrentDoc();

    // If the nodes have different current documents, compare the document
    // pointers.
    if (document != otherDocument) {
        return document < otherDocument ? -1 : 1;
    }

    // Now either both nodes are in orphan trees, or they are both in the
    // same tree.

    // Get parents up the tree.
    nsAutoTArray<nsINode*, 8> parents, otherParents;
    nsINode* node = aNode.mNode;
    nsINode* otherNode = aOtherNode.mNode;
    nsINode* parent, *otherParent;
    while (node && otherNode) {
        parent = node->GetNodeParent();
        otherParent = otherNode->GetNodeParent();

        // Hopefully this is a common case.
        if (parent == otherParent) {
            if (!parent) {
                // Both node and otherNode are root nodes in respective orphan
                // tree.
                return node < otherNode ? -1 : 1;
            }

            return parent->IndexOf(node) < parent->IndexOf(otherNode) ?
                   -1 : 1;
        }

        parents.AppendElement(node);
        otherParents.AppendElement(otherNode);
        node = parent;
        otherNode = otherParent;
    }

    while (node) {
        parents.AppendElement(node);
        node = node->GetNodeParent();
    }
    while (otherNode) {
        otherParents.AppendElement(otherNode);
        otherNode = otherNode->GetNodeParent();
    }

    // Walk back down along the parent-chains until we find where they split.
    PRInt32 total = parents.Length() - 1;
    PRInt32 otherTotal = otherParents.Length() - 1;
    NS_ASSERTION(total != otherTotal, "Can't have same number of parents");

    PRInt32 lastIndex = NS_MIN(total, otherTotal);
    PRInt32 i;
    parent = nsnull;
    for (i = 0; i <= lastIndex; ++i) {
        node = parents.ElementAt(total - i);
        otherNode = otherParents.ElementAt(otherTotal - i);
        if (node != otherNode) {
            if (!parent) {
                // The two nodes are in different orphan subtrees.
                NS_ASSERTION(i == 0, "this shouldn't happen");
                return node < otherNode ? -1 : 1;
            }

            PRInt32 index = parent->IndexOf(node);
            PRInt32 otherIndex = parent->IndexOf(otherNode);
            NS_ASSERTION(index != otherIndex && index >= 0 && otherIndex >= 0,
                         "invalid index in compareTreePosition");

            return index < otherIndex ? -1 : 1;
        }

        parent = node;
    }

    // One node is a descendant of the other. The one with the shortest
    // parent-chain is first in the document.
    return total < otherTotal ? -1 : 1;
}

/* static */
txXPathNode*
txXPathNativeNode::createXPathNode(nsIContent* aContent, bool aKeepRootAlive)
{
    nsINode* root = aKeepRootAlive ? txXPathNode::RootOf(aContent) : nsnull;

    return new txXPathNode(aContent, txXPathNode::eContent, root);
}

/* static */
txXPathNode*
txXPathNativeNode::createXPathNode(nsIDOMNode* aNode, bool aKeepRootAlive)
{
    PRUint16 nodeType;
    aNode->GetNodeType(&nodeType);

    if (nodeType == nsIDOMNode::ATTRIBUTE_NODE) {
        nsCOMPtr<nsIAttribute> attr = do_QueryInterface(aNode);
        NS_ASSERTION(attr, "doesn't implement nsIAttribute");

        nsINodeInfo *nodeInfo = attr->NodeInfo();
        nsIContent *parent = attr->GetContent();
        if (!parent) {
            return nsnull;
        }

        nsINode* root = aKeepRootAlive ? txXPathNode::RootOf(parent) : nsnull;

        PRUint32 i, total = parent->GetAttrCount();
        for (i = 0; i < total; ++i) {
            const nsAttrName* name = parent->GetAttrNameAt(i);
            if (nodeInfo->Equals(name->LocalName(), name->NamespaceID())) {
                return new txXPathNode(parent, i, root);
            }
        }

        NS_ERROR("Couldn't find the attribute in its parent!");

        return nsnull;
    }

    nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
    PRUint32 index;
    nsINode* root = aKeepRootAlive ? node.get() : nsnull;

    if (nodeType == nsIDOMNode::DOCUMENT_NODE) {
        index = txXPathNode::eDocument;
    }
    else {
        index = txXPathNode::eContent;
        if (root) {
            root = txXPathNode::RootOf(root);
        }
    }

    return new txXPathNode(node, index, root);
}

/* static */
txXPathNode*
txXPathNativeNode::createXPathNode(nsIDOMDocument* aDocument)
{
    nsCOMPtr<nsIDocument> document = do_QueryInterface(aDocument);
    return new txXPathNode(document);
}

/* static */
nsresult
txXPathNativeNode::getNode(const txXPathNode& aNode, nsIDOMNode** aResult)
{
    if (!aNode.isAttribute()) {
        return CallQueryInterface(aNode.mNode, aResult);
    }

    const nsAttrName* name = aNode.Content()->GetAttrNameAt(aNode.mIndex);

    nsAutoString namespaceURI;
    nsContentUtils::NameSpaceManager()->GetNameSpaceURI(name->NamespaceID(), namespaceURI);

    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode.mNode);
    nsCOMPtr<nsIDOMAttr> attr;
    element->GetAttributeNodeNS(namespaceURI,
                                nsDependentAtomString(name->LocalName()),
                                getter_AddRefs(attr));

    return CallQueryInterface(attr, aResult);
}

/* static */
nsIContent*
txXPathNativeNode::getContent(const txXPathNode& aNode)
{
    NS_ASSERTION(aNode.isContent(),
                 "Only call getContent on nsIContent wrappers!");
    return aNode.Content();
}

/* static */
nsIDocument*
txXPathNativeNode::getDocument(const txXPathNode& aNode)
{
    NS_ASSERTION(aNode.isDocument(),
                 "Only call getDocument on nsIDocument wrappers!");
    return aNode.Document();
}
