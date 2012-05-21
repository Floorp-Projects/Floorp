/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccEvent.h"

#include "ApplicationAccessibleWrap.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsDocAccessible.h"
#include "nsIAccessibleText.h"
#include "nsAccEvent.h"
#include "States.h"

#include "nsIDOMDocument.h"
#include "nsEventStateManager.h"
#include "nsIServiceManager.h"
#ifdef MOZ_XUL
#include "nsIDOMXULMultSelectCntrlEl.h"
#endif

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// AccEvent
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// AccEvent constructors

AccEvent::AccEvent(PRUint32 aEventType, nsAccessible* aAccessible,
                   EIsFromUserInput aIsFromUserInput, EEventRule aEventRule) :
  mEventType(aEventType), mEventRule(aEventRule), mAccessible(aAccessible)
{
  CaptureIsFromUserInput(aIsFromUserInput);
}

AccEvent::AccEvent(PRUint32 aEventType, nsINode* aNode,
                   EIsFromUserInput aIsFromUserInput, EEventRule aEventRule) :
  mEventType(aEventType), mEventRule(aEventRule), mNode(aNode)
{
  CaptureIsFromUserInput(aIsFromUserInput);
}

////////////////////////////////////////////////////////////////////////////////
// AccEvent public methods

nsAccessible *
AccEvent::GetAccessible()
{
  if (!mAccessible)
    mAccessible = GetAccessibleForNode();

  return mAccessible;
}

nsINode*
AccEvent::GetNode()
{
  if (!mNode && mAccessible)
    mNode = mAccessible->GetNode();

  return mNode;
}

nsDocAccessible*
AccEvent::GetDocAccessible()
{
  if (mAccessible)
    return mAccessible->Document();

  nsINode* node = GetNode();
  if (node)
    return GetAccService()->GetDocAccessible(node->OwnerDoc());

  return nsnull;
}

already_AddRefed<nsAccEvent>
AccEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccEvent(this);
  NS_IF_ADDREF(event);
  return event;
}

////////////////////////////////////////////////////////////////////////////////
// AccEvent cycle collection

NS_IMPL_CYCLE_COLLECTION_CLASS(AccEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_NATIVE(AccEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mAccessible)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(AccEvent)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mAccessible");
  cb.NoteXPCOMChild(static_cast<nsIAccessible*>(tmp->mAccessible));
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AccEvent, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AccEvent, Release)

////////////////////////////////////////////////////////////////////////////////
// AccEvent protected methods

nsAccessible*
AccEvent::GetAccessibleForNode() const
{
  return mNode ? GetAccService()->GetAccessible(mNode, nsnull) : nsnull;
}

void
AccEvent::CaptureIsFromUserInput(EIsFromUserInput aIsFromUserInput)
{
  nsINode *targetNode = GetNode();

#ifdef DEBUG
  if (!targetNode) {
    // XXX: remove this hack during reorganization of 506907. Meanwhile we
    // want to get rid an assertion for application accessible events which
    // don't have DOM node (see bug 506206).
    ApplicationAccessible* applicationAcc =
      nsAccessNode::GetApplicationAccessible();

    if (mAccessible != static_cast<nsIAccessible*>(applicationAcc))
      NS_ASSERTION(targetNode, "There should always be a DOM node for an event");
  }
#endif

  if (aIsFromUserInput != eAutoDetect) {
    mIsFromUserInput = aIsFromUserInput == eFromUserInput ? true : false;
    return;
  }

  if (!targetNode)
    return;

  nsIPresShell *presShell = nsCoreUtils::GetPresShellFor(targetNode);
  if (!presShell) {
    NS_NOTREACHED("Threre should always be an pres shell for an event");
    return;
  }

  nsEventStateManager *esm = presShell->GetPresContext()->EventStateManager();
  if (!esm) {
    NS_NOTREACHED("There should always be an ESM for an event");
    return;
  }

  mIsFromUserInput = esm->IsHandlingUserInputExternal();
}


////////////////////////////////////////////////////////////////////////////////
// AccStateChangeEvent
////////////////////////////////////////////////////////////////////////////////

// Note: we pass in eAllowDupes to the base class because we don't currently
// support correct state change coalescence (XXX Bug 569356). Also we need to
// decide how to coalesce events created via accessible (instead of node).
AccStateChangeEvent::
  AccStateChangeEvent(nsAccessible* aAccessible, PRUint64 aState,
                      bool aIsEnabled, EIsFromUserInput aIsFromUserInput):
  AccEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, aAccessible,
           aIsFromUserInput, eAllowDupes),
  mState(aState), mIsEnabled(aIsEnabled)
{
}

AccStateChangeEvent::
  AccStateChangeEvent(nsINode* aNode, PRUint64 aState, bool aIsEnabled):
  AccEvent(::nsIAccessibleEvent::EVENT_STATE_CHANGE, aNode,
           eAutoDetect, eAllowDupes),
  mState(aState), mIsEnabled(aIsEnabled)
{
}

AccStateChangeEvent::
  AccStateChangeEvent(nsINode* aNode, PRUint64 aState) :
  AccEvent(::nsIAccessibleEvent::EVENT_STATE_CHANGE, aNode,
           eAutoDetect, eAllowDupes),
  mState(aState)
{
  // Use GetAccessibleForNode() because we do not want to store an accessible
  // since it leads to problems with delayed events in the case when
  // an accessible gets reorder event before delayed event is processed.
  nsAccessible *accessible = GetAccessibleForNode();
  mIsEnabled = accessible && ((accessible->State() & mState) != 0);
}

already_AddRefed<nsAccEvent>
AccStateChangeEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccStateChangeEvent(this);
  NS_IF_ADDREF(event);
  return event;
}


////////////////////////////////////////////////////////////////////////////////
// AccTextChangeEvent
////////////////////////////////////////////////////////////////////////////////

// Note: we pass in eAllowDupes to the base class because we don't support text
// events coalescence. We fire delayed text change events in nsDocAccessible but
// we continue to base the event off the accessible object rather than just the
// node. This means we won't try to create an accessible based on the node when
// we are ready to fire the event and so we will no longer assert at that point
// if the node was removed from the document. Either way, the AT won't work with
// a defunct accessible so the behaviour should be equivalent.
// XXX revisit this when coalescence is faster (eCoalesceFromSameSubtree)
AccTextChangeEvent::
  AccTextChangeEvent(nsAccessible* aAccessible, PRInt32 aStart,
                     const nsAString& aModifiedText, bool aIsInserted,
                     EIsFromUserInput aIsFromUserInput)
  : AccEvent(aIsInserted ?
             static_cast<PRUint32>(nsIAccessibleEvent::EVENT_TEXT_INSERTED) :
             static_cast<PRUint32>(nsIAccessibleEvent::EVENT_TEXT_REMOVED),
             aAccessible, aIsFromUserInput, eAllowDupes)
  , mStart(aStart)
  , mIsInserted(aIsInserted)
  , mModifiedText(aModifiedText)
{
  // XXX We should use IsFromUserInput here, but that isn't always correct
  // when the text change isn't related to content insertion or removal.
   mIsFromUserInput = mAccessible->State() &
    (states::FOCUSED | states::EDITABLE);
}

already_AddRefed<nsAccEvent>
AccTextChangeEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccTextChangeEvent(this);
  NS_IF_ADDREF(event);
  return event;
}


////////////////////////////////////////////////////////////////////////////////
// AccMutationEvent
////////////////////////////////////////////////////////////////////////////////

AccMutationEvent::
  AccMutationEvent(PRUint32 aEventType, nsAccessible* aTarget,
                   nsINode* aTargetNode) :
  AccEvent(aEventType, aTarget, eAutoDetect, eCoalesceFromSameSubtree)
{
  mNode = aTargetNode;
}


////////////////////////////////////////////////////////////////////////////////
// AccHideEvent
////////////////////////////////////////////////////////////////////////////////

AccHideEvent::
  AccHideEvent(nsAccessible* aTarget, nsINode* aTargetNode) :
  AccMutationEvent(::nsIAccessibleEvent::EVENT_HIDE, aTarget, aTargetNode)
{
  mParent = mAccessible->Parent();
  mNextSibling = mAccessible->NextSibling();
  mPrevSibling = mAccessible->PrevSibling();
}

already_AddRefed<nsAccEvent>
AccHideEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccHideEvent(this);
  NS_ADDREF(event);
  return event;
}


////////////////////////////////////////////////////////////////////////////////
// AccShowEvent
////////////////////////////////////////////////////////////////////////////////

AccShowEvent::
  AccShowEvent(nsAccessible* aTarget, nsINode* aTargetNode) :
  AccMutationEvent(::nsIAccessibleEvent::EVENT_SHOW, aTarget, aTargetNode)
{
}


////////////////////////////////////////////////////////////////////////////////
// AccCaretMoveEvent
////////////////////////////////////////////////////////////////////////////////

AccCaretMoveEvent::
  AccCaretMoveEvent(nsAccessible* aAccessible, PRInt32 aCaretOffset) :
  AccEvent(::nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED, aAccessible),
  mCaretOffset(aCaretOffset)
{
}

AccCaretMoveEvent::
  AccCaretMoveEvent(nsINode* aNode) :
  AccEvent(::nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED, aNode),
  mCaretOffset(-1)
{
}

already_AddRefed<nsAccEvent>
AccCaretMoveEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccCaretMoveEvent(this);
  NS_IF_ADDREF(event);
  return event;
}


////////////////////////////////////////////////////////////////////////////////
// AccSelChangeEvent
////////////////////////////////////////////////////////////////////////////////

AccSelChangeEvent::
  AccSelChangeEvent(nsAccessible* aWidget, nsAccessible* aItem,
                    SelChangeType aSelChangeType) :
    AccEvent(0, aItem, eAutoDetect, eCoalesceSelectionChange),
    mWidget(aWidget), mItem(aItem), mSelChangeType(aSelChangeType),
    mPreceedingCount(0), mPackedEvent(nsnull)
{
  if (aSelChangeType == eSelectionAdd) {
    if (mWidget->GetSelectedItem(1))
      mEventType = nsIAccessibleEvent::EVENT_SELECTION_ADD;
    else
      mEventType = nsIAccessibleEvent::EVENT_SELECTION;
  } else {
    mEventType = nsIAccessibleEvent::EVENT_SELECTION_REMOVE;
  }
}


////////////////////////////////////////////////////////////////////////////////
// AccTableChangeEvent
////////////////////////////////////////////////////////////////////////////////

AccTableChangeEvent::
  AccTableChangeEvent(nsAccessible* aAccessible, PRUint32 aEventType,
                      PRInt32 aRowOrColIndex, PRInt32 aNumRowsOrCols) :
  AccEvent(aEventType, aAccessible),
  mRowOrColIndex(aRowOrColIndex), mNumRowsOrCols(aNumRowsOrCols)
{
}

already_AddRefed<nsAccEvent>
AccTableChangeEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccTableChangeEvent(this);
  NS_IF_ADDREF(event);
  return event;
}


////////////////////////////////////////////////////////////////////////////////
// AccVCChangeEvent
////////////////////////////////////////////////////////////////////////////////

AccVCChangeEvent::
  AccVCChangeEvent(nsAccessible* aAccessible,
                   nsIAccessible* aOldAccessible,
                   PRInt32 aOldStart, PRInt32 aOldEnd) :
    AccEvent(::nsIAccessibleEvent::EVENT_VIRTUALCURSOR_CHANGED, aAccessible),
    mOldAccessible(aOldAccessible), mOldStart(aOldStart), mOldEnd(aOldEnd)
{
}

already_AddRefed<nsAccEvent>
AccVCChangeEvent::CreateXPCOMObject()
{
  nsAccEvent* event = new nsAccVirtualCursorChangeEvent(this);
  NS_ADDREF(event);
  return event;
}
