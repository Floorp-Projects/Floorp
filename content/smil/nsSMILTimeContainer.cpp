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
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
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

#include "nsSMILTimeContainer.h"
#include "nsSMILTimeValue.h"
#include "nsSMILTimedElement.h"

nsSMILTimeContainer::nsSMILTimeContainer()
:
  mParent(nsnull),
  mCurrentTime(0L),
  mParentOffset(0L),
  mPauseStart(0L),
  mNeedsPauseSample(PR_FALSE),
  mNeedsRewind(PR_FALSE),
  mIsSeeking(PR_FALSE),
  mPauseState(PAUSE_BEGIN)
{
}

nsSMILTimeContainer::~nsSMILTimeContainer()
{
  if (mParent) {
    mParent->RemoveChild(*this);
  }
}

nsSMILTimeValue
nsSMILTimeContainer::ContainerToParentTime(nsSMILTime aContainerTime) const
{
  // If we're paused, then future times are indefinite
  if (IsPaused() && aContainerTime > mCurrentTime)
    return nsSMILTimeValue::Indefinite();

  return nsSMILTimeValue(aContainerTime + mParentOffset);
}

nsSMILTimeValue
nsSMILTimeContainer::ParentToContainerTime(nsSMILTime aParentTime) const
{
  // If we're paused, then any time after when we paused is indefinite
  if (IsPaused() && aParentTime > mPauseStart)
    return nsSMILTimeValue::Indefinite();

  return nsSMILTimeValue(aParentTime - mParentOffset);
}

void
nsSMILTimeContainer::Begin()
{
  Resume(PAUSE_BEGIN);
  if (mPauseState) {
    mNeedsPauseSample = PR_TRUE;
  }

  // This is a little bit complicated here. Ideally we'd just like to call
  // Sample() and force an initial sample but this turns out to be a bad idea
  // because this may mean that NeedsSample() no longer reports true and so when
  // we come to the first real sample our parent will skip us over altogether.
  // So we force the time to be updated and adopt the policy to never call
  // Sample() ourselves but to always leave that to our parent or client.

  UpdateCurrentTime();
}

void
nsSMILTimeContainer::Pause(PRUint32 aType)
{
  bool didStartPause = false;

  if (!mPauseState && aType) {
    mPauseStart = GetParentTime();
    mNeedsPauseSample = PR_TRUE;
    didStartPause = PR_TRUE;
  }

  mPauseState |= aType;

  if (didStartPause) {
    NotifyTimeChange();
  }
}

void
nsSMILTimeContainer::Resume(PRUint32 aType)
{
  if (!mPauseState)
    return;

  mPauseState &= ~aType;

  if (!mPauseState) {
    nsSMILTime extraOffset = GetParentTime() - mPauseStart;
    mParentOffset += extraOffset;
    NotifyTimeChange();
  }
}

nsSMILTime
nsSMILTimeContainer::GetCurrentTime() const
{
  // The following behaviour is consistent with:
  // http://www.w3.org/2003/01/REC-SVG11-20030114-errata
  //  #getCurrentTime_setCurrentTime_undefined_before_document_timeline_begin
  // which says that if GetCurrentTime is called before the document timeline
  // has begun we should just return 0.
  if (IsPausedByType(PAUSE_BEGIN))
    return 0L;

  return mCurrentTime;
}

void
nsSMILTimeContainer::SetCurrentTime(nsSMILTime aSeekTo)
{
  // SVG 1.1 doesn't specify what to do for negative times so we adopt SVGT1.2's
  // behaviour of clamping negative times to 0.
  aSeekTo = NS_MAX<nsSMILTime>(0, aSeekTo);

  // The following behaviour is consistent with:
  // http://www.w3.org/2003/01/REC-SVG11-20030114-errata
  //  #getCurrentTime_setCurrentTime_undefined_before_document_timeline_begin
  // which says that if SetCurrentTime is called before the document timeline
  // has begun we should still adjust the offset.
  nsSMILTime parentTime = GetParentTime();
  mParentOffset = parentTime - aSeekTo;
  mIsSeeking = PR_TRUE;

  if (IsPaused()) {
    mNeedsPauseSample = PR_TRUE;
    mPauseStart = parentTime;
  }

  if (aSeekTo < mCurrentTime) {
    // Backwards seek
    mNeedsRewind = PR_TRUE;
    ClearMilestones();
  }

  // Force an update to the current time in case we get a call to GetCurrentTime
  // before another call to Sample().
  UpdateCurrentTime();

  NotifyTimeChange();
}

nsSMILTime
nsSMILTimeContainer::GetParentTime() const
{
  if (mParent)
    return mParent->GetCurrentTime();

  return 0L;
}

void
nsSMILTimeContainer::SyncPauseTime()
{
  if (IsPaused()) {
    nsSMILTime parentTime = GetParentTime();
    nsSMILTime extraOffset = parentTime - mPauseStart;
    mParentOffset += extraOffset;
    mPauseStart = parentTime;
  }
}

void
nsSMILTimeContainer::Sample()
{
  if (!NeedsSample())
    return;

  UpdateCurrentTime();
  DoSample();

  mNeedsPauseSample = PR_FALSE;
}

nsresult
nsSMILTimeContainer::SetParent(nsSMILTimeContainer* aParent)
{
  if (mParent) {
    mParent->RemoveChild(*this);
    // When we're not attached to a parent time container, GetParentTime() will
    // return 0. We need to adjust our pause state information to be relative to
    // this new time base.
    // Note that since "current time = parent time - parent offset" setting the
    // parent offset and pause start as follows preserves our current time even
    // while parent time = 0.
    mParentOffset = -mCurrentTime;
    mPauseStart = 0L;
  }

  mParent = aParent;

  nsresult rv = NS_OK;
  if (mParent) {
    rv = mParent->AddChild(*this);
  }

  return rv;
}

bool
nsSMILTimeContainer::AddMilestone(const nsSMILMilestone& aMilestone,
                                  nsISMILAnimationElement& aElement)
{
  // We record the milestone time and store it along with the element but this
  // time may change (e.g. if attributes are changed on the timed element in
  // between samples). If this happens, then we may do an unecessary sample
  // but that's pretty cheap.
  return mMilestoneEntries.Push(MilestoneEntry(aMilestone, aElement));
}

void
nsSMILTimeContainer::ClearMilestones()
{
  mMilestoneEntries.Clear();
}

bool
nsSMILTimeContainer::GetNextMilestoneInParentTime(
    nsSMILMilestone& aNextMilestone) const
{
  if (mMilestoneEntries.IsEmpty())
    return PR_FALSE;

  nsSMILTimeValue parentTime =
    ContainerToParentTime(mMilestoneEntries.Top().mMilestone.mTime);
  if (!parentTime.IsDefinite())
    return PR_FALSE;

  aNextMilestone = nsSMILMilestone(parentTime.GetMillis(),
                                   mMilestoneEntries.Top().mMilestone.mIsEnd);

  return PR_TRUE;
}

bool
nsSMILTimeContainer::PopMilestoneElementsAtMilestone(
      const nsSMILMilestone& aMilestone,
      AnimElemArray& aMatchedElements)
{
  if (mMilestoneEntries.IsEmpty())
    return PR_FALSE;

  nsSMILTimeValue containerTime = ParentToContainerTime(aMilestone.mTime);
  if (!containerTime.IsDefinite())
    return PR_FALSE;

  nsSMILMilestone containerMilestone(containerTime.GetMillis(),
                                     aMilestone.mIsEnd);

  NS_ABORT_IF_FALSE(mMilestoneEntries.Top().mMilestone >= containerMilestone,
      "Trying to pop off earliest times but we have earlier ones that were "
      "overlooked");

  bool gotOne = false;
  while (!mMilestoneEntries.IsEmpty() &&
      mMilestoneEntries.Top().mMilestone == containerMilestone)
  {
    aMatchedElements.AppendElement(mMilestoneEntries.Pop().mTimebase);
    gotOne = PR_TRUE;
  }

  return gotOne;
}

void
nsSMILTimeContainer::Traverse(nsCycleCollectionTraversalCallback* aCallback)
{
  const MilestoneEntry* p = mMilestoneEntries.Elements();
  while (p < mMilestoneEntries.Elements() + mMilestoneEntries.Length()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCallback, "mTimebase");
    aCallback->NoteXPCOMChild(p->mTimebase.get());
    ++p;
  }
}

void
nsSMILTimeContainer::Unlink()
{
  mMilestoneEntries.Clear();
}

void
nsSMILTimeContainer::UpdateCurrentTime()
{
  nsSMILTime now = IsPaused() ? mPauseStart : GetParentTime();
  mCurrentTime = now - mParentOffset;
  NS_ABORT_IF_FALSE(mCurrentTime >= 0, "Container has negative time");
}

void
nsSMILTimeContainer::NotifyTimeChange()
{
  // Called when the container time is changed with respect to the document
  // time. When this happens time dependencies in other time containers need to
  // re-resolve their times because begin and end times are stored in container
  // time.
  //
  // To get the list of timed elements with dependencies we simply re-use the
  // milestone elements. This is because any timed element with dependents and
  // with significant transitions yet to fire should have their next milestone
  // registered. Other timed elements don't matter.
  const MilestoneEntry* p = mMilestoneEntries.Elements();
#if DEBUG
  PRUint32 queueLength = mMilestoneEntries.Length();
#endif
  while (p < mMilestoneEntries.Elements() + mMilestoneEntries.Length()) {
    nsISMILAnimationElement* elem = p->mTimebase.get();
    elem->TimedElement().HandleContainerTimeChange();
    NS_ABORT_IF_FALSE(queueLength == mMilestoneEntries.Length(),
        "Call to HandleContainerTimeChange resulted in a change to the "
        "queue of milestones");
    ++p;
  }
}
