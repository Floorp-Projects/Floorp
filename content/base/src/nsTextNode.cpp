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
 * The Original Code is Mozilla Communicator client code.
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
 * Implementation of DOM Core's nsIDOMText node.
 */

#include "nsGenericDOMDataNode.h"
#include "nsIDOMText.h"
#include "nsContentUtils.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMMutationEvent.h"
#include "nsIAttribute.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"

/**
 * Class used to implement DOM text nodes
 */
class nsTextNode : public nsGenericDOMDataNode,
                   public nsIDOMText
{
public:
  nsTextNode(nsINodeInfo *aNodeInfo);
  virtual ~nsTextNode();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_IMPL_NSIDOMNODE_USING_GENERIC_DOM_DATA

  // nsIDOMCharacterData
  NS_FORWARD_NSIDOMCHARACTERDATA(nsGenericDOMDataNode::)

  // nsIDOMText
  NS_FORWARD_NSIDOMTEXT(nsGenericDOMDataNode::)

  // nsIContent
  virtual PRBool IsNodeOfType(PRUint32 aFlags) const;
#ifdef DEBUG
  virtual void List(FILE* out, PRInt32 aIndent) const;
  virtual void DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const;
#endif
};

/**
 * class used to implement attr() generated content
 */
class nsAttributeTextNode : public nsTextNode,
                            public nsStubMutationObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  
  nsAttributeTextNode(nsINodeInfo *aNodeInfo,
                      PRInt32 aNameSpaceID,
                      nsIAtom* aAttrName) :
    nsTextNode(aNodeInfo),
    mNameSpaceID(aNameSpaceID),
    mAttrName(aAttrName)
  {
    NS_ASSERTION(mNameSpaceID != kNameSpaceID_Unknown, "Must know namespace");
    NS_ASSERTION(mAttrName, "Must have attr name");
  }

  virtual ~nsAttributeTextNode() {
    DetachListener();
  }

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);

  virtual void AttributeChanged(nsIDocument* aDocument,
                                nsIContent* aContent,
                                PRInt32 aNameSpaceID,
                                nsIAtom* aAttribute,
                                PRInt32 aModType);

  virtual nsGenericDOMDataNode *CloneDataNode(nsINodeInfo *aNodeInfo,
                                              PRBool aCloneText) const
  {
    nsAttributeTextNode *it = new nsAttributeTextNode(aNodeInfo,
                                                      mNameSpaceID,
                                                      mAttrName);
    if (it && aCloneText) {
      it->mText = mText;
    }

    return it;
  }

  // Public method for the event to run
  void UpdateText() {
    UpdateText(PR_TRUE);
  }

private:
  // Update our text to our parent's current attr value
  void UpdateText(PRBool aNotify);

  // Detach ourselves as an attribute listener.
  void DetachListener();

  // What attribute we're showing
  PRInt32 mNameSpaceID;
  nsCOMPtr<nsIAtom> mAttrName;
};

nsresult
NS_NewTextNode(nsIContent** aInstancePtrResult,
               nsNodeInfoManager *aNodeInfoManager)
{
  NS_PRECONDITION(aNodeInfoManager, "Missing nodeInfoManager");

  *aInstancePtrResult = nsnull;

  nsCOMPtr<nsINodeInfo> ni = aNodeInfoManager->GetTextNodeInfo();
  if (!ni) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsIContent *instance = new nsTextNode(ni);
  if (!instance) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = instance);

  return NS_OK;
}

nsTextNode::nsTextNode(nsINodeInfo *aNodeInfo)
  : nsGenericDOMDataNode(aNodeInfo)
{
}

nsTextNode::~nsTextNode()
{
}

NS_IMPL_ADDREF_INHERITED(nsTextNode, nsGenericDOMDataNode)
NS_IMPL_RELEASE_INHERITED(nsTextNode, nsGenericDOMDataNode)


// QueryInterface implementation for nsTextNode
NS_INTERFACE_MAP_BEGIN(nsTextNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMText)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCharacterData)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Text)
NS_INTERFACE_MAP_END_INHERITING(nsGenericDOMDataNode)

NS_IMETHODIMP
nsTextNode::GetNodeName(nsAString& aNodeName)
{
  aNodeName.AssignLiteral("#text");
  return NS_OK;
}

NS_IMETHODIMP
nsTextNode::GetNodeValue(nsAString& aNodeValue)
{
  return nsGenericDOMDataNode::GetNodeValue(aNodeValue);
}

NS_IMETHODIMP
nsTextNode::SetNodeValue(const nsAString& aNodeValue)
{
  return nsGenericDOMDataNode::SetNodeValue(aNodeValue);
}

NS_IMETHODIMP
nsTextNode::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::TEXT_NODE;
  return NS_OK;
}

PRBool
nsTextNode::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eTEXT | eDATA_NODE));
}

nsGenericDOMDataNode*
nsTextNode::CloneDataNode(nsINodeInfo *aNodeInfo, PRBool aCloneText) const
{
  nsTextNode *it = new nsTextNode(aNodeInfo);
  if (it && aCloneText) {
    it->mText = mText;
  }

  return it;
}

#ifdef DEBUG
void
nsTextNode::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Text@%p refcount=%d<", this, mRefCnt.get());

  nsAutoString tmp;
  ToCString(tmp, 0, mText.GetLength());
  fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);

  fputs(">\n", out);
}

void
nsTextNode::DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const
{
  if(aDumpAll) {
    PRInt32 index;
    for (index = aIndent; --index >= 0; ) fputs("  ", out);

    nsAutoString tmp;
    ToCString(tmp, 0, mText.GetLength());

    if(!tmp.EqualsLiteral("\\n")) {
      fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);
      if(aIndent) fputs("\n", out);
    }
  }
}
#endif

nsresult
NS_NewAttributeContent(nsNodeInfoManager *aNodeInfoManager,
                       PRInt32 aNameSpaceID, nsIAtom* aAttrName,
                       nsIContent** aResult)
{
  NS_PRECONDITION(aNodeInfoManager, "Missing nodeInfoManager");
  NS_PRECONDITION(aAttrName, "Must have an attr name");
  NS_PRECONDITION(aNameSpaceID != kNameSpaceID_Unknown, "Must know namespace");
  
  *aResult = nsnull;

  nsCOMPtr<nsINodeInfo> ni = aNodeInfoManager->GetTextNodeInfo();
  if (!ni) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsAttributeTextNode* textNode = new nsAttributeTextNode(ni, aNameSpaceID,
                                                          aAttrName);
  if (!textNode) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult = textNode);

  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED1(nsAttributeTextNode, nsTextNode,
                             nsIMutationObserver)

nsresult
nsAttributeTextNode::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                PRBool aCompileEventHandlers)
{
  NS_PRECONDITION(aParent, "This node can't be a child of the document");

  nsresult rv = nsTextNode::BindToTree(aDocument, aParent,
                                       aBindingParent, aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  nsINode* parent = GetNodeParent();
  NS_ENSURE_TRUE(parent, NS_ERROR_UNEXPECTED);

  parent->AddMutationObserver(this);

  // Note that there is no need to notify here, since we have no
  // frame yet at this point.
  UpdateText(PR_FALSE);

  return NS_OK;
}

void
nsAttributeTextNode::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  // Detach as listener while we know who our parent is!
  if (aNullParent) {
    DetachListener();
  }
  nsTextNode::UnbindFromTree(aDeep, aNullParent);
}

void
nsAttributeTextNode::AttributeChanged(nsIDocument* aDocument,
                                      nsIContent* aContent,
                                      PRInt32 aNameSpaceID,
                                      nsIAtom* aAttribute,
                                      PRInt32 aModType)
{
  if (aNameSpaceID == mNameSpaceID && aAttribute == mAttrName &&
      aContent == GetNodeParent()) {
    // Since UpdateText notifies, do it asynchronously.  Note that if we get
    // unbound while the event is up that's ok -- we'll just have no parent
    // when it fires, and will do nothing.    
    // XXXbz ideally we'd either process this on layout flushes or do it right
    // after nsIMutationObserver notifications are over or something, instead
    // of doing it fully async.
    nsCOMPtr<nsIRunnable> ev = new nsRunnableMethod<nsAttributeTextNode>(
            this, &nsAttributeTextNode::UpdateText);
    NS_DispatchToCurrentThread(ev);
  }
}

void
nsAttributeTextNode::UpdateText(PRBool aNotify)
{
  nsIContent* parent = GetParent();
  if (parent) {
    nsAutoString attrValue;
    parent->GetAttr(mNameSpaceID, mAttrName, attrValue);
    SetText(attrValue, aNotify);
  }  
}

void
nsAttributeTextNode::DetachListener()
{
  nsINode* parent = GetNodeParent();
  if (parent) {
    parent->RemoveMutationObserver(this);
  }
}
