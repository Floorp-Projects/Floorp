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

nsSMILTimeContainer::nsSMILTimeContainer()
:
  mParent(nsnull),
  mCurrentTime(0L),
  mParentOffset(0L),
  mPauseStart(0L),
  mNeedsPauseSample(PR_FALSE),
  mPauseState(PAUSE_BEGIN)
{
}

nsSMILTimeContainer::~nsSMILTimeContainer()
{
  if (mParent) {
    mParent->RemoveChild(*this);
  }
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
  if (!mPauseState && aType) {
    mPauseStart = GetParentTime();
    mNeedsPauseSample = PR_TRUE;
  }

  mPauseState |= aType;
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
  // The following behaviour is consistent with:
  // http://www.w3.org/2003/01/REC-SVG11-20030114-errata
  //  #getCurrentTime_setCurrentTime_undefined_before_document_timeline_begin
  // which says that if SetCurrentTime is called before the document timeline
  // has begun we should still adjust the offset.
  mParentOffset = GetParentTime() - aSeekTo;

  if (mPauseState) {
    mNeedsPauseSample = PR_TRUE;
  }

  // Force an update to the current time in case we get a call to GetCurrentTime
  // before another call to Sample().
  UpdateCurrentTime();
}

nsSMILTime
nsSMILTimeContainer::GetParentTime() const
{
  if (mParent)
    return mParent->GetCurrentTime();

  return 0L;
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
  }

  mParent = aParent;

  nsresult rv = NS_OK;
  if (mParent) {
    rv = mParent->AddChild(*this);
  }

  return rv;
}

void
nsSMILTimeContainer::UpdateCurrentTime()
{
  nsSMILTime now = mPauseState ? mPauseStart : GetParentTime();
  mCurrentTime = now - mParentOffset;
}
