/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txXPathTreeWalker.h"
#include "nsAtom.h"
#include "nsIAttribute.h"
#include "nsIDOMAttr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsTextFragment.h"
#include "txXMLUtils.h"
#include "txLog.h"
#include "nsUnicharUtils.h"
#include "nsAttrName.h"
#include "nsTArray.h"
#include "mozilla/dom/Attr.h"
#include "mozilla/dom/Element.h"
#include <stdint.h>
#include <algorithm>

using namespace mozilla::dom;

txXPathTreeWalker::txXPathTreeWalker(const txXPathTreeWalker& aOther)
    : mPosition(aOther.mPosition)
{
}

txXPathTreeWalker::txXPathTreeWalker(const txXPathNode& aNode)
    : mPosition(aNode)
{
}

void
txXPathTreeWalker::moveToRoot()
{
    if (mPosition.isDocument()) {
        return;
    }

    nsIDocument* root = mPosition.mNode->GetUncomposedDoc();
    if (root) {
        mPosition.mIndex = txXPathNode::eDocument;
        mPosition.mNode = root;
    }
    else {
        nsINode *rootNode = mPosition.Root();

        NS_ASSERTION(rootNode->IsContent(),
                     "root of subtree wasn't an nsIContent");

        mPosition.mIndex = txXPathNode::eContent;
        mPosition.mNode = rootNode;
    }
}

bool
txXPathTreeWalker::moveToElementById(const nsAString& aID)
{
    if (aID.IsEmpty()) {
        return false;
    }

    nsIDocument* doc = mPosition.mNode->GetUncomposedDoc();

    nsCOMPtr<nsIContent> content;
    if (doc) {
        content = doc->GetElementById(aID);
    }
    else {
        // We're in a disconnected subtree, search only that subtree.
        nsINode *rootNode = mPosition.Root();

        NS_ASSERTION(rootNode->IsContent(),
                     "root of subtree wasn't an nsIContent");

        content = nsContentUtils::MatchElementId(
            static_cast<nsIContent*>(rootNode), aID);
    }

    if (!content) {
        return false;
    }

    mPosition.mIndex = txXPathNode::eContent;
    mPosition.mNode = content;

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
txXPathTreeWalker::moveToValidAttribute(uint32_t aStartIndex)
{
    NS_ASSERTION(!mPosition.isDocument(), "documents doesn't have attrs");

    if (!mPosition.Content()->IsElement()) {
      return false;
    }

    Element* element = mPosition.Content()->AsElement();
    uint32_t total = element->GetAttrCount();
    if (aStartIndex >= total) {
        return false;
    }

    uint32_t index;
    for (index = aStartIndex; index < total; ++index) {
        const nsAttrName* name = element->GetAttrNameAt(index);

        // We need to ignore XMLNS attributes.
        if (name->NamespaceID() != kNameSpaceID_XMLNS) {
            mPosition.mIndex = index;

            return true;
        }
    }
    return false;
}

bool
txXPathTreeWalker::moveToNamedAttribute(nsAtom* aLocalName, int32_t aNSID)
{
    if (!mPosition.isContent() || !mPosition.Content()->IsElement()) {
        return false;
    }

    Element* element = mPosition.Content()->AsElement();

    const nsAttrName* name;
    uint32_t i;
    for (i = 0; (name = element->GetAttrNameAt(i)); ++i) {
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

    nsIContent* child = mPosition.mNode->GetFirstChild();
    if (!child) {
        return false;
    }
    mPosition.mIndex = txXPathNode::eContent;
    mPosition.mNode = child;

    return true;
}

bool
txXPathTreeWalker::moveToLastChild()
{
    if (mPosition.isAttribute()) {
        return false;
    }

    nsIContent* child = mPosition.mNode->GetLastChild();
    if (!child) {
        return false;
    }

    mPosition.mIndex = txXPathNode::eContent;
    mPosition.mNode = child;

    return true;
}

bool
txXPathTreeWalker::moveToNextSibling()
{
    if (!mPosition.isContent()) {
        return false;
    }

    nsINode* sibling = mPosition.mNode->GetNextSibling();
    if (!sibling) {
      return false;
    }

    mPosition.mNode = sibling;

    return true;
}

bool
txXPathTreeWalker::moveToPreviousSibling()
{
    if (!mPosition.isContent()) {
        return false;
    }

    nsINode* sibling = mPosition.mNode->GetPreviousSibling();
    if (!sibling) {
      return false;
    }

    mPosition.mNode = sibling;

    return true;
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

    nsINode* parent = mPosition.mNode->GetParentNode();
    if (!parent) {
        return false;
    }

    mPosition.mIndex = mPosition.mNode->GetParent() ?
      txXPathNode::eContent : txXPathNode::eDocument;
    mPosition.mNode = parent;

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
txXPathNodeUtils::getAttr(const txXPathNode& aNode, nsAtom* aLocalName,
                          int32_t aNSID, nsAString& aValue)
{
    if (aNode.isDocument() || aNode.isAttribute() ||
        !aNode.Content()->IsElement()) {
        return false;
    }

    return aNode.Content()->AsElement()->GetAttr(aNSID, aLocalName, aValue);
}

/* static */
already_AddRefed<nsAtom>
txXPathNodeUtils::getLocalName(const txXPathNode& aNode)
{
    if (aNode.isDocument()) {
        return nullptr;
    }

    if (aNode.isContent()) {
        if (aNode.mNode->IsElement()) {
            RefPtr<nsAtom> localName =
                aNode.Content()->NodeInfo()->NameAtom();
            return localName.forget();
        }

        if (aNode.mNode->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION)) {
            nsCOMPtr<nsIDOMNode> node = do_QueryInterface(aNode.mNode);
            nsAutoString target;
            node->GetNodeName(target);

            return NS_Atomize(target);
        }

        return nullptr;
    }

    // This is an attribute node, so we necessarily come from an element.
    RefPtr<nsAtom> localName =
      aNode.Content()->AsElement()->GetAttrNameAt(aNode.mIndex)->LocalName();

    return localName.forget();
}

nsAtom*
txXPathNodeUtils::getPrefix(const txXPathNode& aNode)
{
    if (aNode.isDocument()) {
        return nullptr;
    }

    if (aNode.isContent()) {
        // All other nsIContent node types but elements have a null prefix
        // which is what we want here.
        return aNode.Content()->NodeInfo()->GetPrefixAtom();
    }

    return aNode.Content()->AsElement()->GetAttrNameAt(aNode.mIndex)->GetPrefix();
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
            mozilla::dom::NodeInfo* nodeInfo = aNode.Content()->NodeInfo();
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

    aNode.Content()->AsElement()->GetAttrNameAt(aNode.mIndex)->LocalName()->
      ToString(aLocalName);

    // Check for html
    if (aNode.Content()->NodeInfo()->NamespaceEquals(kNameSpaceID_None) &&
        aNode.Content()->IsHTMLElement()) {
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

    aNode.Content()->AsElement()->GetAttrNameAt(aNode.mIndex)->GetQualifiedName(aName);
}

/* static */
int32_t
txXPathNodeUtils::getNamespaceID(const txXPathNode& aNode)
{
    if (aNode.isDocument()) {
        return kNameSpaceID_None;
    }

    if (aNode.isContent()) {
        return aNode.Content()->GetNameSpaceID();
    }

    return aNode.Content()->AsElement()->GetAttrNameAt(aNode.mIndex)->NamespaceID();
}

/* static */
void
txXPathNodeUtils::getNamespaceURI(const txXPathNode& aNode, nsAString& aURI)
{
    nsContentUtils::NameSpaceManager()->GetNameSpaceURI(getNamespaceID(aNode), aURI);
}

/* static */
uint16_t
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
        const nsAttrName* name = aNode.Content()->AsElement()->GetAttrNameAt(aNode.mIndex);

        if (aResult.IsEmpty()) {
            aNode.Content()->AsElement()->GetAttr(name->NamespaceID(),
                                                  name->LocalName(),
                                                  aResult);
        } else {
            nsAutoString result;
            aNode.Content()->AsElement()->GetAttr(name->NamespaceID(),
                                                  name->LocalName(),
                                                  result);
            aResult.Append(result);
        }

        return;
    }

    if (aNode.isDocument() ||
        aNode.mNode->IsElement() ||
        aNode.mNode->IsNodeOfType(nsINode::eDOCUMENT_FRAGMENT)) {
        nsContentUtils::AppendNodeTextContent(aNode.mNode, true, aResult,
                                              mozilla::fallible);

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

const char gPrintfFmt[] = "id0x%" PRIxPTR;
const char gPrintfFmtAttr[] = "id0x%" PRIxPTR "-%010i";

/* static */
nsresult
txXPathNodeUtils::getXSLTId(const txXPathNode& aNode,
                            const txXPathNode& aBase,
                            nsAString& aResult)
{
    uintptr_t nodeid = ((uintptr_t)aNode.mNode) - ((uintptr_t)aBase.mNode);
    if (!aNode.isAttribute()) {
        CopyASCIItoUTF16(nsPrintfCString(gPrintfFmt, nodeid),
                         aResult);
    }
    else {
        CopyASCIItoUTF16(nsPrintfCString(gPrintfFmtAttr,
                                         nodeid, aNode.mIndex), aResult);
    }

    return NS_OK;
}

/* static */
nsresult
txXPathNodeUtils::getBaseURI(const txXPathNode& aNode, nsAString& aURI)
{
    return aNode.mNode->GetBaseURI(aURI);
}

/* static */
int
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
    nsIDocument* document = aNode.mNode->GetUncomposedDoc();
    nsIDocument* otherDocument = aOtherNode.mNode->GetUncomposedDoc();

    // If the nodes have different current documents, compare the document
    // pointers.
    if (document != otherDocument) {
        return document < otherDocument ? -1 : 1;
    }

    // Now either both nodes are in orphan trees, or they are both in the
    // same tree.

    // Get parents up the tree.
    AutoTArray<nsINode*, 8> parents, otherParents;
    nsINode* node = aNode.mNode;
    nsINode* otherNode = aOtherNode.mNode;
    nsINode* parent;
    nsINode* otherParent;
    while (node && otherNode) {
        parent = node->GetParentNode();
        otherParent = otherNode->GetParentNode();

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
        node = node->GetParentNode();
    }
    while (otherNode) {
        otherParents.AppendElement(otherNode);
        otherNode = otherNode->GetParentNode();
    }

    // Walk back down along the parent-chains until we find where they split.
    int32_t total = parents.Length() - 1;
    int32_t otherTotal = otherParents.Length() - 1;
    NS_ASSERTION(total != otherTotal, "Can't have same number of parents");

    int32_t lastIndex = std::min(total, otherTotal);
    int32_t i;
    parent = nullptr;
    for (i = 0; i <= lastIndex; ++i) {
        node = parents.ElementAt(total - i);
        otherNode = otherParents.ElementAt(otherTotal - i);
        if (node != otherNode) {
            if (!parent) {
                // The two nodes are in different orphan subtrees.
                NS_ASSERTION(i == 0, "this shouldn't happen");
                return node < otherNode ? -1 : 1;
            }

            int32_t index = parent->IndexOf(node);
            int32_t otherIndex = parent->IndexOf(otherNode);
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
    nsINode* root = aKeepRootAlive ? txXPathNode::RootOf(aContent) : nullptr;

    return new txXPathNode(aContent, txXPathNode::eContent, root);
}

/* static */
txXPathNode*
txXPathNativeNode::createXPathNode(nsINode* aNode, bool aKeepRootAlive)
{
    uint16_t nodeType = aNode->NodeType();
    if (nodeType == nsIDOMNode::ATTRIBUTE_NODE) {
        nsCOMPtr<nsIAttribute> attr = do_QueryInterface(aNode);
        NS_ASSERTION(attr, "doesn't implement nsIAttribute");

        mozilla::dom::NodeInfo *nodeInfo = attr->NodeInfo();
        mozilla::dom::Element* parent =
          static_cast<Attr*>(attr.get())->GetElement();
        if (!parent) {
            return nullptr;
        }

        nsINode* root = aKeepRootAlive ? txXPathNode::RootOf(parent) : nullptr;

        uint32_t i, total = parent->GetAttrCount();
        for (i = 0; i < total; ++i) {
            const nsAttrName* name = parent->GetAttrNameAt(i);
            if (nodeInfo->Equals(name->LocalName(), name->NamespaceID())) {
                return new txXPathNode(parent, i, root);
            }
        }

        NS_ERROR("Couldn't find the attribute in its parent!");

        return nullptr;
    }

    uint32_t index;
    nsINode* root = aKeepRootAlive ? aNode : nullptr;

    if (nodeType == nsIDOMNode::DOCUMENT_NODE) {
        index = txXPathNode::eDocument;
    }
    else {
        index = txXPathNode::eContent;
        if (root) {
            root = txXPathNode::RootOf(root);
        }
    }

    return new txXPathNode(aNode, index, root);
}

/* static */
txXPathNode*
txXPathNativeNode::createXPathNode(nsIDOMDocument* aDocument)
{
    nsCOMPtr<nsIDocument> document = do_QueryInterface(aDocument);
    return new txXPathNode(document);
}

/* static */
nsINode*
txXPathNativeNode::getNode(const txXPathNode& aNode)
{
    if (!aNode.isAttribute()) {
        return aNode.mNode;
    }

    const nsAttrName* name =
      aNode.Content()->AsElement()->GetAttrNameAt(aNode.mIndex);

    nsAutoString namespaceURI;
    nsContentUtils::NameSpaceManager()->GetNameSpaceURI(name->NamespaceID(), namespaceURI);

    nsCOMPtr<Element> element = do_QueryInterface(aNode.mNode);
    nsDOMAttributeMap* map = element->Attributes();
    return map->GetNamedItemNS(namespaceURI,
                               nsDependentAtomString(name->LocalName()));
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
