/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for DOM Core's nsIDOMComment, nsIDOMDocumentType, nsIDOMText,
 * nsIDOMCDATASection, and nsIDOMProcessingInstruction nodes.
 */

#include "mozilla/DebugOnly.h"

#include "nsGenericDOMDataNode.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsReadableUtils.h"
#include "mozilla/InternalMutationEvent.h"
#include "nsIURI.h"
#include "nsIDOMEvent.h"
#include "nsIDOMText.h"
#include "nsCOMPtr.h"
#include "nsDOMString.h"
#include "nsIDOMUserDataHandler.h"
#include "nsChangeHint.h"
#include "nsCOMArray.h"
#include "nsNodeUtils.h"
#include "mozilla/dom/DirectionalityUtils.h"
#include "nsBindingManager.h"
#include "nsCCUncollectableMarker.h"
#include "mozAutoDocUpdate.h"

#include "pldhash.h"
#include "prprf.h"
#include "nsWrapperCacheInlines.h"

using namespace mozilla;
using namespace mozilla::dom;

nsGenericDOMDataNode::nsGenericDOMDataNode(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : nsIContent(aNodeInfo)
{
  NS_ABORT_IF_FALSE(mNodeInfo->NodeType() == nsIDOMNode::TEXT_NODE ||
                    mNodeInfo->NodeType() == nsIDOMNode::CDATA_SECTION_NODE ||
                    mNodeInfo->NodeType() == nsIDOMNode::COMMENT_NODE ||
                    mNodeInfo->NodeType() ==
                      nsIDOMNode::PROCESSING_INSTRUCTION_NODE ||
                    mNodeInfo->NodeType() == nsIDOMNode::DOCUMENT_TYPE_NODE,
                    "Bad NodeType in aNodeInfo");
}

nsGenericDOMDataNode::nsGenericDOMDataNode(already_AddRefed<nsINodeInfo>&& aNodeInfo)
  : nsIContent(aNodeInfo)
{
  NS_ABORT_IF_FALSE(mNodeInfo->NodeType() == nsIDOMNode::TEXT_NODE ||
                    mNodeInfo->NodeType() == nsIDOMNode::CDATA_SECTION_NODE ||
                    mNodeInfo->NodeType() == nsIDOMNode::COMMENT_NODE ||
                    mNodeInfo->NodeType() ==
                      nsIDOMNode::PROCESSING_INSTRUCTION_NODE ||
                    mNodeInfo->NodeType() == nsIDOMNode::DOCUMENT_TYPE_NODE,
                    "Bad NodeType in aNodeInfo");
}

nsGenericDOMDataNode::~nsGenericDOMDataNode()
{
  NS_PRECONDITION(!IsInDoc(),
                  "Please remove this from the document properly");
  if (GetParent()) {
    NS_RELEASE(mParent);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGenericDOMDataNode)

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(nsGenericDOMDataNode)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsGenericDOMDataNode)
  return Element::CanSkip(tmp, aRemovingAllowed);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsGenericDOMDataNode)
  return Element::CanSkipInCC(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsGenericDOMDataNode)
  return Element::CanSkipThis(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsGenericDOMDataNode)
  // Always need to traverse script objects, so do that before we check
  // if we're uncollectable.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS

  if (!nsINode::Traverse(tmp, cb)) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }

  nsDataSlots *slots = tmp->GetExistingDataSlots();
  if (slots) {
    slots->Traverse(cb);
  }

  tmp->OwnerDoc()->BindingManager()->Traverse(tmp, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGenericDOMDataNode)
  nsINode::Unlink(tmp);

  nsDataSlots *slots = tmp->GetExistingDataSlots();
  if (slots) {
    slots->Unlink();
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN(nsGenericDOMDataNode)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsGenericDOMDataNode)
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsINode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsISupportsWeakReference,
                                 new nsNodeSupportsWeakRefTearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMXPathNSResolver,
                                 new nsNode3Tearoff(this))
  // DOM bindings depend on the identity pointer being the
  // same as nsINode (which nsIContent inherits).
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsGenericDOMDataNode)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(nsGenericDOMDataNode,
                                                   nsNodeUtils::LastRelease(this))


void
nsGenericDOMDataNode::GetNodeValueInternal(nsAString& aNodeValue)
{
  DebugOnly<nsresult> rv = GetData(aNodeValue);
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetData() failed!");
}

void
nsGenericDOMDataNode::SetNodeValueInternal(const nsAString& aNodeValue,
                                           ErrorResult& aError)
{
  aError = SetTextInternal(0, mText.GetLength(), aNodeValue.BeginReading(),
                           aNodeValue.Length(), true);
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
                         aData.Length(), true);
}

nsresult
nsGenericDOMDataNode::GetLength(uint32_t* aLength)
{
  *aLength = mText.GetLength();
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::SubstringData(uint32_t aStart, uint32_t aCount,
                                    nsAString& aReturn)
{
  ErrorResult rv;
  SubstringData(aStart, aCount, aReturn, rv);
  return rv.ErrorCode();
}

void
nsGenericDOMDataNode::SubstringData(uint32_t aStart, uint32_t aCount,
                                    nsAString& aReturn, ErrorResult& rv)
{
  aReturn.Truncate();

  uint32_t textLength = mText.GetLength();
  if (aStart > textLength) {
    rv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  uint32_t amount = aCount;
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
}

NS_IMETHODIMP
nsGenericDOMDataNode::MozRemove()
{
  Remove();
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult
nsGenericDOMDataNode::AppendData(const nsAString& aData)
{
  return SetTextInternal(mText.GetLength(), 0, aData.BeginReading(),
                         aData.Length(), true);
}

nsresult
nsGenericDOMDataNode::InsertData(uint32_t aOffset,
                                 const nsAString& aData)
{
  return SetTextInternal(aOffset, 0, aData.BeginReading(),
                         aData.Length(), true);
}

nsresult
nsGenericDOMDataNode::DeleteData(uint32_t aOffset, uint32_t aCount)
{
  return SetTextInternal(aOffset, aCount, nullptr, 0, true);
}

nsresult
nsGenericDOMDataNode::ReplaceData(uint32_t aOffset, uint32_t aCount,
                                  const nsAString& aData)
{
  return SetTextInternal(aOffset, aCount, aData.BeginReading(),
                         aData.Length(), true);
}

nsresult
nsGenericDOMDataNode::SetTextInternal(uint32_t aOffset, uint32_t aCount,
                                      const char16_t* aBuffer,
                                      uint32_t aLength, bool aNotify,
                                      CharacterDataChangeInfo::Details* aDetails)
{
  NS_PRECONDITION(aBuffer || !aLength,
                  "Null buffer passed to SetTextInternal!");

  // sanitize arguments
  uint32_t textLength = mText.GetLength();
  if (aOffset > textLength) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  if (aCount > textLength - aOffset) {
    aCount = textLength - aOffset;
  }

  uint32_t endOffset = aOffset + aCount;

  // Make sure the text fragment can hold the new data.
  if (aLength > aCount && !mText.CanGrowBy(aLength - aCount)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsIDocument *document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

  bool haveMutationListeners = aNotify &&
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
      aLength,
      aDetails
    };
    nsNodeUtils::CharacterDataWillChange(this, &info);
  }

  if (NodeType() == nsIDOMNode::TEXT_NODE) {
    SetDirectionFromChangedTextNode(this, aOffset, aBuffer, aLength, aNotify);
  }

  if (aOffset == 0 && endOffset == textLength) {
    // Replacing whole text or old text was empty.  Don't bother to check for
    // bidi in this string if the document already has bidi enabled.
    bool ok = mText.SetTo(aBuffer, aLength, !document || !document->GetBidiEnabled());
    NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  }
  else if (aOffset == textLength) {
    // Appending to existing
    bool ok = mText.Append(aBuffer, aLength, !document || !document->GetBidiEnabled());
    NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  }
  else {
    // Merging old and new

    // Allocate new buffer
    int32_t newLength = textLength - aCount + aLength;
    char16_t* to = new char16_t[newLength];
    NS_ENSURE_TRUE(to, NS_ERROR_OUT_OF_MEMORY);

    // Copy over appropriate data
    if (aOffset) {
      mText.CopyTo(to, 0, aOffset);
    }
    if (aLength) {
      memcpy(to + aOffset, aBuffer, aLength * sizeof(char16_t));
    }
    if (endOffset != textLength) {
      mText.CopyTo(to + aOffset + aLength, endOffset, textLength - endOffset);
    }

    bool ok = mText.SetTo(to, newLength, !document || !document->GetBidiEnabled());

    delete [] to;

    NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  }

  UnsetFlags(NS_CACHED_TEXT_IS_ONLY_WHITESPACE);

  if (document && mText.IsBidi()) {
    // If we found bidi characters in mText.SetTo() above, indicate that the
    // document contains bidi characters.
    document->SetBidiEnabled();
  }

  // Notify observers
  if (aNotify) {
    CharacterDataChangeInfo info = {
      aOffset == textLength,
      aOffset,
      endOffset,
      aLength,
      aDetails
    };
    nsNodeUtils::CharacterDataChanged(this, &info);

    if (haveMutationListeners) {
      InternalMutationEvent mutation(true, NS_MUTATION_CHARACTERDATAMODIFIED);

      mutation.mPrevAttrValue = oldValue;
      if (aLength > 0) {
        nsAutoString val;
        mText.AppendTo(val);
        mutation.mNewAttrValue = do_GetAtom(val);
      }

      mozAutoSubtreeModified subtree(OwnerDoc(), this);
      (new AsyncEventDispatcher(this, mutation))->RunDOMEventWhenSafe();
    }
  }

  return NS_OK;
}

//----------------------------------------------------------------------

// Implementation of nsIContent

#ifdef DEBUG
void
nsGenericDOMDataNode::ToCString(nsAString& aBuf, int32_t aOffset,
                                int32_t aLen) const
{
  if (mText.Is2b()) {
    const char16_t* cp = mText.Get2b() + aOffset;
    const char16_t* end = cp + aLen;

    while (cp < end) {
      char16_t ch = *cp++;
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
      char16_t ch = *cp++;
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
                                 bool aCompileEventHandlers)
{
  NS_PRECONDITION(aParent || aDocument, "Must have document if no parent!");
  NS_PRECONDITION(NODE_FROM(aParent, aDocument)->OwnerDoc() == OwnerDoc(),
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
    NS_ASSERTION(IsRootOfNativeAnonymousSubtree() ||
                 !HasFlag(NODE_IS_IN_ANONYMOUS_SUBTREE) ||
                 (aParent && aParent->IsInNativeAnonymousSubtree()),
                 "Trying to re-bind content from native anonymous subtree to "
                 "non-native anonymous parent!");
    DataSlots()->mBindingParent = aBindingParent; // Weak, so no addref happens.
    if (aParent->IsInNativeAnonymousSubtree()) {
      SetFlags(NODE_IS_IN_ANONYMOUS_SUBTREE);
    }
    if (aParent->HasFlag(NODE_CHROME_ONLY_ACCESS)) {
      SetFlags(NODE_CHROME_ONLY_ACCESS);
    }
    if (aParent->HasFlag(NODE_IS_IN_SHADOW_TREE)) {
      SetFlags(NODE_IS_IN_SHADOW_TREE);
    }
    ShadowRoot* parentContainingShadow = aParent->GetContainingShadow();
    if (parentContainingShadow) {
      DataSlots()->mContainingShadow = parentContainingShadow;
    }
  }

  // Set parent
  if (aParent) {
    if (!GetParent()) {
      NS_ADDREF(aParent);
    }
    mParent = aParent;
  }
  else {
    mParent = aDocument;
  }
  SetParentIsContent(aParent);

  // XXXbz sXBL/XBL2 issue!

  // Set document
  if (aDocument) {
    // We no longer need to track the subtree pointer (and in fact we'll assert
    // if we do this any later).
    ClearSubtreeRootPointer();

    // XXX See the comment in Element::BindToTree
    SetInDocument();
    if (mText.IsBidi()) {
      aDocument->SetBidiEnabled();
    }
    // Clear the lazy frame construction bits.
    UnsetFlags(NODE_NEEDS_FRAME | NODE_DESCENDANTS_NEED_FRAMES);
  } else {
    // If we're not in the doc, update our subtree pointer.
    SetSubtreeRootPointer(aParent->SubtreeRoot());
  }

  nsNodeUtils::ParentChainChanged(this);

  UpdateEditableState(false);

  NS_POSTCONDITION(aDocument == GetCurrentDoc(), "Bound to wrong document");
  NS_POSTCONDITION(aParent == GetParent(), "Bound to wrong parent");
  NS_POSTCONDITION(aBindingParent == GetBindingParent(),
                   "Bound to wrong binding parent");

  return NS_OK;
}

void
nsGenericDOMDataNode::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // Unset frame flags; if we need them again later, they'll get set again.
  UnsetFlags(NS_CREATE_FRAME_IF_NON_WHITESPACE |
             NS_REFRAME_IF_WHITESPACE |
             // Also unset the shadow tree flag because it can
             // no longer be a descendant of a ShadowRoot.
             NODE_IS_IN_SHADOW_TREE);
  
  nsIDocument *document = GetCurrentDoc();
  if (document) {
    // Notify XBL- & nsIAnonymousContentCreator-generated
    // anonymous content that the document is changing.
    // This is needed to update the insertion point.
    document->BindingManager()->RemovedFromDocument(this, document);
  }

  if (aNullParent) {
    if (GetParent()) {
      NS_RELEASE(mParent);
    } else {
      mParent = nullptr;
    }
    SetParentIsContent(false);
  }
  ClearInDocument();

  // Begin keeping track of our subtree root.
  SetSubtreeRootPointer(aNullParent ? this : mParent->SubtreeRoot());

  nsDataSlots *slots = GetExistingDataSlots();
  if (slots) {
    slots->mBindingParent = nullptr;
    slots->mContainingShadow = nullptr;
  }

  nsNodeUtils::ParentChainChanged(this);
}

already_AddRefed<nsINodeList>
nsGenericDOMDataNode::GetChildren(uint32_t aFilter)
{
  return nullptr;
}

nsIAtom *
nsGenericDOMDataNode::GetIDAttributeName() const
{
  return nullptr;
}

nsresult
nsGenericDOMDataNode::SetAttr(int32_t aNameSpaceID, nsIAtom* aAttr,
                              nsIAtom* aPrefix, const nsAString& aValue,
                              bool aNotify)
{
  return NS_OK;
}

nsresult
nsGenericDOMDataNode::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttr,
                                bool aNotify)
{
  return NS_OK;
}

const nsAttrName*
nsGenericDOMDataNode::GetAttrNameAt(uint32_t aIndex) const
{
  return nullptr;
}

uint32_t
nsGenericDOMDataNode::GetAttrCount() const
{
  return 0;
}

uint32_t
nsGenericDOMDataNode::GetChildCount() const
{
  return 0;
}

nsIContent *
nsGenericDOMDataNode::GetChildAt(uint32_t aIndex) const
{
  return nullptr;
}

nsIContent * const *
nsGenericDOMDataNode::GetChildArray(uint32_t* aChildCount) const
{
  *aChildCount = 0;
  return nullptr;
}

int32_t
nsGenericDOMDataNode::IndexOf(const nsINode* aPossibleChild) const
{
  return -1;
}

nsresult
nsGenericDOMDataNode::InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                                    bool aNotify)
{
  return NS_OK;
}

void
nsGenericDOMDataNode::RemoveChildAt(uint32_t aIndex, bool aNotify)
{
}

nsIContent *
nsGenericDOMDataNode::GetBindingParent() const
{
  nsDataSlots *slots = GetExistingDataSlots();
  return slots ? slots->mBindingParent : nullptr;
}

ShadowRoot *
nsGenericDOMDataNode::GetShadowRoot() const
{
  return nullptr;
}

ShadowRoot *
nsGenericDOMDataNode::GetContainingShadow() const
{
  nsDataSlots *slots = GetExistingDataSlots();
  return slots ? slots->mContainingShadow : nullptr;
}

void
nsGenericDOMDataNode::SetShadowRoot(ShadowRoot* aShadowRoot)
{
}

nsXBLBinding *
nsGenericDOMDataNode::GetXBLBinding() const
{
  return nullptr;
}

void
nsGenericDOMDataNode::SetXBLBinding(nsXBLBinding* aBinding,
                                    nsBindingManager* aOldBindingManager)
{
}

nsIContent *
nsGenericDOMDataNode::GetXBLInsertionParent() const
{
  if (HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
    nsDataSlots *slots = GetExistingDataSlots();
    if (slots) {
      return slots->mXBLInsertionParent;
    }
  }

  return nullptr;
}

void
nsGenericDOMDataNode::SetXBLInsertionParent(nsIContent* aContent)
{
  nsDataSlots *slots = DataSlots();
  if (aContent) {
    SetFlags(NODE_MAY_BE_IN_BINDING_MNGR);
  }
  slots->mXBLInsertionParent = aContent;
}

CustomElementData *
nsGenericDOMDataNode::GetCustomElementData() const
{
  return nullptr;
}

void
nsGenericDOMDataNode::SetCustomElementData(CustomElementData* aData)
{
}

bool
nsGenericDOMDataNode::IsNodeOfType(uint32_t aFlags) const
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
  ReleaseWrapper(this);
}

#ifdef DEBUG
void
nsGenericDOMDataNode::List(FILE* out, int32_t aIndent) const
{
}

void
nsGenericDOMDataNode::DumpContent(FILE* out, int32_t aIndent,
                                  bool aDumpAll) const 
{
}
#endif

bool
nsGenericDOMDataNode::IsLink(nsIURI** aURI) const
{
  *aURI = nullptr;
  return false;
}

nsINode::nsSlots*
nsGenericDOMDataNode::CreateSlots()
{
  return new nsDataSlots();
}

nsGenericDOMDataNode::nsDataSlots::nsDataSlots()
  : nsINode::nsSlots(), mBindingParent(nullptr)
{
}

void
nsGenericDOMDataNode::nsDataSlots::Traverse(nsCycleCollectionTraversalCallback &cb)
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mXBLInsertionParent");
  cb.NoteXPCOMChild(mXBLInsertionParent.get());

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mSlots->mContainingShadow");
  cb.NoteXPCOMChild(NS_ISUPPORTS_CAST(nsIContent*, mContainingShadow));
}

void
nsGenericDOMDataNode::nsDataSlots::Unlink()
{
  mXBLInsertionParent = nullptr;
  mContainingShadow = nullptr;
}

//----------------------------------------------------------------------

// Implementation of the nsIDOMText interface

nsresult
nsGenericDOMDataNode::SplitData(uint32_t aOffset, nsIContent** aReturn,
                                bool aCloneAfterOriginal)
{
  *aReturn = nullptr;
  nsresult rv = NS_OK;
  nsAutoString cutText;
  uint32_t length = TextLength();

  if (aOffset > length) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  uint32_t cutStartOffset = aCloneAfterOriginal ? aOffset : 0;
  uint32_t cutLength = aCloneAfterOriginal ? length - aOffset : aOffset;
  rv = SubstringData(cutStartOffset, cutLength, cutText);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsIDocument* document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, true);

  // Use Clone for creating the new node so that the new node is of same class
  // as this node!
  nsCOMPtr<nsIContent> newContent = CloneDataNode(mNodeInfo, false);
  if (!newContent) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  newContent->SetText(cutText, true); // XXX should be false?

  CharacterDataChangeInfo::Details details = {
    CharacterDataChangeInfo::Details::eSplit, newContent
  };
  rv = SetTextInternal(cutStartOffset, cutLength, nullptr, 0, true,
                       aCloneAfterOriginal ? &details : nullptr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsINode> parent = GetParentNode();
  if (parent) {
    int32_t insertionIndex = parent->IndexOf(this);
    if (aCloneAfterOriginal) {
      ++insertionIndex;
    }
    parent->InsertChildAt(newContent, insertionIndex, true);
  }

  newContent.swap(*aReturn);
  return rv;
}

nsresult
nsGenericDOMDataNode::SplitText(uint32_t aOffset, nsIDOMText** aReturn)
{
  nsCOMPtr<nsIContent> newChild;
  nsresult rv = SplitData(aOffset, getter_AddRefs(newChild));
  if (NS_SUCCEEDED(rv)) {
    rv = CallQueryInterface(newChild, aReturn);
  }
  return rv;
}

/* static */ int32_t
nsGenericDOMDataNode::FirstLogicallyAdjacentTextNode(nsIContent* aParent,
                                                     int32_t aIndex)
{
  while (aIndex-- > 0) {
    nsIContent* sibling = aParent->GetChildAt(aIndex);
    if (!sibling->IsNodeOfType(nsINode::eTEXT))
      return aIndex + 1;
  }
  return 0;
}

/* static */ int32_t
nsGenericDOMDataNode::LastLogicallyAdjacentTextNode(nsIContent* aParent,
                                                    int32_t aIndex,
                                                    uint32_t aCount)
{
  while (++aIndex < int32_t(aCount)) {
    nsIContent* sibling = aParent->GetChildAt(aIndex);
    if (!sibling->IsNodeOfType(nsINode::eTEXT))
      return aIndex - 1;
  }
  return aCount - 1;
}

nsresult
nsGenericDOMDataNode::GetWholeText(nsAString& aWholeText)
{
  nsIContent* parent = GetParent();

  // Handle parent-less nodes
  if (!parent)
    return GetData(aWholeText);

  int32_t index = parent->IndexOf(this);
  NS_WARN_IF_FALSE(index >= 0,
                   "Trying to use .wholeText with an anonymous"
                    "text node child of a binding parent?");
  NS_ENSURE_TRUE(index >= 0, NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  int32_t first =
    FirstLogicallyAdjacentTextNode(parent, index);
  int32_t last =
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

//----------------------------------------------------------------------

// Implementation of the nsIContent interface text functions

const nsTextFragment *
nsGenericDOMDataNode::GetText()
{
  return &mText;
}

uint32_t
nsGenericDOMDataNode::TextLength() const
{
  return mText.GetLength();
}

nsresult
nsGenericDOMDataNode::SetText(const char16_t* aBuffer,
                              uint32_t aLength,
                              bool aNotify)
{
  return SetTextInternal(0, mText.GetLength(), aBuffer, aLength, aNotify);
}

nsresult
nsGenericDOMDataNode::AppendText(const char16_t* aBuffer,
                                 uint32_t aLength,
                                 bool aNotify)
{
  return SetTextInternal(mText.GetLength(), 0, aBuffer, aLength, aNotify);
}

bool
nsGenericDOMDataNode::TextIsOnlyWhitespace()
{
  // FIXME: should this method take content language into account?
  if (mText.Is2b()) {
    // The fragment contains non-8bit characters and such characters
    // are never considered whitespace.
    return false;
  }

  if (HasFlag(NS_CACHED_TEXT_IS_ONLY_WHITESPACE)) {
    return HasFlag(NS_TEXT_IS_ONLY_WHITESPACE);
  }

  const char* cp = mText.Get1b();
  const char* end = cp + mText.GetLength();

  while (cp < end) {
    char ch = *cp;

    if (!dom::IsSpaceCharacter(ch)) {
      UnsetFlags(NS_TEXT_IS_ONLY_WHITESPACE);
      SetFlags(NS_CACHED_TEXT_IS_ONLY_WHITESPACE);
      return false;
    }

    ++cp;
  }

  SetFlags(NS_CACHED_TEXT_IS_ONLY_WHITESPACE | NS_TEXT_IS_ONLY_WHITESPACE);
  return true;
}

void
nsGenericDOMDataNode::AppendTextTo(nsAString& aResult)
{
  mText.AppendTo(aResult);
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
  return nullptr;
}

const nsAttrValue*
nsGenericDOMDataNode::DoGetClasses() const
{
  NS_NOTREACHED("Shouldn't ever be called");
  return nullptr;
}

NS_IMETHODIMP
nsGenericDOMDataNode::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  return NS_OK;
}

NS_IMETHODIMP_(bool)
nsGenericDOMDataNode::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  return false;
}

nsChangeHint
nsGenericDOMDataNode::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                             int32_t aModType) const
{
  NS_NOTREACHED("Shouldn't be calling this!");
  return nsChangeHint(0);
}

nsIAtom*
nsGenericDOMDataNode::GetClassAttributeName() const
{
  return nullptr;
}

size_t
nsGenericDOMDataNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = nsIContent::SizeOfExcludingThis(aMallocSizeOf);
  n += mText.SizeOfExcludingThis(aMallocSizeOf);
  return n;
}

