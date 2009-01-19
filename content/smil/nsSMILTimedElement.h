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
 * Portions created by the Initial Developer are Copyright (C) 2005
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

#ifndef NS_SMILTIMEDELEMENT_H_
#define NS_SMILTIMEDELEMENT_H_

#include "nsSMILInterval.h"
#include "nsSMILInstanceTime.h"
#include "nsSMILTimeValueSpec.h"
#include "nsSMILRepeatCount.h"
#include "nsSMILTypes.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsAttrValue.h"

class nsSMILAnimationFunction;
class nsSMILTimeContainer;
class nsSMILTimeValue;
class nsIAtom;

//----------------------------------------------------------------------
// nsSMILTimedElement

class nsSMILTimedElement
{
public:
  nsSMILTimedElement();

  /** 
   * Methods for supporting the nsIDOMElementTimeControl interface.
   */

  /*
   * Adds a new begin instance time at the current document time (as defined by
   * aContainer) plus or minus the specified offset.
   *
   * @param aOffsetSeconds A real number specifying the number of seconds to add
   *                       to the current container time.
   * @param aContainer The time container with which this timed element is
   *                   associated and which should be used for determining the
   *                   current time.
   * @return NS_OK if the operation succeeeded, or an error code otherwise.
   */
  PRBool BeginElementAt(double aOffsetSeconds,
                        const nsSMILTimeContainer* aContainer);

  /*
   * Adds a new end instance time at the current document time (as defined by
   * aContainer) plus or minus the specified offset.
   *
   * @param aOffsetSeconds A real number specifying the number of seconds to add
   *                       to the current container time.
   * @param aContainer The time container with which this timed element is
   *                   associated and which should be used for determining the
   *                   current time.
   * @return NS_OK if the operation succeeeded, or an error code otherwise.
   */
  PRBool EndElementAt(double aOffsetSeconds,
                      const nsSMILTimeContainer* aContainer);

  /** 
   * Methods for supporting the nsSVGAnimationElement interface.
   */

  /**
   * According to SVG 1.1 this is supposed to return the start time for the
   * animation but at this stage no one seems to know what that means.
   *
   * For now we have adopted Opera's behaviour which seems to be:
   *
   *   (i) If the animation is in the active state, return the start of the
   *       current interval
   *  (ii) Otherwise, if there is a previous interval, return the start of the
   *       previous interval
   * (iii) Otherwise, if there is a future resolved interval, the the start of
   *       the next interval
   *  (iv) Otherwise, return 0.
   *
   * As this method represents a SMIL interface which is called by the SVG
   * interface, instead of returning 0 in case (iv) we return 'indefinite' and
   * then allow the SVG interface to decide what to do with it. That is, we
   * don't throw away information until the last moment.
   *
   * @return the start time as defined above in milliseconds or 'indefinite' if
   * there is no resolved start time for this element (case iv).
   */
  nsSMILTimeValue GetStartTime() const;

  /**
   * Returns the simple duration of this element.
   *
   * @return the simple duration in milliseconds or INDEFINITE.
   */
  nsSMILTimeValue GetSimpleDuration() const
  {
    return mSimpleDur;
  }

  /** 
   * Internal SMIL methods
   */

  /**
   * Adds an instance time object this element's list of instance times.
   * These instance times are used when creating intervals.
   *
   * This method is typically called by an nsSMILTimeValueSpec.
   *
   * @param aInstanceTime   The time to add, expressed in document time.
   *
   * @param aIsBegin        True if the time to be added represents a begin time
   *                        or false if it represents an end time.
   */
  void AddInstanceTime(const nsSMILInstanceTime& aInstanceTime,
                       PRBool aIsBegin);

  /**
   * Sets the object that will be called by this timed element each time it is
   * sampled.
   *
   * In Schmitz's model it is possible to associate several time clients with
   * a timed element but for now we only allow one.
   *
   * @param aClient   The time client to associate. Any previous time client
   *                  will be disassociated and no longer sampled. Setting this
   *                  to nsnull will simply disassociate the previous client, if
   *                  any.
   */
  void SetTimeClient(nsSMILAnimationFunction* aClient);

  /**
   * Samples the object at the given document time. Timing intervals are updated
   * and if this element is active at the given time the associated time client
   * will be sampled with the appropriate simple time.
   *
   * @param aDocumentTime The document time at which to sample.
   */
  void SampleAt(nsSMILTime aDocumentTime);

  /**
   * Reset the element's internal state. As described in SMILANIM 3.3.7, all
   * instance times associated with DOM calls, events, etc. are cleared.
   */
  void Reset();

  /**
   * Restores the element to its initial state. As with Reset() this involves
   * clearing certain instance times, however in addition to the Reset()
   * behaviour this method also discards all previously constructed intervals,
   * removes any previously applied fill effects, and resets the elements
   * internal state. It is suitable for use, for example, when the begin or end
   * attribute has been updated.
   */
  void HardReset();

  /**
   * Attempts to set an attribute on this timed element.
   *
   * @param aAttribute  The name of the attribute to set. The namespace of this
   *                    attribute is not specified as it is checked by the host
   *                    element. Only attributes in the namespace defined for
   *                    SMIL attributes in the host language are passed to the
   *                    timed element.
   * @param aValue      The attribute value.
   * @param aResult     The nsAttrValue object that may be used for storing the
   *                    parsed result.
   * @param[out] aParseResult The result of parsing the attribute. Will be set
   *                          to NS_OK if parsing is successful.
   *
   * @return PR_TRUE if the given attribute is a timing attribute, PR_FALSE
   * otherwise.
   */
  PRBool SetAttr(nsIAtom* aAttribute, const nsAString& aValue,
                 nsAttrValue& aResult, nsresult* aParseResult = nsnull);

  /**
   * Attempts to unset an attribute on this timed element.
   *
   * @param aAttribute  The name of the attribute to set. As with SetAttr the
   *                    namespace of the attribute is not specified (see
   *                    SetAttr).
   *
   * @return PR_TRUE if the given attribute is a timing attribute, PR_FALSE
   * otherwise.
   */
  PRBool UnsetAttr(nsIAtom* aAttribute);

protected:
  //
  // Implementation helpers
  //

  nsresult          SetBeginSpec(const nsAString& aBeginSpec);
  nsresult          SetEndSpec(const nsAString& aEndSpec);
  nsresult          SetSimpleDuration(const nsAString& aDurSpec);
  nsresult          SetMin(const nsAString& aMinSpec);
  nsresult          SetMax(const nsAString& aMaxSpec);
  nsresult          SetRestart(const nsAString& aRestartSpec);
  nsresult          SetRepeatCount(const nsAString& aRepeatCountSpec);
  nsresult          SetRepeatDur(const nsAString& aRepeatDurSpec);
  nsresult          SetFillMode(const nsAString& aFillModeSpec);

  void              UnsetBeginSpec();
  void              UnsetEndSpec();
  void              UnsetSimpleDuration();
  void              UnsetMin();
  void              UnsetMax();
  void              UnsetRestart();
  void              UnsetRepeatCount();
  void              UnsetRepeatDur();
  void              UnsetFillMode();

  nsresult          SetBeginOrEndSpec(const nsAString& aSpec, PRBool aIsBegin);

  /**
   * Calculates the first acceptable interval for this element.
   *
   * @see SMILANIM 3.6.8
   */
  nsresult          GetNextInterval(const nsSMILTimeValue& aBeginAfter,
                                    PRBool aFirstInstance,
                                    nsSMILInterval& aResult);
  PRBool            GetNextGreaterOrEqual(
                                   const nsTArray<nsSMILInstanceTime>& aList,
                                   const nsSMILTimeValue& aBase,
                                   PRInt32& aPosition,
                                   nsSMILTimeValue& aResult);
  nsSMILTimeValue   CalcActiveEnd(const nsSMILTimeValue& aBegin,
                                  const nsSMILTimeValue& aEnd);
  nsSMILTimeValue   GetRepeatDuration();
  nsSMILTimeValue   ApplyMinAndMax(const nsSMILTimeValue& aDuration);
  nsSMILTime        ActiveTimeToSimpleTime(nsSMILTime aActiveTime,
                                           PRUint32& aRepeatIteration);
  void              CheckForEarlyEnd(const nsSMILTimeValue& aDocumentTime);
  void              UpdateCurrentInterval();
  void              SampleSimpleTime(nsSMILTime aActiveTime);
  void              SampleFillValue();
  PRBool            AddInstanceTimeFromCurrentTime(double aOffsetSeconds,
                        PRBool aIsBegin, const nsSMILTimeContainer* aContainer);

  // Typedefs
  typedef nsTArray<nsRefPtr<nsSMILTimeValueSpec> >  SMILTimeValueSpecList;

  //
  // Members
  //
  SMILTimeValueSpecList mBeginSpecs;
  SMILTimeValueSpecList mEndSpecs;

  nsSMILTimeValue                 mSimpleDur;

  nsSMILRepeatCount               mRepeatCount;
  nsSMILTimeValue                 mRepeatDur;

  nsSMILTimeValue                 mMin;
  nsSMILTimeValue                 mMax;

  enum nsSMILFillMode
  {
    FILL_REMOVE,
    FILL_FREEZE
  };
  nsSMILFillMode                  mFillMode;
  static nsAttrValue::EnumTable   sFillModeTable[];

  enum nsSMILRestartMode
  {
    RESTART_ALWAYS,
    RESTART_WHENNOTACTIVE,
    RESTART_NEVER
  };
  nsSMILRestartMode               mRestartMode;
  static nsAttrValue::EnumTable   sRestartModeTable[];

  //
  // We need to distinguish between attempting to set the begin spec and failing
  // (in which case the mBeginSpecs array will be empty) and not attempting to
  // set the begin spec at all. In the first case, we should act as if the begin
  // was indefinite, and in the second, we should act as if begin was 0s.
  //
  PRPackedBool                    mBeginSpecSet;

  PRPackedBool                    mEndHasEventConditions;

  nsTArray<nsSMILInstanceTime>    mBeginInstances;
  nsTArray<nsSMILInstanceTime>    mEndInstances;

  nsSMILAnimationFunction*        mClient;
  nsSMILInterval                  mCurrentInterval;
  nsTArray<nsSMILInterval>        mOldIntervals;

  /**
   * The state of the element in its life-cycle. These states are based on the
   * element life-cycle described in SMILANIM 3.6.8
   */
  enum nsSMILElementState
  {
    STATE_STARTUP,
    STATE_WAITING,
    STATE_ACTIVE,
    STATE_POSTACTIVE
  };
  nsSMILElementState              mElementState;
};

#endif // NS_SMILTIMEDELEMENT_H_
