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

#include "nsGenericDOMDataNode.h"
#include "nsIDOMText.h"
#include "nsLayoutAtoms.h"
#include "nsContentUtils.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMMutationEvent.h"
#include "nsIAttribute.h"
#include "nsIDocument.h"

/**
 * Class used to implement DOM text nodes
 */
class nsTextNode : public nsGenericDOMDataNode,
                   public nsIDOMText
{
public:
  nsTextNode(nsIDocument *aDocument);
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
  virtual nsIAtom *Tag() const;
  virtual PRBool IsContentOfType(PRUint32 aFlags) const;
#ifdef DEBUG
  virtual void List(FILE* out, PRInt32 aIndent) const;
  virtual void DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const;
#endif

  virtual already_AddRefed<nsITextContent> CloneContent(PRBool aCloneText,
                                                        nsIDocument *aOwnerDocument);
};

/**
 * class used to implement attr() generated content
 */
class nsAttributeTextNode : public nsTextNode {
public:
  class nsAttrChangeListener : public nsIDOMEventListener {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMEVENTLISTENER
  
    nsAttrChangeListener(PRInt32 aNameSpaceID, nsIAtom* aAttrName,
                         nsITextContent* aContent) :
      mNameSpaceID(aNameSpaceID),
      mAttrName(aAttrName),
      mContent(aContent)
    {
      NS_ASSERTION(mNameSpaceID != kNameSpaceID_Unknown, "Must know namespace");
      NS_ASSERTION(mAttrName, "Must have attr name");
      NS_ASSERTION(mContent, "Must have text content");
    }
    
    virtual PRBool IsNativeAnonymous() const { return PR_TRUE; }
    virtual void SetNativeAnonymous(PRBool aAnonymous) {
      NS_ASSERTION(aAnonymous,
                   "Attempt to set nsAttributeTextNode not anonymous!");
    }
  protected:
    friend class nsAttributeTextNode;
    PRInt32 mNameSpaceID;
    nsCOMPtr<nsIAtom> mAttrName;
    nsITextContent* mContent;  // Weak ref; it owns us
  };

  nsAttributeTextNode() : nsTextNode(nsnull) {
  }
  virtual ~nsAttributeTextNode() {
    DetachListener();
  }
  
  virtual void SetParent(nsIContent* aParent) {
    if (!aParent) {
      DetachListener();
    }
    nsTextNode::SetParent(aParent);
  }

  nsRefPtr<nsAttrChangeListener> mListener;  // our listener
private:
  void DetachListener();
};

nsresult
NS_NewTextNode(nsITextContent** aInstancePtrResult,
               nsIDocument *aOwnerDocument)
{
  *aInstancePtrResult = nsnull;

  nsCOMPtr<nsITextContent> instance = new nsTextNode(aOwnerDocument);
  NS_ENSURE_TRUE(instance, NS_ERROR_OUT_OF_MEMORY);

  if (aOwnerDocument && !aOwnerDocument->AddOrphan(instance)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  instance.swap(*aInstancePtrResult);

  return NS_OK;
}

nsTextNode::nsTextNode(nsIDocument *aDocument)
  : nsGenericDOMDataNode(aDocument)
{
}

nsTextNode::~nsTextNode()
{
}

NS_IMPL_ADDREF_INHERITED(nsTextNode, nsGenericDOMDataNode)
NS_IMPL_RELEASE_INHERITED(nsTextNode, nsGenericDOMDataNode)


// QueryInterface implementation for nsTextNode
NS_INTERFACE_MAP_BEGIN(nsTextNode)
  NS_INTERFACE_MAP_ENTRY(nsITextContent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMText)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCharacterData)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Text)
NS_INTERFACE_MAP_END_INHERITING(nsGenericDOMDataNode)

nsIAtom *
nsTextNode::Tag() const
{
  return nsLayoutAtoms::textTagName;
}

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

NS_IMETHODIMP
nsTextNode::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsCOMPtr<nsITextContent> textContent = CloneContent(PR_TRUE, GetOwnerDoc());
  NS_ENSURE_TRUE(textContent, NS_ERROR_OUT_OF_MEMORY);

  return CallQueryInterface(textContent, aReturn);
}

already_AddRefed<nsITextContent>
nsTextNode::CloneContent(PRBool aCloneText, nsIDocument *aOwnerDocument)
{
  nsTextNode* it = new nsTextNode(aOwnerDocument);
  if (!it)
    return nsnull;

  if (aCloneText) {
    it->mText = mText;
  }

  NS_ADDREF(it);

  if (aOwnerDocument && !aOwnerDocument->AddOrphan(it)) {
    NS_RELEASE(it);
  }

  return it;
}

PRBool
nsTextNode::IsContentOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~eTEXT);
}

#ifdef DEBUG
void
nsTextNode::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(IsInDoc(), "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Text@%p refcount=%d<", this, mRefCnt.get());

  nsAutoString tmp;
  ToCString(tmp, 0, mText.GetLength());
  fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);

  fputs(">\n", out);
}

void
nsTextNode::DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const
{
  NS_PRECONDITION(IsInDoc(), "bad content");

  if(aDumpAll) {
    PRInt32 index;
    for (index = aIndent; --index >= 0; ) fputs("  ", out);

    nsAutoString tmp;
    ToCString(tmp, 0, mText.GetLength());

    if(!tmp.EqualsLiteral("\\n")) {
      fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);
      if(aIndent) fputs("\n", out);
    }
  }
}
#endif

NS_IMPL_ISUPPORTS1(nsAttributeTextNode::nsAttrChangeListener,
                   nsIDOMEventListener)

NS_IMETHODIMP
nsAttributeTextNode::nsAttrChangeListener::HandleEvent(nsIDOMEvent* aEvent)
{
  // If mContent is null while we still get events, something is very wrong
  NS_ENSURE_TRUE(mContent, NS_ERROR_UNEXPECTED);
  
  nsCOMPtr<nsIDOMMutationEvent> event(do_QueryInterface(aEvent));
  NS_ENSURE_TRUE(event, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMEventTarget> target;
  event->GetTarget(getter_AddRefs(target));
  // Can't compare as an nsIDOMEventTarget, since that's implemented via a
  // tearoff...
  nsCOMPtr<nsIContent> targetContent(do_QueryInterface(target));
  if (targetContent != mContent->GetParent()) {
    // not the right node
    return NS_OK;
  }
  
  nsCOMPtr<nsIDOMNode> relatedNode;
  event->GetRelatedNode(getter_AddRefs(relatedNode));
  NS_ENSURE_TRUE(relatedNode, NS_ERROR_UNEXPECTED);
  nsCOMPtr<nsIAttribute> attr(do_QueryInterface(relatedNode));
  NS_ENSURE_TRUE(attr, NS_ERROR_UNEXPECTED);

  if (attr->NodeInfo()->Equals(mAttrName, mNameSpaceID)) {
    nsAutoString value;
    targetContent->GetAttr(mNameSpaceID, mAttrName, value);
    mContent->SetText(value, PR_TRUE);
  }
  
  return NS_OK;
}

nsresult
NS_NewAttributeContent(nsIContent* aContent, PRInt32 aNameSpaceID,
                       nsIAtom* aAttrName, nsIContent** aResult)
{
  NS_PRECONDITION(aContent, "Must have parent content");
  NS_PRECONDITION(aAttrName, "Must have an attr name");
  NS_PRECONDITION(aNameSpaceID != kNameSpaceID_Unknown, "Must know namespace");
  
  *aResult = nsnull;
  
  nsRefPtr<nsAttributeTextNode> textNode = new nsAttributeTextNode();
  NS_ENSURE_TRUE(textNode, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIDOMEventTarget> eventTarget(do_QueryInterface(aContent));
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_UNEXPECTED);
  
  nsRefPtr<nsAttributeTextNode::nsAttrChangeListener> listener =
    new nsAttributeTextNode::nsAttrChangeListener(aNameSpaceID,
                                                  aAttrName,
                                                  textNode);
  NS_ENSURE_TRUE(listener, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = eventTarget->AddEventListener(NS_LITERAL_STRING("DOMAttrModified"),
                                              listener,
                                              PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString attrValue;
  aContent->GetAttr(aNameSpaceID, aAttrName, attrValue);
  textNode->SetData(attrValue);

  textNode->SetParent(aContent);
  textNode->mListener = listener;  // Now textNode going away will detach the listener automatically.

  NS_ADDREF(*aResult = textNode);
  return NS_OK;
}

void
nsAttributeTextNode::DetachListener()
{
  if (!mListener)
    return;

  NS_ASSERTION(GetParent(), "How did our parent go away while we still have an observer?");

  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(GetParent()));
#ifdef DEBUG
  nsresult rv =
#endif
    target->RemoveEventListener(NS_LITERAL_STRING("DOMAttrModified"),
                                mListener,
                                PR_FALSE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "'Leaking' listener for lifetime of this page");
  mListener->mContent = nsnull;  // Make it forget us
  mListener = nsnull; // Goodbye, listener
}
