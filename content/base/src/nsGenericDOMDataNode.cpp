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
#include "nsGenericElement.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMRange.h"
#include "nsIDOMDocument.h"
#include "nsRange.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsReadableUtils.h"
#include "nsMutationEvent.h"
#include "nsINameSpaceManager.h"
#include "nsIDOM3Node.h"
#include "nsIURI.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMText.h"
#include "nsCOMPtr.h"
#include "nsDOMString.h"

#include "pldhash.h"
#include "prprf.h"

nsGenericDOMDataNode::nsGenericDOMDataNode(nsIDocument *aDocument)
{
  mParentPtrBits = NS_REINTERPRET_CAST(PtrBits, aDocument);
}

nsGenericDOMDataNode::~nsGenericDOMDataNode()
{
  if (HasEventListenerManager()) {
    PL_DHashTableOperate(&nsGenericElement::sEventListenerManagersHash,
                         this, PL_DHASH_REMOVE);
  }

  if (HasRangeList()) {
    PL_DHashTableOperate(&nsGenericElement::sRangeListsHash,
                         this, PL_DHASH_REMOVE);
  }

  if (ParentIsDocument()) {
    nsIDocument *document = ParentPtrBitsAsDocument();
    if (document) {
      document->RemoveOrphan(this);
    }
  }
}


NS_IMPL_ADDREF(nsGenericDOMDataNode)
NS_IMPL_RELEASE(nsGenericDOMDataNode)

NS_INTERFACE_MAP_BEGIN(nsGenericDOMDataNode)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMEventReceiver,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMEventTarget,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3EventTarget,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  // No nsITextContent since all subclasses might not want that.
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3Node, new nsNode3Tearoff(this))
NS_INTERFACE_MAP_END


nsresult
nsGenericDOMDataNode::GetNodeValue(nsAString& aNodeValue)
{
  return GetData(aNodeValue);
}

nsresult
nsGenericDOMDataNode::SetNodeValue(const nsAString& aNodeValue)
{
  return SetData(aNodeValue);
}

nsresult
nsGenericDOMDataNode::GetParentNode(nsIDOMNode** aParentNode)
{
  nsresult rv = NS_OK;

  nsIContent *parent = GetParent();
  if (parent) {
    rv = CallQueryInterface(parent, aParentNode);
  }
  else {
    nsIDocument *doc = ParentPtrBitsAsDocument();
    if (doc && !doc->IsOrphan(this)) {
      rv = CallQueryInterface(doc, aParentNode);
    }
  }

  NS_ASSERTION(NS_SUCCEEDED(rv), "Must be a DOM Node");

  return rv;
}

nsresult
nsGenericDOMDataNode::GetPreviousSibling(nsIDOMNode** aPrevSibling)
{
  nsresult rv = NS_OK;

  nsIContent *sibling = nsnull;
  nsIContent *parent = GetParent();
  if (parent) {
    PRInt32 pos = parent->IndexOf(this);
    if (pos > 0) {
      sibling = parent->GetChildAt(pos - 1);
    }
  }
  else {
    nsIDocument *doc = ParentPtrBitsAsDocument();
    if (doc) {
      PRInt32 pos = doc->IndexOf(this);
      if (pos > 0) {
        sibling = doc->GetChildAt(pos - 1);
      }
    }
  }

  if (sibling) {
    rv = CallQueryInterface(sibling, aPrevSibling);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Must be a DOM Node");
  } else {
    *aPrevSibling = nsnull;
  }

  return rv;
}

nsresult
nsGenericDOMDataNode::GetNextSibling(nsIDOMNode** aNextSibling)
{
  nsresult rv = NS_OK;

  nsIContent *sibling = nsnull;
  nsIContent *parent = GetParent();
  if (parent) {
    PRInt32 pos = parent->IndexOf(this);
    if (pos > 0) {
      sibling = parent->GetChildAt(pos + 1);
    }
  }
  else {
    nsIDocument *doc = ParentPtrBitsAsDocument();
    if (doc) {
      PRInt32 pos = doc->IndexOf(this);
      if (pos > 0) {
        sibling = doc->GetChildAt(pos + 1);
      }
    }
  }

  if (sibling) {
    rv = CallQueryInterface(sibling, aNextSibling);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Must be a DOM Node");
  } else {
    *aNextSibling = nsnull;
  }

  return rv;
}

nsresult
nsGenericDOMDataNode::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  // XXX Since we believe this won't be done very often, we won't
  // burn another slot in the data node and just create a new
  // (empty) childNodes list every time we're asked.
  nsChildContentList* list = new nsChildContentList(nsnull);
  if (!list) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(list, aChildNodes);
}

nsresult
nsGenericDOMDataNode::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  nsIDocument *document = GetOwnerDoc();
  if (document) {
    return CallQueryInterface(document, aOwnerDocument);
  }

  *aOwnerDocument = nsnull;

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::GetNamespaceURI(nsAString& aNamespaceURI)
{
  SetDOMStringToNull(aNamespaceURI);

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::GetPrefix(nsAString& aPrefix)
{
  SetDOMStringToNull(aPrefix);

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SetPrefix(const nsAString& aPrefix)
{
  return NS_ERROR_DOM_NAMESPACE_ERR;
}

nsresult
nsGenericDOMDataNode::GetLocalName(nsAString& aLocalName)
{
  SetDOMStringToNull(aLocalName);

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::Normalize()
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::IsSupported(const nsAString& aFeature,
                                  const nsAString& aVersion,
                                  PRBool* aReturn)
{
  return nsGenericElement::InternalIsSupported(aFeature, aVersion, aReturn);
}

nsresult
nsGenericDOMDataNode::GetBaseURI(nsAString& aURI)
{
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();
  nsCAutoString spec;

  if (baseURI) {
    baseURI->GetSpec(spec);
  }

  CopyUTF8toUTF16(spec, aURI);

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::LookupPrefix(const nsAString& aNamespaceURI,
                                   nsAString& aPrefix)
{
  aPrefix.Truncate();

  nsIContent *parent_weak = GetParent();

  // DOM Data Node passes the query on to its parent
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(parent_weak));
  if (node) {
    return node->LookupPrefix(aNamespaceURI, aPrefix);
  }

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                                         nsAString& aNamespaceURI)
{
  aNamespaceURI.Truncate();

  nsIContent *parent_weak = GetParent();

  // DOM Data Node passes the query on to its parent
  nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(parent_weak));

  if (node) {
    return node->LookupNamespaceURI(aNamespacePrefix, aNamespaceURI);
  }

  return NS_OK;
}

//----------------------------------------------------------------------

// Implementation of nsIDOMCharacterData

nsresult
nsGenericDOMDataNode::GetData(nsAString& aData)
{
  if (mText.Is2b()) {
    aData.Assign(mText.Get2b(), mText.GetLength());
  } else {
    // Must use Substring() since nsDependentCString() requires null
    // terminated strings.

    const char *data = mText.Get1b();

    if (data) {
      CopyASCIItoUCS2(Substring(data, data + mText.GetLength()), aData);
    } else {
      aData.Truncate();
    }
  }

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SetData(const nsAString& aData)
{
  // inform any enclosed ranges of change
  // we can lie and say we are deleting all the text, since in a total
  // text replacement we should just collapse all the ranges.

  if (HasRangeList()) {
    nsRange::TextOwnerChanged(this, 0, mText.GetLength(), 0);
  }

  nsCOMPtr<nsITextContent> textContent = do_QueryInterface(this);

  SetText(aData, PR_TRUE);

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::GetLength(PRUint32* aLength)
{
  *aLength = mText.GetLength();
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SubstringData(PRUint32 aStart, PRUint32 aCount,
                                    nsAString& aReturn)
{
  aReturn.Truncate();

  // XXX add <0 checks if types change
  PRUint32 textLength = PRUint32( mText.GetLength() );
  if (aStart > textLength) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  PRUint32 amount = aCount;
  if (amount > textLength - aStart) {
    amount = textLength - aStart;
  }

  if (mText.Is2b()) {
    aReturn.Assign(mText.Get2b() + aStart, amount);
  } else {
    // Must use Substring() since nsDependentCString() requires null
    // terminated strings.

    const char *data = mText.Get1b() + aStart;
    CopyASCIItoUCS2(Substring(data, data + amount), aReturn);
  }

  return NS_OK;
}

//----------------------------------------------------------------------

nsresult
nsGenericDOMDataNode::AppendData(const nsAString& aData)
{
#if 1
  PRInt32 length = 0;

  // See bugzilla bug 77585.
  if (mText.Is2b() || (!IsASCII(aData))) {
    nsAutoString old_data;
    mText.AppendTo(old_data);
    length = old_data.Length();
    // XXXjag We'd like to just say |old_data + aData|, but due
    // to issues with dependent concatenation and sliding (sub)strings
    // we'll just have to copy for now. See bug 121841 for details.
    old_data.Append(aData);
    SetText(old_data, PR_FALSE);
  } else {
    // We know aData and the current data is ASCII, so use a
    // nsC*String, no need for any fancy unicode stuff here.
    nsCAutoString old_data;
    mText.AppendTo(old_data);
    length = old_data.Length();
    LossyAppendUTF16toASCII(aData, old_data);
    SetText(old_data.get(), old_data.Length(), PR_FALSE);
  }

  // Trigger a reflow
  nsIDocument *document = GetCurrentDoc();
  if (document) {
    document->CharacterDataChanged(this, PR_TRUE);
  }

  return NS_OK;
#else
  return ReplaceData(mText.GetLength(), 0, aData);
#endif
}

nsresult
nsGenericDOMDataNode::InsertData(PRUint32 aOffset,
                                 const nsAString& aData)
{
  return ReplaceData(aOffset, 0, aData);
}

nsresult
nsGenericDOMDataNode::DeleteData(PRUint32 aOffset, PRUint32 aCount)
{
  nsAutoString empty;
  return ReplaceData(aOffset, aCount, empty);
}

nsresult
nsGenericDOMDataNode::ReplaceData(PRUint32 aOffset, PRUint32 aCount,
                                  const nsAString& aData)
{
  // sanitize arguments
  PRUint32 textLength = mText.GetLength();
  if (aOffset > textLength) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Allocate new buffer
  PRUint32 endOffset = aOffset + aCount;
  if (endOffset > textLength) {
    aCount = textLength - aOffset;
    endOffset = textLength;
  }
  PRInt32 dataLength = aData.Length();
  PRInt32 newLength = textLength - aCount + dataLength;
  PRUnichar* to = new PRUnichar[newLength + 1];
  if (!to) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // inform any enclosed ranges of change

  if (HasRangeList()) {
    nsRange::TextOwnerChanged(this, aOffset, endOffset, dataLength);
  }

  // Copy over appropriate data
  if (0 != aOffset) {
    mText.CopyTo(to, 0, aOffset);
  }
  if (0 != dataLength) {
    CopyUnicodeTo(aData, 0, to+aOffset, dataLength);
  }
  if (endOffset != textLength) {
    mText.CopyTo(to + aOffset + dataLength, endOffset, textLength - endOffset);
  }

  // Null terminate the new buffer...
  to[newLength] = (PRUnichar)0;

  SetText(to, newLength, PR_TRUE);
  delete [] to;

  return NS_OK;
}

//----------------------------------------------------------------------

nsresult
nsGenericDOMDataNode::GetListenerManager(nsIEventListenerManager **aResult)
{
  nsCOMPtr<nsIEventListenerManager> listener_manager;
  LookupListenerManager(getter_AddRefs(listener_manager));

  if (listener_manager) {
    *aResult = listener_manager;
    NS_ADDREF(*aResult);

    return NS_OK;
  }

  if (!nsGenericElement::sEventListenerManagersHash.ops) {
    nsresult rv = nsGenericElement::InitHashes();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsresult rv = NS_NewEventListenerManager(aResult);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add a mapping to the hash table
  EventListenerManagerMapEntry *entry =
    NS_STATIC_CAST(EventListenerManagerMapEntry *,
                   PL_DHashTableOperate(&nsGenericElement::
                                        sEventListenerManagersHash, this,
                                        PL_DHASH_ADD));

  entry->mListenerManager = *aResult;

  entry->mListenerManager->SetListenerTarget(this);

  SetHasEventListenerManager(PR_TRUE);

  return NS_OK;
}

//----------------------------------------------------------------------

// Implementation of nsIContent

#ifdef DEBUG
void
nsGenericDOMDataNode::ToCString(nsAString& aBuf, PRInt32 aOffset,
                                PRInt32 aLen) const
{
  if (mText.Is2b()) {
    const PRUnichar* cp = mText.Get2b() + aOffset;
    const PRUnichar* end = cp + aLen;

    while (cp < end) {
      PRUnichar ch = *cp++;
      if (ch == '\r') {
        aBuf.AppendLiteral("\\r");
      } else if (ch == '\n') {
        aBuf.AppendLiteral("\\n");
      } else if (ch == '\t') {
        aBuf.AppendLiteral("\\t");
      } else if ((ch < ' ') || (ch >= 127)) {
        char buf[10];
        PR_snprintf(buf, sizeof(buf), "\\u%04x", ch);
        AppendASCIItoUTF16(buf, aBuf);
      } else {
        aBuf.Append(ch);
      }
    }
  } else {
    unsigned char* cp = (unsigned char*)mText.Get1b() + aOffset;
    const unsigned char* end = cp + aLen;

    while (cp < end) {
      PRUnichar ch = *cp++;
      if (ch == '\r') {
        aBuf.AppendLiteral("\\r");
      } else if (ch == '\n') {
        aBuf.AppendLiteral("\\n");
      } else if (ch == '\t') {
        aBuf.AppendLiteral("\\t");
      } else if ((ch < ' ') || (ch >= 127)) {
        char buf[10];
        PR_snprintf(buf, sizeof(buf), "\\u%04x", ch);
        AppendASCIItoUTF16(buf, aBuf);
      } else {
        aBuf.Append(ch);
      }
    }
  }
}
#endif

nsIDocument*
nsGenericDOMDataNode::GetDocument() const
{
  nsIContent *parent = GetParent();
  if (parent) {
    return parent->GetDocument();
  }

  nsIDocument *document = ParentPtrBitsAsDocument();
  if (document &&
      document->IsOrphan(NS_CONST_CAST(nsGenericDOMDataNode*, this))) {
    return nsnull;
  }

  return document;
}

void
nsGenericDOMDataNode::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                                  PRBool aCompileEventHandlers)
{
  if (aDocument) {
    if (ParentIsDocument()) {
      nsIDocument *document = ParentPtrBitsAsDocument();
      if (document) {
        document->RemoveOrphan(this);
      }

      mParentPtrBits =
        NS_REINTERPRET_CAST(PtrBits, aDocument) |
        (mParentPtrBits & PARENT_BIT_RANGELISTS_OR_LISTENERMANAGER);
    }

    if (mText.IsBidi()) {
      aDocument->SetBidiEnabled(PR_TRUE);
    }
  }
  else if (ParentIsDocument()) {
    // XXX We should call AddOrphan here, but we first need ClearDocumentPointer
    //     so that RemoveOrphan/RemoveOrphans don't end up here.
    mParentPtrBits &= nsIContent::kParentBitMask;
  }
}

nsIDocument*
nsGenericDOMDataNode::GetOwnerDoc() const
{
  nsIContent *parent = GetParent();

  return parent ? parent->GetOwnerDoc() : ParentPtrBitsAsDocument();
}

PRBool
nsGenericDOMDataNode::IsInDoc() const
{
  nsIContent *parent = GetParent();
  if (parent) {
    return parent->IsInDoc();
  }

  nsIDocument *document = ParentPtrBitsAsDocument();

  return document && !document->IsOrphan(NS_CONST_CAST(nsGenericDOMDataNode*,
                                                       this));
}

void
nsGenericDOMDataNode::SetParent(nsIContent* aParent)
{
  PtrBits new_bits = NS_REINTERPRET_CAST(PtrBits, aParent);

  if (aParent) {
    if (ParentIsDocument()) {
      nsIDocument *document = ParentPtrBitsAsDocument();
      if (document) {
        document->RemoveOrphan(this);
      }
    }

    new_bits |= PARENT_BIT_BITPTR_IS_CONTENT;
  }
  else {
    nsIContent *parent = GetParent();
    if (parent) {
      nsIDocument *document = parent->GetOwnerDoc();
      if (document && document->AddOrphan(this)) {
        new_bits = NS_REINTERPRET_CAST(PtrBits, document);
      }
    }
  }

  mParentPtrBits = new_bits |
                   (mParentPtrBits & PARENT_BIT_RANGELISTS_OR_LISTENERMANAGER);
}

PRBool
nsGenericDOMDataNode::IsNativeAnonymous() const
{
  nsIContent* parent = GetParent();
  return parent && parent->IsNativeAnonymous();
}

void
nsGenericDOMDataNode::SetNativeAnonymous(PRBool aAnonymous)
{
  // XXX Need to fix this to do something - bug 165110
}

void
nsGenericDOMDataNode::GetNameSpaceID(PRInt32* aID) const
{
  *aID = kNameSpaceID_None;
}

nsIAtom *
nsGenericDOMDataNode::GetIDAttributeName() const
{
  return nsnull;
}

nsIAtom *
nsGenericDOMDataNode::GetClassAttributeName() const
{
  return nsnull;
}

already_AddRefed<nsINodeInfo>
nsGenericDOMDataNode::GetExistingAttrNameFromQName(const nsAString& aStr) const
{
  return nsnull;
}

nsresult
nsGenericDOMDataNode::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttr,
                              nsIAtom* aPrefix, const nsAString& aValue,
                              PRBool aNotify)
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttr,
                                PRBool aNotify)
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttr,
                              nsAString& aResult) const
{
  aResult.Truncate();

  return NS_CONTENT_ATTR_NOT_THERE;
}

PRBool
nsGenericDOMDataNode::HasAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute) const
{
  return PR_FALSE;
}

nsresult
nsGenericDOMDataNode::GetAttrNameAt(PRUint32 aIndex, PRInt32* aNameSpaceID,
                                    nsIAtom** aName, nsIAtom** aPrefix) const
{
  *aNameSpaceID = kNameSpaceID_None;
  *aName = nsnull;
  *aPrefix = nsnull;

  return NS_ERROR_ILLEGAL_VALUE;
}

PRUint32
nsGenericDOMDataNode::GetAttrCount() const
{
  return 0;
}

nsresult
nsGenericDOMDataNode::HandleDOMEvent(nsPresContext* aPresContext,
                                     nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                     PRUint32 aFlags,
                                     nsEventStatus* aEventStatus)
{
  nsresult ret = NS_OK;
  nsIDOMEvent* domEvent = nsnull;

  PRBool externalDOMEvent = PR_FALSE;

  if (NS_EVENT_FLAG_INIT & aFlags) {
    if (!aDOMEvent) {
      aDOMEvent = &domEvent;
    } else {
      externalDOMEvent = PR_TRUE;
    }

    aEvent->flags |= aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
    aFlags |= NS_EVENT_FLAG_BUBBLE | NS_EVENT_FLAG_CAPTURE;
  }

  nsIContent *parent = GetParent();

  //Capturing stage evaluation
  if (NS_EVENT_FLAG_CAPTURE & aFlags) {
    //Initiate capturing phase.  Special case first call to document
    if (parent) {
      parent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                             aFlags & NS_EVENT_CAPTURE_MASK, aEventStatus);
    }
    else {
      nsIDocument *document = GetCurrentDoc();
      if (document) {
        document->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                 aFlags & NS_EVENT_CAPTURE_MASK,
                                 aEventStatus);
      }
    }
  }

  nsCOMPtr<nsIEventListenerManager> listener_manager;
  LookupListenerManager(getter_AddRefs(listener_manager));

  //Local handling stage
  //Check for null ELM, check if we're a non-bubbling event in the bubbling state (bubbling state
  //is indicated by the presence of the NS_EVENT_FLAG_BUBBLE flag and not the NS_EVENT_FLAG_INIT), and check 
  //if we're a no content dispatch event
  if (listener_manager &&
       !(NS_EVENT_FLAG_CANT_BUBBLE & aEvent->flags && NS_EVENT_FLAG_BUBBLE & aFlags && !(NS_EVENT_FLAG_INIT & aFlags)) &&
       !(aEvent->flags & NS_EVENT_FLAG_NO_CONTENT_DISPATCH)) {
    aEvent->flags |= aFlags;
    listener_manager->HandleEvent(aPresContext, aEvent, aDOMEvent, nsnull,
                                  aFlags, aEventStatus);
    aEvent->flags &= ~aFlags;
  }

  //Bubbling stage
  if (NS_EVENT_FLAG_BUBBLE & aFlags && parent) {
    ret = parent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                 aFlags & NS_EVENT_BUBBLE_MASK, aEventStatus);
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event,
    // release here.

    if (!externalDOMEvent && *aDOMEvent) {
      if (0 != (*aDOMEvent)->Release()) {
        // Okay, so someone in the DOM loop (a listener, JS object)
        // still has a ref to the DOM Event but the internal data
        // hasn't been malloc'd.  Force a copy of the data here so the
        // DOM Event is still valid.

        nsCOMPtr<nsIPrivateDOMEvent> privateEvent =
          do_QueryInterface(*aDOMEvent);

        if (privateEvent) {
          privateEvent->DuplicatePrivateData();
        }
      }
    }

    aDOMEvent = nsnull;
  }

  return ret;
}

PRUint32
nsGenericDOMDataNode::ContentID() const
{
  return 0;
}

nsINodeInfo *
nsGenericDOMDataNode::GetNodeInfo() const
{
  return nsnull;
}

PRUint32
nsGenericDOMDataNode::GetChildCount() const
{
  return 0;
}

nsIContent *
nsGenericDOMDataNode::GetChildAt(PRUint32 aIndex) const
{
  return nsnull;
}

PRInt32
nsGenericDOMDataNode::IndexOf(nsIContent* aPossibleChild) const
{
  return -1;
}

nsresult
nsGenericDOMDataNode::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                    PRBool aNotify, PRBool aDeepSetDocument)
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                    PRBool aDeepSetDocument)
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::RangeAdd(nsIDOMRange* aRange)
{
  // lazy allocation of range list

  if (!nsGenericElement::sRangeListsHash.ops) {
    nsresult rv = nsGenericElement::InitHashes();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  RangeListMapEntry *entry =
    NS_STATIC_CAST(RangeListMapEntry *,
                   PL_DHashTableOperate(&nsGenericElement::sRangeListsHash,
                                        this, PL_DHASH_ADD));

  if (!entry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsVoidArray *range_list = entry->mRangeList;

  if (!range_list) {
    range_list = new nsAutoVoidArray();

    if (!range_list) {
      PL_DHashTableRawRemove(&nsGenericElement::sRangeListsHash, entry);

      return NS_ERROR_OUT_OF_MEMORY;
    }

    entry->mRangeList = range_list;

    SetHasRangeList(PR_TRUE);
  } else {
    // Make sure we don't add a range that is already
    // in the list!
    PRInt32 i = range_list->IndexOf(aRange);

    if (i >= 0) {
      // Range is already in the list, so there is nothing to do!

      return NS_OK;
    }
  }

  // dont need to addref - this call is made by the range object itself
  PRBool rv = range_list->AppendElement(aRange);

  return rv ? NS_OK : NS_ERROR_FAILURE;
}


void
nsGenericDOMDataNode::RangeRemove(nsIDOMRange* aRange)
{
  RangeListMapEntry *entry = nsnull;

  if (HasRangeList()) {
    entry =
      NS_STATIC_CAST(RangeListMapEntry *,
                     PL_DHashTableOperate(&nsGenericElement::sRangeListsHash,
                                          this, PL_DHASH_LOOKUP));
  }

  if (entry && PL_DHASH_ENTRY_IS_BUSY(entry)) {
    // dont need to release - this call is made by the range object
    // itself
    PRBool rv = entry->mRangeList->RemoveElement(aRange);

    if (rv) {
      if (entry->mRangeList->Count() == 0) {
        PL_DHashTableRawRemove(&nsGenericElement::sRangeListsHash, entry);

        SetHasRangeList(PR_FALSE);
      }
    }
  }
}

const nsVoidArray *
nsGenericDOMDataNode::GetRangeList() const
{
  return LookupRangeList();
}

nsIContent *
nsGenericDOMDataNode::GetBindingParent() const
{
  nsIContent* parent = GetParent();
  return parent ? parent->GetBindingParent() : nsnull;
}

nsresult
nsGenericDOMDataNode::SetBindingParent(nsIContent* aParent)
{
  return NS_OK;
}

PRBool
nsGenericDOMDataNode::IsContentOfType(PRUint32 aFlags) const
{
  return PR_FALSE;
}

#ifdef DEBUG
void
nsGenericDOMDataNode::List(FILE* out, PRInt32 aIndent) const
{
}

void
nsGenericDOMDataNode::DumpContent(FILE* out, PRInt32 aIndent,
                                  PRBool aDumpAll) const 
{
}
#endif

already_AddRefed<nsIURI>
nsGenericDOMDataNode::GetBaseURI() const
{
  // DOM Data Node inherits the base from its parent element/document
  nsIContent *parent = GetParent();
  if (parent) {
    return parent->GetBaseURI();
  }

  nsIURI *uri;
  nsIDocument *document = ParentPtrBitsAsDocument();
  if (document) {
    NS_IF_ADDREF(uri = document->GetBaseURI());
  }
  else {
    uri = nsnull;
  }

  return uri;
}

//----------------------------------------------------------------------

// Implementation of the nsIDOMText interface

nsresult
nsGenericDOMDataNode::SplitText(PRUint32 aOffset, nsIDOMText** aReturn)
{
  nsresult rv = NS_OK;
  nsAutoString cutText;
  PRUint32 length = TextLength();

  if (aOffset > length) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  rv = SubstringData(aOffset, length - aOffset, cutText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = DeleteData(aOffset, length - aOffset);
  if (NS_FAILED(rv)) {
    return rv;
  }

  /*
   * Use CloneContent() for creating the new node so that the new node is of
   * same class as this node!
   */

  nsCOMPtr<nsITextContent> newContent = CloneContent(PR_FALSE, nsnull);
  if (!newContent) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  newContent->SetText(cutText, PR_TRUE);

  nsIContent* parent = GetParent();

  if (parent) {
    PRInt32 index = parent->IndexOf(this);

    nsCOMPtr<nsIContent> content(do_QueryInterface(newContent));

    parent->InsertChildAt(content, index+1, PR_TRUE, PR_FALSE);
  }

  // XXX Shouldn't we handle the case where this is a child of the document?

  return CallQueryInterface(newContent, aReturn);
}

//----------------------------------------------------------------------

// Implementation of the nsITextContent interface

const nsTextFragment *
nsGenericDOMDataNode::Text()
{
  return &mText;
}

PRUint32
nsGenericDOMDataNode::TextLength()
{
  return mText.GetLength();
}

void
nsGenericDOMDataNode::SetText(const PRUnichar* aBuffer,
                              PRUint32 aLength,
                              PRBool aNotify)
{
  if (!aBuffer) {
    NS_ERROR("Null buffer passed to SetText()!");

    return;
  }

  nsIDocument *document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

  PRBool haveMutationListeners =
    document && nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED);

  nsCOMPtr<nsIAtom> oldValue;
  if (haveMutationListeners) {
    oldValue = GetCurrentValueAtom();
  }
    
  mText.SetTo(aBuffer, aLength);

  SetBidiStatus();

  if (haveMutationListeners) {
    nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(this));
    nsMutationEvent mutation(NS_MUTATION_CHARACTERDATAMODIFIED, node);

    mutation.mPrevAttrValue = oldValue;
    if (aLength > 0) {
      // Must use Substring() since nsDependentString() requires null
      // terminated strings.
      mutation.mNewAttrValue =
        do_GetAtom(Substring(aBuffer, aBuffer + aLength));
    }

    nsEventStatus status = nsEventStatus_eIgnore;
    HandleDOMEvent(nsnull, &mutation, nsnull,
                   NS_EVENT_FLAG_INIT, &status);
  }

  // Trigger a reflow
  if (aNotify && document) {
    document->CharacterDataChanged(this, PR_FALSE);
  }
}

void
nsGenericDOMDataNode::SetText(const char* aBuffer, PRUint32 aLength,
                              PRBool aNotify)
{
  if (!aBuffer) {
    NS_ERROR("Null buffer passed to SetText()!");

    return;
  }

  nsIDocument *document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

  PRBool haveMutationListeners =
    document && nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED);

  nsCOMPtr<nsIAtom> oldValue;
  if (haveMutationListeners) {
    oldValue = GetCurrentValueAtom();
  }
    
  mText.SetTo(aBuffer, aLength);

  if (haveMutationListeners) {
    nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(this));
    nsMutationEvent mutation(NS_MUTATION_CHARACTERDATAMODIFIED, node);

    mutation.mPrevAttrValue = oldValue;
    if (aLength > 0) {
      // Must use Substring() since nsDependentCString() requires null
      // terminated strings.
      mutation.mNewAttrValue =
        do_GetAtom(Substring(aBuffer, aBuffer + aLength));
    }

    nsEventStatus status = nsEventStatus_eIgnore;
    HandleDOMEvent(nsnull, &mutation, nsnull,
                   NS_EVENT_FLAG_INIT, &status);
  }

  // Trigger a reflow
  if (aNotify && document) {
    document->CharacterDataChanged(this, PR_FALSE);
  }
}

void
nsGenericDOMDataNode::SetText(const nsAString& aStr,
                              PRBool aNotify)
{
  nsIDocument *document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

  PRBool haveMutationListeners =
    document && nsGenericElement::HasMutationListeners(this, NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED);

  nsCOMPtr<nsIAtom> oldValue;
  if (haveMutationListeners) {
    oldValue = GetCurrentValueAtom();
  }
    
  mText = aStr;

  SetBidiStatus();

  if (haveMutationListeners) {
    nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(this));
    nsMutationEvent mutation(NS_MUTATION_CHARACTERDATAMODIFIED, node);

    mutation.mPrevAttrValue = oldValue;
    if (!aStr.IsEmpty())
      mutation.mNewAttrValue = do_GetAtom(aStr);
    nsEventStatus status = nsEventStatus_eIgnore;
    HandleDOMEvent(nsnull, &mutation, nsnull,
                   NS_EVENT_FLAG_INIT, &status);
  }

  // Trigger a reflow
  if (aNotify && document) {
    document->CharacterDataChanged(this, PR_FALSE);
  }
}

PRBool
nsGenericDOMDataNode::IsOnlyWhitespace()
{
  nsTextFragment& frag = mText;
  if (frag.Is2b()) {
    const PRUnichar* cp = frag.Get2b();
    const PRUnichar* end = cp + frag.GetLength();

    while (cp < end) {
      PRUnichar ch = *cp++;

      if (!XP_IS_SPACE(ch)) {
        return PR_FALSE;
      }
    }
  } else {
    const char* cp = frag.Get1b();
    const char* end = cp + frag.GetLength();

    while (cp < end) {
      PRUnichar ch = PRUnichar(*(unsigned char*)cp);
      ++cp;

      if (!XP_IS_SPACE(ch)) {
        return PR_FALSE;
      }
    }
  }

  return PR_TRUE;
}

void
nsGenericDOMDataNode::AppendTextTo(nsAString& aResult)
{
  mText.AppendTo(aResult);
}

void
nsGenericDOMDataNode::LookupListenerManager(nsIEventListenerManager **aListenerManager) const
{
  *aListenerManager = nsnull;

  if (!HasEventListenerManager()) {
    return;
  }

  EventListenerManagerMapEntry *entry =
    NS_STATIC_CAST(EventListenerManagerMapEntry *,
                   PL_DHashTableOperate(&nsGenericElement::
                                        sEventListenerManagersHash, this,
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
    *aListenerManager = entry->mListenerManager;
    NS_ADDREF(*aListenerManager);
  }
}

nsVoidArray *
nsGenericDOMDataNode::LookupRangeList() const
{
  if (!HasRangeList()) {
    return nsnull;
  }

  RangeListMapEntry *entry =
    NS_STATIC_CAST(RangeListMapEntry *,
                   PL_DHashTableOperate(&nsGenericElement::sRangeListsHash,
                                        this, PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry)) {
    return entry->mRangeList;
  }

  return nsnull;
}

void nsGenericDOMDataNode::SetBidiStatus()
{
  nsIDocument *document = GetCurrentDoc();
  if (document && document->GetBidiEnabled()) {
    // OK, we already know it's Bidi, so we won't test again
    return;
  }

  mText.SetBidiFlag();

  if (document && mText.IsBidi()) {
    document->SetBidiEnabled(PR_TRUE);
  }
}

already_AddRefed<nsIAtom>
nsGenericDOMDataNode::GetCurrentValueAtom()
{
  nsAutoString val;
  GetData(val);
  return NS_NewAtom(val);
}

already_AddRefed<nsITextContent> 
nsGenericDOMDataNode::CloneContent(PRBool aCloneText,
                                   nsIDocument *aOwnerDocument)
{
  NS_ERROR("Huh, this shouldn't be called!");

  return nsnull;
}
