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

#include "nsTextNode.h"
#include "nsContentUtils.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMMutationEvent.h"
#include "nsIAttribute.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"

using namespace mozilla::dom;

/**
 * class used to implement attr() generated content
 */
class nsAttributeTextNode : public nsTextNode,
                            public nsStubMutationObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  
  nsAttributeTextNode(already_AddRefed<nsINodeInfo> aNodeInfo,
                      PRInt32 aNameSpaceID,
                      nsIAtom* aAttrName) :
    nsTextNode(aNodeInfo),
    mGrandparent(nsnull),
    mNameSpaceID(aNameSpaceID),
    mAttrName(aAttrName)
  {
    NS_ASSERTION(mNameSpaceID != kNameSpaceID_Unknown, "Must know namespace");
    NS_ASSERTION(mAttrName, "Must have attr name");
  }

  virtual ~nsAttributeTextNode() {
    NS_ASSERTION(!mGrandparent, "We were not unbound!");
  }

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  virtual nsGenericDOMDataNode *CloneDataNode(nsINodeInfo *aNodeInfo,
                                              PRBool aCloneText) const
  {
    nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
    nsAttributeTextNode *it = new nsAttributeTextNode(ni.forget(),
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

  // This doesn't need to be a strong pointer because it's only non-null
  // while we're bound to the document tree, and it points to an ancestor
  // so the ancestor must be bound to the document tree the whole time
  // and can't be deleted.
  nsIContent* mGrandparent;
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

  nsTextNode *instance = new nsTextNode(ni.forget());
  if (!instance) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = instance);

  return NS_OK;
}

nsTextNode::nsTextNode(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericDOMDataNode(aNodeInfo)
{
  NS_ABORT_IF_FALSE(mNodeInfo->NodeType() == nsIDOMNode::TEXT_NODE,
                    "Bad NodeType in aNodeInfo");
}

nsTextNode::~nsTextNode()
{
}

NS_IMPL_ADDREF_INHERITED(nsTextNode, nsGenericDOMDataNode)
NS_IMPL_RELEASE_INHERITED(nsTextNode, nsGenericDOMDataNode)

DOMCI_NODE_DATA(Text, nsTextNode)

// QueryInterface implementation for nsTextNode
NS_INTERFACE_TABLE_HEAD(nsTextNode)
  NS_NODE_INTERFACE_TABLE3(nsTextNode, nsIDOMNode, nsIDOMText,
                           nsIDOMCharacterData)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsTextNode)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Text)
NS_INTERFACE_MAP_END_INHERITING(nsGenericDOMDataNode)

PRBool
nsTextNode::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eTEXT | eDATA_NODE));
}

nsGenericDOMDataNode*
nsTextNode::CloneDataNode(nsINodeInfo *aNodeInfo, PRBool aCloneText) const
{
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  nsTextNode *it = new nsTextNode(ni.forget());
  if (it && aCloneText) {
    it->mText = mText;
  }

  return it;
}

nsresult
nsTextNode::BindToAttribute(nsIAttribute* aAttr)
{
  NS_ASSERTION(!IsInDoc(), "Unbind before binding!");
  NS_ASSERTION(!GetNodeParent(), "Unbind before binding!");
  NS_ASSERTION(HasSameOwnerDoc(aAttr), "Wrong owner document!");

  mParent = aAttr;
  SetParentIsContent(false);
  ClearInDocument();
  return NS_OK;
}

nsresult
nsTextNode::UnbindFromAttribute()
{
  NS_ASSERTION(GetNodeParent(), "Bind before unbinding!");
  NS_ASSERTION(GetNodeParent() &&
               GetNodeParent()->IsNodeOfType(nsINode::eATTRIBUTE),
               "Use this method only to unbind from an attribute!");
  mParent = nsnull;
  return NS_OK;
}

nsresult
nsTextNode::AppendTextForNormalize(const PRUnichar* aBuffer, PRUint32 aLength,
                                   PRBool aNotify, nsIContent* aNextSibling)
{
  CharacterDataChangeInfo::Details details = {
    CharacterDataChangeInfo::Details::eMerge, aNextSibling
  };
  return SetTextInternal(mText.GetLength(), 0, aBuffer, aLength, aNotify, &details);
}

#ifdef DEBUG
void
nsTextNode::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Text@%p", static_cast<const void*>(this));
  fprintf(out, " flags=[%08x]", static_cast<unsigned int>(GetFlags()));
  fprintf(out, " primaryframe=%p", static_cast<void*>(GetPrimaryFrame()));
  fprintf(out, " refcount=%d<", mRefCnt.get());

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

  nsAttributeTextNode* textNode = new nsAttributeTextNode(ni.forget(),
                                                          aNameSpaceID,
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
  NS_PRECONDITION(aParent && aParent->GetParent(),
                  "This node can't be a child of the document or of the document root");

  nsresult rv = nsTextNode::BindToTree(aDocument, aParent,
                                       aBindingParent, aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(!mGrandparent, "We were already bound!");
  mGrandparent = aParent->GetParent();
  mGrandparent->AddMutationObserver(this);

  // Note that there is no need to notify here, since we have no
  // frame yet at this point.
  UpdateText(PR_FALSE);

  return NS_OK;
}

void
nsAttributeTextNode::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  // UnbindFromTree can be called anytime so we have to be safe.
  if (mGrandparent) {
    // aNullParent might not be true here, but we want to remove the
    // mutation observer anyway since we only need it while we're
    // in the document.
    mGrandparent->RemoveMutationObserver(this);
    mGrandparent = nsnull;
  }
  nsTextNode::UnbindFromTree(aDeep, aNullParent);
}

void
nsAttributeTextNode::AttributeChanged(nsIDocument* aDocument,
                                      Element* aElement,
                                      PRInt32 aNameSpaceID,
                                      nsIAtom* aAttribute,
                                      PRInt32 aModType)
{
  if (aNameSpaceID == mNameSpaceID && aAttribute == mAttrName &&
      aElement == mGrandparent) {
    // Since UpdateText notifies, do it when it's safe to run script.  Note
    // that if we get unbound while the event is up that's ok -- we'll just
    // have no grandparent when it fires, and will do nothing.
    void (nsAttributeTextNode::*update)() = &nsAttributeTextNode::UpdateText;
    nsCOMPtr<nsIRunnable> ev = NS_NewRunnableMethod(this, update);
    nsContentUtils::AddScriptRunner(ev);
  }
}

void
nsAttributeTextNode::NodeWillBeDestroyed(const nsINode* aNode)
{
  NS_ASSERTION(aNode == static_cast<nsINode*>(mGrandparent), "Wrong node!");
  mGrandparent = nsnull;
}

void
nsAttributeTextNode::UpdateText(PRBool aNotify)
{
  if (mGrandparent) {
    nsAutoString attrValue;
    mGrandparent->GetAttr(mNameSpaceID, mAttrName, attrValue);
    SetText(attrValue, aNotify);
  }  
}
