/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for DOM Core's Comment, DocumentType, Text,
 * CDATASection and ProcessingInstruction nodes.
 */

#include "mozilla/dom/CharacterData.h"

#include "mozilla/DebugOnly.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsIDocument.h"
#include "nsReadableUtils.h"
#include "mozilla/InternalMutationEvent.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsDOMString.h"
#include "nsChangeHint.h"
#include "nsCOMArray.h"
#include "nsNodeUtils.h"
#include "mozilla/dom/DirectionalityUtils.h"
#include "nsBindingManager.h"
#include "nsCCUncollectableMarker.h"
#include "mozAutoDocUpdate.h"
#include "nsTextNode.h"
#include "nsBidiUtils.h"
#include "PLDHashTable.h"
#include "mozilla/Sprintf.h"
#include "nsWindowSizes.h"
#include "nsWrapperCacheInlines.h"

namespace mozilla {
namespace dom {

CharacterData::CharacterData(already_AddRefed<dom::NodeInfo>& aNodeInfo)
  : nsIContent(aNodeInfo)
{
  MOZ_ASSERT(mNodeInfo->NodeType() == TEXT_NODE ||
             mNodeInfo->NodeType() == CDATA_SECTION_NODE ||
             mNodeInfo->NodeType() == COMMENT_NODE ||
             mNodeInfo->NodeType() == PROCESSING_INSTRUCTION_NODE ||
             mNodeInfo->NodeType() == DOCUMENT_TYPE_NODE,
             "Bad NodeType in aNodeInfo");
}

CharacterData::CharacterData(already_AddRefed<dom::NodeInfo>&& aNodeInfo)
  : nsIContent(aNodeInfo)
{
  MOZ_ASSERT(mNodeInfo->NodeType() == TEXT_NODE ||
             mNodeInfo->NodeType() == CDATA_SECTION_NODE ||
             mNodeInfo->NodeType() == COMMENT_NODE ||
             mNodeInfo->NodeType() == PROCESSING_INSTRUCTION_NODE ||
             mNodeInfo->NodeType() == DOCUMENT_TYPE_NODE,
             "Bad NodeType in aNodeInfo");
}

CharacterData::~CharacterData()
{
  MOZ_ASSERT(!IsInUncomposedDoc(),
             "Please remove this from the document properly");
  if (GetParent()) {
    NS_RELEASE(mParent);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(CharacterData)

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(CharacterData)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(CharacterData)
  return Element::CanSkip(tmp, aRemovingAllowed);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(CharacterData)
  return Element::CanSkipInCC(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(CharacterData)
  return Element::CanSkipThis(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

// We purposefully don't TRAVERSE_BEGIN_INHERITED here.  All the bits
// we should traverse should be added here or in nsINode::Traverse.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(CharacterData)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[40];
    SprintfLiteral(name, "CharacterData (len=%d)",
                   tmp->mText.GetLength());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(CharacterData, tmp->mRefCnt.get())
  }

  if (!nsIContent::Traverse(tmp, cb)) {
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// We purposefully don't UNLINK_BEGIN_INHERITED here.
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CharacterData)
  nsIContent::Unlink(tmp);

  // Clear flag here because unlinking slots will clear the
  // containing shadow root pointer.
  tmp->UnsetFlags(NODE_IS_IN_SHADOW_TREE);

  nsContentSlots* slots = tmp->GetExistingContentSlots();
  if (slots) {
    slots->Unlink();
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN(CharacterData)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(CharacterData)
NS_INTERFACE_MAP_END_INHERITING(nsIContent)

void
CharacterData::GetNodeValueInternal(nsAString& aNodeValue)
{
  GetData(aNodeValue);
}

void
CharacterData::SetNodeValueInternal(const nsAString& aNodeValue,
                                           ErrorResult& aError)
{
  aError = SetTextInternal(0, mText.GetLength(), aNodeValue.BeginReading(),
                           aNodeValue.Length(), true);
}

//----------------------------------------------------------------------

// Implementation of CharacterData

void
CharacterData::GetData(nsAString& aData) const
{
  if (mText.Is2b()) {
    aData.Truncate();
    mText.AppendTo(aData);
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
}

void
CharacterData::SetData(const nsAString& aData, ErrorResult& aRv)
{
  nsresult rv = SetTextInternal(0, mText.GetLength(), aData.BeginReading(),
                                aData.Length(), true);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
CharacterData::SubstringData(uint32_t aStart, uint32_t aCount,
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

//----------------------------------------------------------------------

void
CharacterData::AppendData(const nsAString& aData, ErrorResult& aRv)
{
  InsertData(mText.GetLength(), aData, aRv);
}

void
CharacterData::InsertData(uint32_t aOffset,
                          const nsAString& aData,
                          ErrorResult& aRv)
{
  nsresult rv = SetTextInternal(aOffset, 0, aData.BeginReading(),
                                aData.Length(), true);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
CharacterData::DeleteData(uint32_t aOffset, uint32_t aCount, ErrorResult& aRv)
{
  nsresult rv = SetTextInternal(aOffset, aCount, nullptr, 0, true);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

void
CharacterData::ReplaceData(uint32_t aOffset, uint32_t aCount,
                           const nsAString& aData, ErrorResult& aRv)
{
  nsresult rv = SetTextInternal(aOffset, aCount, aData.BeginReading(),
                                aData.Length(), true);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }  
}

nsresult
CharacterData::SetTextInternal(uint32_t aOffset, uint32_t aCount,
                               const char16_t* aBuffer,
                               uint32_t aLength, bool aNotify,
                               CharacterDataChangeInfo::Details* aDetails)
{
  MOZ_ASSERT(aBuffer || !aLength,
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

  nsIDocument *document = GetComposedDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

  bool haveMutationListeners = aNotify &&
    nsContentUtils::HasMutationListeners(this,
      NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED,
      this);

  RefPtr<nsAtom> oldValue;
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
    nsNodeUtils::CharacterDataWillChange(this, info);
  }

  Directionality oldDir = eDir_NotSet;
  bool dirAffectsAncestor = (NodeType() == TEXT_NODE &&
                             TextNodeWillChangeDirection(this, &oldDir, aOffset));

  if (aOffset == 0 && endOffset == textLength) {
    // Replacing whole text or old text was empty.  Don't bother to check for
    // bidi in this string if the document already has bidi enabled.
    // If this is marked as "maybe modified frequently", the text should be
    // stored as char16_t since converting char* to char16_t* is expensive.
    bool ok =
      mText.SetTo(aBuffer, aLength, !document || !document->GetBidiEnabled(),
                  HasFlag(NS_MAYBE_MODIFIED_FREQUENTLY));
    NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  }
  else if (aOffset == textLength) {
    // Appending to existing
    bool ok =
      mText.Append(aBuffer, aLength, !document || !document->GetBidiEnabled(),
                   HasFlag(NS_MAYBE_MODIFIED_FREQUENTLY));
    NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  }
  else {
    // Merging old and new

    bool bidi = mText.IsBidi();

    // Allocate new buffer
    int32_t newLength = textLength - aCount + aLength;
    // Use nsString and not nsAutoString so that we get a nsStringBuffer which
    // can be just AddRefed in nsTextFragment.
    nsString to;
    to.SetCapacity(newLength);

    // Copy over appropriate data
    if (aOffset) {
      mText.AppendTo(to, 0, aOffset);
    }
    if (aLength) {
      to.Append(aBuffer, aLength);
      if (!bidi && (!document || !document->GetBidiEnabled())) {
        bidi = HasRTLChars(MakeSpan(aBuffer, aLength));
      }
    }
    if (endOffset != textLength) {
      mText.AppendTo(to, endOffset, textLength - endOffset);
    }

    // If this is marked as "maybe modified frequently", the text should be
    // stored as char16_t since converting char* to char16_t* is expensive.
    // Use char16_t also when we have bidi characters.
    bool use2b = HasFlag(NS_MAYBE_MODIFIED_FREQUENTLY) || bidi;
    bool ok = mText.SetTo(to, false, use2b);
    mText.SetBidi(bidi);

    NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  }

  UnsetFlags(NS_CACHED_TEXT_IS_ONLY_WHITESPACE);

  if (document && mText.IsBidi()) {
    // If we found bidi characters in mText.SetTo() above, indicate that the
    // document contains bidi characters.
    document->SetBidiEnabled();
  }

  if (dirAffectsAncestor) {
    // dirAffectsAncestor being true implies that we have a text node, see
    // above.
    MOZ_ASSERT(NodeType() == TEXT_NODE);
    TextNodeChangedDirection(static_cast<nsTextNode*>(this), oldDir, aNotify);
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
    nsNodeUtils::CharacterDataChanged(this, info);

    if (haveMutationListeners) {
      InternalMutationEvent mutation(true, eLegacyCharacterDataModified);

      mutation.mPrevAttrValue = oldValue;
      if (aLength > 0) {
        nsAutoString val;
        mText.AppendTo(val);
        mutation.mNewAttrValue = NS_Atomize(val);
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
CharacterData::ToCString(nsAString& aBuf, int32_t aOffset,
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
        aBuf.AppendPrintf("\\u%04x", ch);
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
        aBuf.AppendPrintf("\\u%04x", ch);
      } else {
        aBuf.Append(ch);
      }
    }
  }
}
#endif


nsresult
CharacterData::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                          nsIContent* aBindingParent,
                          bool aCompileEventHandlers)
{
  MOZ_ASSERT(aParent || aDocument, "Must have document if no parent!");
  MOZ_ASSERT(NODE_FROM(aParent, aDocument)->OwnerDoc() == OwnerDoc(),
             "Must have the same owner document");
  MOZ_ASSERT(!aParent || aDocument == aParent->GetUncomposedDoc(),
             "aDocument must be current doc of aParent");
  MOZ_ASSERT(!GetUncomposedDoc() && !IsInUncomposedDoc(),
             "Already have a document.  Unbind first!");
  // Note that as we recurse into the kids, they'll have a non-null parent.  So
  // only assert if our parent is _changing_ while we have a parent.
  MOZ_ASSERT(!GetParent() || aParent == GetParent(),
             "Already have a parent.  Unbind first!");
  MOZ_ASSERT(!GetBindingParent() ||
             aBindingParent == GetBindingParent() ||
             (!aBindingParent && aParent &&
              aParent->GetBindingParent() == GetBindingParent()),
             "Already have a binding parent.  Unbind first!");
  MOZ_ASSERT(aBindingParent != this,
             "Content must not be its own binding parent");
  MOZ_ASSERT(!IsRootOfNativeAnonymousSubtree() ||
             aBindingParent == aParent,
             "Native anonymous content must have its parent as its "
             "own binding parent");

  if (!aBindingParent && aParent) {
    aBindingParent = aParent->GetBindingParent();
  }

  // First set the binding parent
  if (aBindingParent) {
    NS_ASSERTION(IsRootOfNativeAnonymousSubtree() ||
                 !HasFlag(NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE) ||
                 (aParent && aParent->IsInNativeAnonymousSubtree()),
                 "Trying to re-bind content from native anonymous subtree to "
                 "non-native anonymous parent!");
    ExtendedContentSlots()->mBindingParent = aBindingParent; // Weak, so no addref happens.
    if (aParent->IsInNativeAnonymousSubtree()) {
      SetFlags(NODE_IS_IN_NATIVE_ANONYMOUS_SUBTREE);
    }
    if (aParent->HasFlag(NODE_CHROME_ONLY_ACCESS)) {
      SetFlags(NODE_CHROME_ONLY_ACCESS);
    }
    if (HasFlag(NODE_IS_ANONYMOUS_ROOT)) {
      aParent->SetMayHaveAnonymousChildren();
    }
    if (aParent->IsInShadowTree()) {
      ClearSubtreeRootPointer();
      SetFlags(NODE_IS_IN_SHADOW_TREE);
    }
    ShadowRoot* parentContainingShadow = aParent->GetContainingShadow();
    if (parentContainingShadow) {
      ExtendedContentSlots()->mContainingShadow = parentContainingShadow;
    }
  }

  bool hadParent = !!GetParentNode();

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
    SetIsInDocument();
    if (mText.IsBidi()) {
      aDocument->SetBidiEnabled();
    }
    // Clear the lazy frame construction bits.
    UnsetFlags(NODE_NEEDS_FRAME | NODE_DESCENDANTS_NEED_FRAMES);
  } else if (!IsInShadowTree()) {
    // If we're not in the doc and not in a shadow tree,
    // update our subtree pointer.
    SetSubtreeRootPointer(aParent->SubtreeRoot());
  }

  nsNodeUtils::ParentChainChanged(this);
  if (!hadParent && IsRootOfNativeAnonymousSubtree()) {
    nsNodeUtils::NativeAnonymousChildListChange(this, false);
  }

  UpdateEditableState(false);

  MOZ_ASSERT(aDocument == GetUncomposedDoc(), "Bound to wrong document");
  MOZ_ASSERT(aParent == GetParent(), "Bound to wrong parent");
  MOZ_ASSERT(aBindingParent == GetBindingParent(),
             "Bound to wrong binding parent");

  return NS_OK;
}

void
CharacterData::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // Unset frame flags; if we need them again later, they'll get set again.
  UnsetFlags(NS_CREATE_FRAME_IF_NON_WHITESPACE |
             NS_REFRAME_IF_WHITESPACE);

  nsIDocument* document = GetComposedDoc();

  if (aNullParent) {
    if (this->IsRootOfNativeAnonymousSubtree()) {
      nsNodeUtils::NativeAnonymousChildListChange(this, true);
    }
    if (GetParent()) {
      NS_RELEASE(mParent);
    } else {
      mParent = nullptr;
    }
    SetParentIsContent(false);
  }
  ClearInDocument();

  if (aNullParent || !mParent->IsInShadowTree()) {
    UnsetFlags(NODE_IS_IN_SHADOW_TREE);

    // Begin keeping track of our subtree root.
    SetSubtreeRootPointer(aNullParent ? this : mParent->SubtreeRoot());
  }

  if (document && !GetContainingShadow()) {
    // Notify XBL- & nsIAnonymousContentCreator-generated
    // anonymous content that the document is changing.
    // Unlike XBL, bindings for web components shadow DOM
    // do not get uninstalled.
    if (HasFlag(NODE_MAY_BE_IN_BINDING_MNGR)) {
      nsContentUtils::AddScriptRunner(
        new RemoveFromBindingManagerRunnable(document->BindingManager(), this,
                                             document));
    }
  }

  nsExtendedContentSlots* slots = GetExistingExtendedContentSlots();
  if (slots) {
    slots->mBindingParent = nullptr;
    if (aNullParent || !mParent->IsInShadowTree()) {
      slots->mContainingShadow = nullptr;
    }
  }

  nsNodeUtils::ParentChainChanged(this);
}

already_AddRefed<nsINodeList>
CharacterData::GetChildren(uint32_t aFilter)
{
  return nullptr;
}

uint32_t
CharacterData::GetChildCount() const
{
  return 0;
}

nsIContent *
CharacterData::GetChildAt_Deprecated(uint32_t aIndex) const
{
  return nullptr;
}


int32_t
CharacterData::ComputeIndexOf(const nsINode* aPossibleChild) const
{
  return -1;
}

nsresult
CharacterData::InsertChildBefore(nsIContent* aKid,
                                 nsIContent* aBeforeThis,
                                 bool aNotify)
{
  return NS_OK;
}

nsresult
CharacterData::InsertChildAt_Deprecated(nsIContent* aKid,
                                        uint32_t aIndex,
                                        bool aNotify)
{
  return NS_OK;
}

void
CharacterData::RemoveChildAt_Deprecated(uint32_t aIndex, bool aNotify)
{
}

void
CharacterData::RemoveChildNode(nsIContent* aKid, bool aNotify)
{
}

nsXBLBinding *
CharacterData::DoGetXBLBinding() const
{
  return nullptr;
}

bool
CharacterData::IsNodeOfType(uint32_t aFlags) const
{
  return false;
}

void
CharacterData::SaveSubtreeState()
{
}

#ifdef DEBUG
void
CharacterData::List(FILE* out, int32_t aIndent) const
{
}

void
CharacterData::DumpContent(FILE* out, int32_t aIndent,
                           bool aDumpAll) const
{
}
#endif

bool
CharacterData::IsLink(nsIURI** aURI) const
{
  *aURI = nullptr;
  return false;
}

//----------------------------------------------------------------------

// Implementation of the nsIContent interface text functions

const nsTextFragment *
CharacterData::GetText()
{
  return &mText;
}

uint32_t
CharacterData::TextLength() const
{
  return TextDataLength();
}

nsresult
CharacterData::SetText(const char16_t* aBuffer,
                       uint32_t aLength,
                       bool aNotify)
{
  return SetTextInternal(0, mText.GetLength(), aBuffer, aLength, aNotify);
}

nsresult
CharacterData::AppendText(const char16_t* aBuffer,
                          uint32_t aLength,
                          bool aNotify)
{
  return SetTextInternal(mText.GetLength(), 0, aBuffer, aLength, aNotify);
}

bool
CharacterData::TextIsOnlyWhitespace()
{

  MOZ_ASSERT(NS_IsMainThread());
  if (!ThreadSafeTextIsOnlyWhitespace()) {
    UnsetFlags(NS_TEXT_IS_ONLY_WHITESPACE);
    SetFlags(NS_CACHED_TEXT_IS_ONLY_WHITESPACE);
    return false;
  }

  SetFlags(NS_CACHED_TEXT_IS_ONLY_WHITESPACE | NS_TEXT_IS_ONLY_WHITESPACE);
  return true;
}

bool
CharacterData::ThreadSafeTextIsOnlyWhitespace() const
{
  // FIXME: should this method take content language into account?
  if (mText.Is2b()) {
    // The fragment contains non-8bit characters and such characters
    // are never considered whitespace.
    //
    // FIXME(emilio): This is not quite true in presence of the
    // NS_MAYBE_MODIFIED_FREQUENTLY flag... But looks like we only set that on
    // anonymous nodes, so should be fine...
    return false;
  }

  if (HasFlag(NS_CACHED_TEXT_IS_ONLY_WHITESPACE)) {
    return HasFlag(NS_TEXT_IS_ONLY_WHITESPACE);
  }

  const char* cp = mText.Get1b();
  const char* end = cp + mText.GetLength();

  while (cp < end) {
    char ch = *cp;

    // NOTE(emilio): If you ever change the definition of "whitespace" here, you
    // need to change it too in RestyleManager::CharacterDataChanged.
    if (!dom::IsSpaceCharacter(ch)) {
      return false;
    }

    ++cp;
  }

  return true;
}

void
CharacterData::AppendTextTo(nsAString& aResult)
{
  mText.AppendTo(aResult);
}

bool
CharacterData::AppendTextTo(nsAString& aResult,
                            const mozilla::fallible_t& aFallible)
{
  return mText.AppendTo(aResult, aFallible);
}

already_AddRefed<nsAtom>
CharacterData::GetCurrentValueAtom()
{
  nsAutoString val;
  GetData(val);
  return NS_Atomize(val);
}

void
CharacterData::AddSizeOfExcludingThis(nsWindowSizes& aSizes,
                                      size_t* aNodeSize) const
{
  nsIContent::AddSizeOfExcludingThis(aSizes, aNodeSize);
  *aNodeSize += mText.SizeOfExcludingThis(aSizes.mState.mMallocSizeOf);
}

} // namespace dom
} // namespace mozilla
