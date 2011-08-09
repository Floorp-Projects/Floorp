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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *   Timothy Nikkel <tnikkel@gmail.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsAutoLayoutPhase_h
#define nsAutoLayoutPhase_h

#ifdef DEBUG

#include "nsPresContext.h"
#include "nsContentUtils.h"

struct nsAutoLayoutPhase {
  nsAutoLayoutPhase(nsPresContext* aPresContext, nsLayoutPhase aPhase)
    : mPresContext(aPresContext), mPhase(aPhase), mCount(0)
  {
    Enter();
  }

  ~nsAutoLayoutPhase()
  {
    Exit();
    NS_ASSERTION(mCount == 0, "imbalanced");
  }

  void Enter()
  {
    switch (mPhase) {
      case eLayoutPhase_Paint:
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_Paint] == 0,
                     "recurring into paint");
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_Reflow] == 0,
                     "painting in the middle of reflow");
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_FrameC] == 0,
                     "painting in the middle of frame construction");
        break;
      case eLayoutPhase_Reflow:
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_Paint] == 0,
                     "reflowing in the middle of a paint");
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_Reflow] == 0,
                     "recurring into reflow");
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_FrameC] == 0,
                     "reflowing in the middle of frame construction");
        break;
      case eLayoutPhase_FrameC:
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_Paint] == 0,
                     "constructing frames in the middle of a paint");
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_Reflow] == 0,
                     "constructing frames in the middle of reflow");
        NS_ASSERTION(mPresContext->mLayoutPhaseCount[eLayoutPhase_FrameC] == 0,
                     "recurring into frame construction");
        NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
                     "constructing frames and scripts are not blocked");
        break;
      default:
        break;
    }
    ++(mPresContext->mLayoutPhaseCount[mPhase]);
    ++mCount;
  }

  void Exit()
  {
    NS_ASSERTION(mCount > 0 && mPresContext->mLayoutPhaseCount[mPhase] > 0,
                 "imbalanced");
    --(mPresContext->mLayoutPhaseCount[mPhase]);
    --mCount;
  }

private:
  nsPresContext* mPresContext;
  nsLayoutPhase mPhase;
  PRUint32 mCount;
};

#define AUTO_LAYOUT_PHASE_ENTRY_POINT(pc_, phase_) \
  nsAutoLayoutPhase autoLayoutPhase((pc_), (eLayoutPhase_##phase_))
#define LAYOUT_PHASE_TEMP_EXIT() \
  PR_BEGIN_MACRO \
    autoLayoutPhase.Exit(); \
  PR_END_MACRO
#define LAYOUT_PHASE_TEMP_REENTER() \
  PR_BEGIN_MACRO \
    autoLayoutPhase.Enter(); \
  PR_END_MACRO

#else

#define AUTO_LAYOUT_PHASE_ENTRY_POINT(pc_, phase_) \
  PR_BEGIN_MACRO PR_END_MACRO
#define LAYOUT_PHASE_TEMP_EXIT() \
  PR_BEGIN_MACRO PR_END_MACRO
#define LAYOUT_PHASE_TEMP_REENTER() \
  PR_BEGIN_MACRO PR_END_MACRO

#endif

#endif // nsAutoLayoutPhase_h
