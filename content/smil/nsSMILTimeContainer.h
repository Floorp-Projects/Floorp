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

#ifndef NS_SMILTIMECONTAINER_H_
#define NS_SMILTIMECONTAINER_H_

#include "nscore.h"
#include "nsSMILTypes.h"

//----------------------------------------------------------------------
// nsSMILTimeContainer
// 
// Common base class for a time base that can be paused, resumed, and sampled.
//
class nsSMILTimeContainer
{
public:
  nsSMILTimeContainer();
  virtual ~nsSMILTimeContainer();

  /*
   * Pause request types.
   */
  enum {
    PAUSE_BEGIN    = 1,
    PAUSE_SCRIPT   = 2,
    PAUSE_PAGEHIDE = 4,
    PAUSE_USERPREF = 8
  };

  /*
   * Cause the time container to records its begin time.
   */
  void Begin();

  /*
   * Pause this time container
   *
   * @param aType The source of the pause request. Successive calls to Pause
   * with the same aType will be ignored. The container will remain paused until
   * each call to Pause of a given aType has been matched by at least one call
   * to Resume with the same aType.
   */
  virtual void Pause(PRUint32 aType);

  /*
   * Resume this time container
   *
   * param @aType The source of the resume request. Clears the pause flag for
   * this particular type of pause request. When all pause flags have been
   * cleared the time container will be resumed.
   */
  virtual void Resume(PRUint32 aType);

  /**
   * Returns true if this time container is paused by the specified type.
   * Note that the time container may also be paused by other types; this method
   * does not test if aType is the exclusive pause source.
   *
   * @param @aType The pause source to test for.
   * @return PR_TRUE if this container is paused by aType.
   */
  PRBool IsPausedByType(PRUint32 aType) const { return mPauseState & aType; }

  /*
   * Return the time elapsed since this time container's begin time (expressed
   * in parent time) minus any accumulated offset from pausing.
   */
  nsSMILTime GetCurrentTime() const;

  /*
   * Seek the document timeline to the specified time.
   *
   * @param aSeekTo The time to seek to, expressed in this time container's time
   * base (i.e. the same units as GetCurrentTime).
   */
  void SetCurrentTime(nsSMILTime aSeekTo);

  /*
   * Return the current time for the parent time container if any.
   */
  virtual nsSMILTime GetParentTime() const;

  /*
   * Updates the current time of this time container and calls DoSample to
   * perform any sample-operations.
   */
  void Sample();

  /*
   * Return if this time container should be sampled or can be skipped.
   *
   * This is most useful as an optimisation for skipping time containers that
   * don't require a sample.
   */
  PRBool NeedsSample() const { return !mPauseState || mNeedsPauseSample; }

  /*
   * Sets the parent time container.
   *
   * The callee still retains ownership of the time container.
   */
  nsresult SetParent(nsSMILTimeContainer* aParent);

protected:
  /*
   * Per-sample operations to be performed whenever Sample() is called and
   * NeedsSample() is true. Called after updating mCurrentTime;
   */
  virtual void DoSample() { }

  /*
   * Adding and removing child containers is not implemented in the base class
   * because not all subclasses need this.
   */

  /*
   * Adds a child time container.
   */
  virtual nsresult AddChild(nsSMILTimeContainer& aChild)
  {
    return NS_ERROR_FAILURE;
  }

  /*
   * Removes a child time container.
   */
  virtual void RemoveChild(nsSMILTimeContainer& aChild) { }

  /*
   * Implementation helper to update the current time.
   */
  void UpdateCurrentTime();

  // The parent time container, if any
  nsSMILTimeContainer* mParent;

  // The current time established at the last call to Sample()
  nsSMILTime mCurrentTime;

  // The number of milliseconds for which the container has been paused
  // (excluding the current pause interval if the container is currently
  // paused).
  //
  //  Current time = parent time - mParentOffset
  //
  nsSMILTime mParentOffset;

  // The timestamp in milliseconds since the epoch (i.e. wallclock time) when
  // the document was paused
  nsSMILTime mPauseStart;

  // Whether or not a pause sample is required
  PRPackedBool mNeedsPauseSample;

  // A bitfield of the pause state for all pause requests
  PRUint32 mPauseState;
};

#endif // NS_SMILTIMECONTAINER_H_
