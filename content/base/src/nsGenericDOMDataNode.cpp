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
 * Base class for DOM Core's nsIDOMComment, nsIDOMDocumentType, nsIDOMText,
 * nsIDOMCDATASection, and nsIDOMProcessingInstruction nodes.
 */

#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMDocument.h"
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
#include "nsIDOMUserDataHandler.h"
#include "nsChangeHint.h"
#include "nsEventDispatcher.h"
#include "nsCOMArray.h"
#include "nsNodeUtils.h"
#include "nsBindingManager.h"
#include "nsCCUncollectableMarker.h"
#include "mozAutoDocUpdate.h"

#include "pldhash.h"
#include "prprf.h"

nsGenericDOMDataNode::nsGenericDOMDataNode(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsIContent(aNodeInfo)
{
}

nsGenericDOMDataNode::~nsGenericDOMDataNode()
{
  NS_PRECONDITION(!IsInDoc(),
                  "Please remove this from the document properly");
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGenericDOMDataNode)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsGenericDOMDataNode)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsGenericDOMDataNode)
  // Always need to traverse script objects, so do that before we check
  // if we're uncollectable.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS

  nsIDocument* currentDoc = tmp->GetCurrentDoc();
  if (currentDoc && nsCCUncollectableMarker::InGeneration(
                      cb, currentDoc->GetMarkedCCGeneration())) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mNodeInfo)

  nsIDocument* ownerDoc = tmp->GetOwnerDoc();
  if (ownerDoc) {
    ownerDoc->BindingManager()->Traverse(tmp, cb);
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_LISTENERMANAGER
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_USERDATA
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_BEGIN(nsGenericDOMDataNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_ROOT_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGenericDOMDataNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_LISTENERMANAGER
  NS_IMPL_CYCLE_COLLECTION_UNLINK_USERDATA
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN(nsGenericDOMDataNode)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsGenericDOMDataNode)
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsINode)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMEventTarget,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3EventTarget,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMNSEventTarget,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsISupportsWeakReference,
                                 new nsNodeSupportsWeakRefTearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3Node, new nsNode3Tearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMXPathNSResolver,
                                 new nsNode3Tearoff(this))
  // nsNodeSH::PreCreate() depends on the identity pointer being the
  // same as nsINode (which nsIContent inherits), so if you change the
  // below line, make sure nsNodeSH::PreCreate() still does the right
  // thing!
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsGenericDOMDataNode, nsIContent)
NS_IMPL_CYCLE_COLLECTING_RELEASE_FULL(nsGenericDOMDataNode, nsIContent,
                                      nsNodeUtils::LastRelease(this))


nsresult
nsGenericDOMDataNode::GetNodeValue(nsAString& aNodeValue)
{
  return GetData(aNodeValue);
}

nsresult
nsGenericDOMDataNode::SetNodeValue(const nsAString& aNodeValue)
{
  return SetTextInternal(0, mText.GetLength(), aNodeValue.BeginReading(),
                         aNodeValue.Length(), PR_TRUE);
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
  return nsGenericElement::InternalIsSupported(static_cast<nsIContent*>(this),
                                               aFeature, aVersion, aReturn);
}

//----------------------------------------------------------------------

// Implementation of nsIDOMCharacterData

nsresult
nsGenericDOMDataNode::GetData(nsAString& aData) const
{
  if (mText.Is2b()) {
    aData.Assign(mText.Get2b(), mText.GetLength());
  } else {
    // Must use Substring() since nsDependentCString() requires null
    // terminated strings.

    const char *data = mText.Get1b();

    if (data) {
      CopyASCIItoUTF16(Substring(data, data + mText.GetLength()), aData);
    } else {
      aData.Truncate();
    }
  }

  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SetData(const nsAString& aData)
{
  return SetTextInternal(0, mText.GetLength(), aData.BeginReading(),
                         aData.Length(), PR_TRUE);
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

  PRUint32 textLength = mText.GetLength();
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
    CopyASCIItoUTF16(Substring(data, data + amount), aReturn);
  }

  return NS_OK;
}

//----------------------------------------------------------------------

nsresult
nsGenericDOMDataNode::AppendData(const nsAString& aData)
{
  return SetTextInternal(mText.GetLength(), 0, aData.BeginReading(),
                         aData.Length(), PR_TRUE);
}

nsresult
nsGenericDOMDataNode::InsertData(PRUint32 aOffset,
                                 const nsAString& aData)
{
  return SetTextInternal(aOffset, 0, aData.BeginReading(),
                         aData.Length(), PR_TRUE);
}

nsresult
nsGenericDOMDataNode::DeleteData(PRUint32 aOffset, PRUint32 aCount)
{
  return SetTextInternal(aOffset, aCount, nsnull, 0, PR_TRUE);
}

nsresult
nsGenericDOMDataNode::ReplaceData(PRUint32 aOffset, PRUint32 aCount,
                                  const nsAString& aData)
{
  return SetTextInternal(aOffset, aCount, aData.BeginReading(),
                         aData.Length(), PR_TRUE);
}

nsresult
nsGenericDOMDataNode::SetTextInternal(PRUint32 aOffset, PRUint32 aCount,
                                      const PRUnichar* aBuffer,
                                      PRUint32 aLength, PRBool aNotify)
{
  NS_PRECONDITION(aBuffer || !aLength,
                  "Null buffer passed to SetTextInternal!");

  // sanitize arguments
  PRUint32 textLength = mText.GetLength();
  if (aOffset > textLength) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  if (aCount > textLength - aOffset) {
    aCount = textLength - aOffset;
  }

  PRUint32 endOffset = aOffset + aCount;

  // Make sure the text fragment can hold the new data.
  if (aLength > aCount && !mText.CanGrowBy(aLength - aCount)) {
    // This exception isn't per spec, but the spec doesn't actually
    // say what to do here.

    return NS_ERROR_DOM_DOMSTRING_SIZE_ERR;
  }

  nsIDocument *document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

  PRBool haveMutationListeners = aNotify &&
    nsContentUtils::HasMutationListeners(this,
      NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED,
      this);

  nsCOMPtr<nsIAtom> oldValue;
  if (haveMutationListeners) {
    oldValue = GetCurrentValueAtom();
  }
    
  if (aNotify) {
    CharacterDataChangeInfo info = {
      aOffset == textLength,
      aOffset,
      endOffset,
      aLength
    };
    nsNodeUtils::CharacterDataWillChange(this, &info);
  }

  if (aOffset == 0 && endOffset == textLength) {
    // Replacing whole text or old text was empty
    mText.SetTo(aBuffer, aLength);
  }
  else if (aOffset == textLength) {
    // Appending to existing
    mText.Append(aBuffer, aLength);
  }
  else {
    // Merging old and new

    // Allocate new buffer
    PRInt32 newLength = textLength - aCount + aLength;
    PRUnichar* to = new PRUnichar[newLength];
    NS_ENSURE_TRUE(to, NS_ERROR_OUT_OF_MEMORY);

    // Copy over appropriate data
    if (aOffset) {
      mText.CopyTo(to, 0, aOffset);
    }
    if (aLength) {
      memcpy(to + aOffset, aBuffer, aLength * sizeof(PRUnichar));
    }
    if (endOffset != textLength) {
      mText.CopyTo(to + aOffset + aLength, endOffset, textLength - endOffset);
    }

    // XXX Add OOM checking to this
    mText.SetTo(to, newLength);

    delete [] to;
  }

  UpdateBidiStatus(aBuffer, aLength);

  // Notify observers
  if (aNotify) {
    CharacterDataChangeInfo info = {
      aOffset == textLength,
      aOffset,
      endOffset,
      aLength
    };
    nsNodeUtils::CharacterDataChanged(this, &info);

    if (haveMutationListeners) {
      mozAutoRemovableBlockerRemover blockerRemover(GetOwnerDoc());

      nsMutationEvent mutation(PR_TRUE, NS_MUTATION_CHARACTERDATAMODIFIED);

      mutation.mPrevAttrValue = oldValue;
      if (aLength > 0) {
        nsAutoString val;
        mText.AppendTo(val);
        mutation.mNewAttrValue = do_GetAtom(val);
      }

      mozAutoSubtreeModified subtree(GetOwnerDoc(), this);
      nsEventDispatcher::Dispatch(this, nsnull, &mutation);
    }
  }

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
      if (ch == '&') {
        aBuf.AppendLiteral("&amp;");
      } else if (ch == '<') {
        aBuf.AppendLiteral("&lt;");
      } else if (ch == '>') {
        aBuf.AppendLiteral("&gt;");
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
      if (ch == '&') {
        aBuf.AppendLiteral("&amp;");
      } else if (ch == '<') {
        aBuf.AppendLiteral("&lt;");
      } else if (ch == '>') {
        aBuf.AppendLiteral("&gt;");
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


nsresult
nsGenericDOMDataNode::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                 nsIContent* aBindingParent,
                                 PRBool aCompileEventHandlers)
{
  NS_PRECONDITION(aParent || aDocument, "Must have document if no parent!");
  NS_PRECONDITION(HasSameOwnerDoc(NODE_FROM(aParent, aDocument)),
                  "Must have the same owner document");
  NS_PRECONDITION(!aParent || aDocument == aParent->GetCurrentDoc(),
                  "aDocument must be current doc of aParent");
  NS_PRECONDITION(!GetCurrentDoc() && !IsInDoc(),
                  "Already have a document.  Unbind first!");
  // Note that as we recurse into the kids, they'll have a non-null parent.  So
  // only assert if our parent is _changing_ while we have a parent.
  NS_PRECONDITION(!GetParent() || aParent == GetParent(),
                  "Already have a parent.  Unbind first!");
  NS_PRECONDITION(!GetBindingParent() ||
                  aBindingParent == GetBindingParent() ||
                  (!aBindingParent && aParent &&
                   aParent->GetBindingParent() == GetBindingParent()),
                  "Already have a binding parent.  Unbind first!");
  NS_PRECONDITION(aBindingParent != this,
                  "Content must not be its own binding parent");
  NS_PRECONDITION(!IsRootOfNativeAnonymousSubtree() || 
                  aBindingParent == aParent,
                  "Native anonymous content must have its parent as its "
                  "own binding parent");

  if (!aBindingParent && aParent) {
    aBindingParent = aParent->GetBindingParent();
  }

  // First set the binding parent
  if (aBindingParent) {
    nsDataSlots *slots = GetDataSlots();
    NS_ENSURE_TRUE(slots, NS_ERROR_OUT_OF_MEMORY);

    NS_ASSERTION(IsRootOfNativeAnonymousSubtree() ||
                 !HasFlag(NODE_IS_IN_ANONYMOUS_SUBTREE) ||
                 (aParent && aParent->IsInNativeAnonymousSubtree()),
                 "Trying to re-bind content from native anonymous subtree to "
                 "non-native anonymous parent!");
    slots->mBindingParent = aBindingParent; // Weak, so no addref happens.
    if (aParent->IsInNativeAnonymousSubtree()) {
      SetFlags(NODE_IS_IN_ANONYMOUS_SUBTREE);
    }
  }

  // Set parent
  if (aParent) {
    mParentPtrBits =
      reinterpret_cast<PtrBits>(aParent) | PARENT_BIT_PARENT_IS_CONTENT;
  }
  else {
    mParentPtrBits = reinterpret_cast<PtrBits>(aDocument);
  }

  // XXXbz sXBL/XBL2 issue!

  // Set document
  if (aDocument) {
    // XXX See the comment in nsGenericElement::BindToTree
    mParentPtrBits |= PARENT_BIT_INDOCUMENT;
    if (mText.IsBidi()) {
      aDocument->SetBidiEnabled();
    }
    // Clear the lazy frame construction bits.
    UnsetFlags(NODE_NEEDS_FRAME | NODE_DESCENDANTS_NEED_FRAMES);
  }

  nsNodeUtils::ParentChainChanged(this);

  UpdateEditableState();

  NS_POSTCONDITION(aDocument == GetCurrentDoc(), "Bound to wrong document");
  NS_POSTCONDITION(aParent == GetParent(), "Bound to wrong parent");
  NS_POSTCONDITION(aBindingParent == GetBindingParent(),
                   "Bound to wrong binding parent");

  return NS_OK;
}

void
nsGenericDOMDataNode::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  // Unset frame flags; if we need them again later, they'll get set again.
  UnsetFlags(NS_CREATE_FRAME_IF_NON_WHITESPACE |
             NS_REFRAME_IF_WHITESPACE);
  
  nsIDocument *document = GetCurrentDoc();
  if (document) {
    // Notify XBL- & nsIAnonymousContentCreator-generated
    // anonymous content that the document is changing.
    // This is needed to update the insertion point.
    document->BindingManager()->RemovedFromDocument(this, document);
  }

  mParentPtrBits = aNullParent ? 0 : mParentPtrBits & ~PARENT_BIT_INDOCUMENT;

  nsDataSlots *slots = GetExistingDataSlots();
  if (slots) {
    slots->mBindingParent = nsnull;
  }

  nsNodeUtils::ParentChainChanged(this);
}

already_AddRefed<nsINodeList>
nsGenericDOMDataNode::GetChildren(PRUint32 aFilter)
{
  return nsnull;
}

nsIAtom *
nsGenericDOMDataNode::GetIDAttributeName() const
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

PRBool
nsGenericDOMDataNode::GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttr,
                              nsAString& aResult) const
{
  aResult.Truncate();

  return PR_FALSE;
}

PRBool
nsGenericDOMDataNode::HasAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute) const
{
  return PR_FALSE;
}

const nsAttrName*
nsGenericDOMDataNode::GetAttrNameAt(PRUint32 aIndex) const
{
  return nsnull;
}

PRUint32
nsGenericDOMDataNode::GetAttrCount() const
{
  return 0;
}

nsresult
nsGenericDOMDataNode::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  return nsGenericElement::doPreHandleEvent(this, aVisitor);
}

nsresult
nsGenericDOMDataNode::PostHandleEvent(nsEventChainPostVisitor& /*aVisitor*/)
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::DispatchDOMEvent(nsEvent* aEvent,
                                       nsIDOMEvent* aDOMEvent,
                                       nsPresContext* aPresContext,
                                       nsEventStatus* aEventStatus)
{
  return nsEventDispatcher::DispatchDOMEvent(static_cast<nsINode*>(this),
                                             aEvent, aDOMEvent,
                                             aPresContext, aEventStatus);
}

nsIEventListenerManager*
nsGenericDOMDataNode::GetListenerManager(PRBool aCreateIfNotFound)
{
  return nsContentUtils::GetListenerManager(this, aCreateIfNotFound);
}

nsresult
nsGenericDOMDataNode::AddEventListenerByIID(nsIDOMEventListener *aListener,
                                            const nsIID& aIID)
{
  nsIEventListenerManager* elm = GetListenerManager(PR_TRUE);
  NS_ENSURE_STATE(elm);
  return elm->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
}

nsresult
nsGenericDOMDataNode::RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                               const nsIID& aIID)
{
  nsIEventListenerManager* elm = GetListenerManager(PR_FALSE);
  return elm ?
    elm->RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE) :
    NS_OK;
}

nsresult
nsGenericDOMDataNode::GetSystemEventGroup(nsIDOMEventGroup** aGroup)
{
  nsIEventListenerManager* elm = GetListenerManager(PR_TRUE);
  NS_ENSURE_STATE(elm);
  return elm->GetSystemEventGroupLM(aGroup);
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

nsIContent * const *
nsGenericDOMDataNode::GetChildArray(PRUint32* aChildCount) const
{
  *aChildCount = 0;
  return nsnull;
}

PRInt32
nsGenericDOMDataNode::IndexOf(nsINode* aPossibleChild) const
{
  return -1;
}

nsresult
nsGenericDOMDataNode::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                    PRBool aNotify)
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::RemoveChildAt(PRUint32 aIndex, PRBool aNotify, PRBool aMutationEvent)
{
  return NS_OK;
}

nsIContent *
nsGenericDOMDataNode::GetBindingParent() const
{
  nsDataSlots *slots = GetExistingDataSlots();
  return slots ? slots->mBindingParent : nsnull;
}

PRBool
nsGenericDOMDataNode::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eDATA_NODE));
}

void
nsGenericDOMDataNode::SaveSubtreeState()
{
}

void
nsGenericDOMDataNode::DestroyContent()
{
  // XXX We really should let cycle collection do this, but that currently still
  //     leaks (see https://bugzilla.mozilla.org/show_bug.cgi?id=406684).
  nsContentUtils::ReleaseWrapper(this, this);
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

  nsIDocument *doc = GetOwnerDoc();

  return doc ? doc->GetBaseURI() : nsnull;
}

PRBool
nsGenericDOMDataNode::IsLink(nsIURI** aURI) const
{
  *aURI = nsnull;
  return PR_FALSE;
}

nsINode::nsSlots*
nsGenericDOMDataNode::CreateSlots()
{
  return new nsDataSlots(mFlagsOrSlots);
}

//----------------------------------------------------------------------

// Implementation of the nsIDOMText interface

nsresult
nsGenericDOMDataNode::SplitData(PRUint32 aOffset, nsIContent** aReturn,
                                PRBool aCloneAfterOriginal)
{
  *aReturn = nsnull;
  nsresult rv = NS_OK;
  nsAutoString cutText;
  PRUint32 length = TextLength();

  if (aOffset > length) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  PRUint32 cutStartOffset = aCloneAfterOriginal ? aOffset : 0;
  PRUint32 cutLength = aCloneAfterOriginal ? length - aOffset : aOffset;
  rv = SubstringData(cutStartOffset, cutLength, cutText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = DeleteData(cutStartOffset, cutLength);
  if (NS_FAILED(rv)) {
    return rv;
  }

  /*
   * Use Clone for creating the new node so that the new node is of same class
   * as this node!
   */

  nsCOMPtr<nsIContent> newContent = CloneDataNode(mNodeInfo, PR_FALSE);
  if (!newContent) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  newContent->SetText(cutText, PR_TRUE);

  nsCOMPtr<nsINode> parent = GetNodeParent();

  if (parent) {
    PRInt32 insertionIndex = parent->IndexOf(this);
    if (aCloneAfterOriginal) {
      ++insertionIndex;
    }
    parent->InsertChildAt(newContent, insertionIndex, PR_TRUE);
  }

  newContent.swap(*aReturn);
  return rv;
}

nsresult
nsGenericDOMDataNode::SplitText(PRUint32 aOffset, nsIDOMText** aReturn)
{
  nsCOMPtr<nsIContent> newChild;
  nsresult rv = SplitData(aOffset, getter_AddRefs(newChild));
  if (NS_SUCCEEDED(rv)) {
    rv = CallQueryInterface(newChild, aReturn);
  }
  return rv;
}

//----------------------------------------------------------------------

// Implementation of the nsGenericDOMDataNode nsIDOM3Text tearoff

NS_IMPL_CYCLE_COLLECTION_CLASS(nsText3Tearoff)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsText3Tearoff)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3Text)
NS_INTERFACE_MAP_END_AGGREGATED(mNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsText3Tearoff)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNode)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsText3Tearoff)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mNode, nsIContent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsText3Tearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsText3Tearoff)

NS_IMETHODIMP
nsText3Tearoff::GetIsElementContentWhitespace(PRBool *aReturn)
{
  *aReturn = mNode->IsElementContentWhitespace();
  return NS_OK;
}

NS_IMETHODIMP
nsText3Tearoff::GetWholeText(nsAString& aWholeText)
{
  return mNode->GetWholeText(aWholeText);
}

NS_IMETHODIMP
nsText3Tearoff::ReplaceWholeText(const nsAString& aContent,
                                 nsIDOMText **aReturn)
{
  nsresult rv;
  nsIContent* result = mNode->ReplaceWholeText(PromiseFlatString(aContent),
                                               &rv);
  return result ? CallQueryInterface(result, aReturn) : rv;
}

// Implementation of the nsIDOM3Text interface

/* static */ PRInt32
nsGenericDOMDataNode::FirstLogicallyAdjacentTextNode(nsIContent* aParent,
                                                     PRInt32 aIndex)
{
  while (aIndex-- > 0) {
    nsIContent* sibling = aParent->GetChildAt(aIndex);
    if (!sibling->IsNodeOfType(nsINode::eTEXT))
      return aIndex + 1;
  }
  return 0;
}

/* static */ PRInt32
nsGenericDOMDataNode::LastLogicallyAdjacentTextNode(nsIContent* aParent,
                                                    PRInt32 aIndex,
                                                    PRUint32 aCount)
{
  while (++aIndex < PRInt32(aCount)) {
    nsIContent* sibling = aParent->GetChildAt(aIndex);
    if (!sibling->IsNodeOfType(nsINode::eTEXT))
      return aIndex - 1;
  }
  return aCount - 1;
}

nsresult
nsGenericTextNode::GetWholeText(nsAString& aWholeText)
{
  nsIContent* parent = GetParent();

  // Handle parent-less nodes
  if (!parent)
    return GetData(aWholeText);

  PRInt32 index = parent->IndexOf(this);
  NS_WARN_IF_FALSE(index >= 0,
                   "Trying to use .wholeText with an anonymous"
                    "text node child of a binding parent?");
  NS_ENSURE_TRUE(index >= 0, NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  PRInt32 first =
    FirstLogicallyAdjacentTextNode(parent, index);
  PRInt32 last =
    LastLogicallyAdjacentTextNode(parent, index, parent->GetChildCount());

  aWholeText.Truncate();

  nsCOMPtr<nsIDOMText> node;
  nsAutoString tmp;
  do {
    node = do_QueryInterface(parent->GetChildAt(first));
    node->GetData(tmp);
    aWholeText.Append(tmp);
  } while (first++ < last);

  return NS_OK;
}

nsIContent*
nsGenericTextNode::ReplaceWholeText(const nsAFlatString& aContent,
                                    nsresult* aResult)
{
  *aResult = NS_OK;

  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(GetOwnerDoc(), nsnull);
  mozAutoDocUpdate updateBatch(GetCurrentDoc(), UPDATE_CONTENT_MODEL, PR_TRUE);

  nsCOMPtr<nsIContent> parent = GetParent();

  // Handle parent-less nodes
  if (!parent) {
    if (aContent.IsEmpty()) {
      return nsnull;
    }

    SetText(aContent.get(), aContent.Length(), PR_TRUE);
    return this;
  }

  PRInt32 index = parent->IndexOf(this);
  if (index < 0) {
    NS_WARNING("Trying to use .replaceWholeText with an anonymous text node "
               "child of a binding parent?");
    *aResult = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    return nsnull;
  }

  // We don't support entity references or read-only nodes, so remove the
  // logically adjacent text nodes (which therefore must all be siblings of
  // this) and set this one to the provided text, if that text isn't empty.
  PRInt32 first =
    FirstLogicallyAdjacentTextNode(parent, index);
  PRInt32 last =
    LastLogicallyAdjacentTextNode(parent, index, parent->GetChildCount());

  do {
    if (last == index && !aContent.IsEmpty())
      continue;

    parent->RemoveChildAt(last, PR_TRUE);
  } while (last-- > first);

  // Empty string means we removed this node too.
  if (aContent.IsEmpty()) {
    return nsnull;
  }

  SetText(aContent.get(), aContent.Length(), PR_TRUE);
  return this;
}

//----------------------------------------------------------------------

// Implementation of the nsIContent interface text functions

const nsTextFragment *
nsGenericDOMDataNode::GetText()
{
  return &mText;
}

PRUint32
nsGenericDOMDataNode::TextLength()
{
  return mText.GetLength();
}

nsresult
nsGenericDOMDataNode::SetText(const PRUnichar* aBuffer,
                              PRUint32 aLength,
                              PRBool aNotify)
{
  return SetTextInternal(0, mText.GetLength(), aBuffer, aLength, aNotify);
}

nsresult
nsGenericDOMDataNode::AppendText(const PRUnichar* aBuffer,
                                 PRUint32 aLength,
                                 PRBool aNotify)
{
  return SetTextInternal(mText.GetLength(), 0, aBuffer, aLength, aNotify);
}

PRBool
nsGenericDOMDataNode::TextIsOnlyWhitespace()
{
  if (mText.Is2b()) {
    // The fragment contains non-8bit characters and such characters
    // are never considered whitespace.
    return PR_FALSE;
  }

  const char* cp = mText.Get1b();
  const char* end = cp + mText.GetLength();

  while (cp < end) {
    char ch = *cp;

    if (!XP_IS_SPACE(ch)) {
      return PR_FALSE;
    }

    ++cp;
  }

  return PR_TRUE;
}

void
nsGenericDOMDataNode::AppendTextTo(nsAString& aResult)
{
  mText.AppendTo(aResult);
}

void nsGenericDOMDataNode::UpdateBidiStatus(const PRUnichar* aBuffer, PRUint32 aLength)
{
  nsIDocument *document = GetCurrentDoc();
  if (document && document->GetBidiEnabled()) {
    // OK, we already know it's Bidi, so we won't test again
    return;
  }

  mText.UpdateBidiFlag(aBuffer, aLength);

  if (document && mText.IsBidi()) {
    document->SetBidiEnabled();
  }
}

already_AddRefed<nsIAtom>
nsGenericDOMDataNode::GetCurrentValueAtom()
{
  nsAutoString val;
  GetData(val);
  return NS_NewAtom(val);
}

nsIAtom*
nsGenericDOMDataNode::DoGetID() const
{
  return nsnull;
}

const nsAttrValue*
nsGenericDOMDataNode::DoGetClasses() const
{
  NS_NOTREACHED("Shouldn't ever be called");
  return nsnull;
}

NS_IMETHODIMP
nsGenericDOMDataNode::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  return NS_OK;
}

#ifdef MOZ_SMIL
nsresult
nsGenericDOMDataNode::GetSMILOverrideStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  *aStyle = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsICSSStyleRule*
nsGenericDOMDataNode::GetSMILOverrideStyleRule()
{
  return nsnull;
}

nsresult
nsGenericDOMDataNode::SetSMILOverrideStyleRule(nsICSSStyleRule* aStyleRule,
                                               PRBool aNotify)
{
  NS_NOTREACHED("How come we're setting SMILOverrideStyle on a non-element?");
  return NS_ERROR_UNEXPECTED;
}
#endif // MOZ_SMIL

nsICSSStyleRule*
nsGenericDOMDataNode::GetInlineStyleRule()
{
  return nsnull;
}

NS_IMETHODIMP
nsGenericDOMDataNode::SetInlineStyleRule(nsICSSStyleRule* aStyleRule,
                                         PRBool aNotify)
{
  NS_NOTREACHED("How come we're setting inline style on a non-element?");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP_(PRBool)
nsGenericDOMDataNode::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  return PR_FALSE;
}

nsChangeHint
nsGenericDOMDataNode::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                             PRInt32 aModType) const
{
  NS_NOTREACHED("Shouldn't be calling this!");
  return nsChangeHint(0);
}

nsIAtom*
nsGenericDOMDataNode::GetClassAttributeName() const
{
  return nsnull;
}
