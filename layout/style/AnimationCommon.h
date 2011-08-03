/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is AnimationCommon, common animation code for transitions
 * and animations.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
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

#ifndef mozilla_css_AnimationCommon_h
#define mozilla_css_AnimationCommon_h

#include "nsIStyleRuleProcessor.h"
#include "nsIStyleRule.h"
#include "nsRefreshDriver.h"
#include "prclist.h"
#include "nsStyleAnimation.h"
#include "nsCSSProperty.h"
#include "mozilla/dom/Element.h"
#include "nsSMILKeySpline.h"
#include "nsStyleStruct.h"

class nsPresContext;

namespace mozilla {
namespace css {

struct CommonElementAnimationData;

class CommonAnimationManager : public nsIStyleRuleProcessor,
                               public nsARefreshObserver {
public:
  CommonAnimationManager(nsPresContext *aPresContext);
  virtual ~CommonAnimationManager();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIStyleRuleProcessor (parts)
  virtual nsRestyleHint HasStateDependentStyle(StateRuleProcessorData* aData);
  virtual PRBool HasDocumentStateDependentStyle(StateRuleProcessorData* aData);
  virtual nsRestyleHint
    HasAttributeDependentStyle(AttributeRuleProcessorData* aData);
  virtual PRBool MediumFeaturesChanged(nsPresContext* aPresContext);
  virtual PRInt64 SizeOf() const;

  /**
   * Notify the manager that the pres context is going away.
   */
  void Disconnect();

  static PRBool ExtractComputedValueForTransition(
                  nsCSSProperty aProperty,
                  nsStyleContext* aStyleContext,
                  nsStyleAnimation::Value& aComputedValue);
protected:
  friend struct CommonElementAnimationData; // for ElementDataRemoved

  void AddElementData(CommonElementAnimationData* aData);
  void ElementDataRemoved();
  void RemoveAllElementData();

  PRCList mElementData;
  nsPresContext *mPresContext; // weak (non-null from ctor to Disconnect)
};

/**
 * A style rule that maps property-nsStyleAnimation::Value pairs.
 */
class AnimValuesStyleRule : public nsIStyleRule
{
public:
  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // nsIStyleRule implementation
  virtual void MapRuleInfoInto(nsRuleData* aRuleData);
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  void AddValue(nsCSSProperty aProperty, nsStyleAnimation::Value &aStartValue)
  {
    PropertyValuePair v = { aProperty, aStartValue };
    mPropertyValuePairs.AppendElement(v);
  }

  // Caller must fill in returned value.
  nsStyleAnimation::Value* AddEmptyValue(nsCSSProperty aProperty)
  {
    PropertyValuePair *p = mPropertyValuePairs.AppendElement();
    p->mProperty = aProperty;
    return &p->mValue;
  }

  struct PropertyValuePair {
    nsCSSProperty mProperty;
    nsStyleAnimation::Value mValue;
  };

private:
  InfallibleTArray<PropertyValuePair> mPropertyValuePairs;
};

class ComputedTimingFunction {
public:
  typedef nsTimingFunction::Type Type;
  void Init(const nsTimingFunction &aFunction);
  double GetValue(double aPortion) const;
private:
  Type mType;
  nsSMILKeySpline mTimingFunction;
  PRUint32 mSteps;
};

struct CommonElementAnimationData : public PRCList
{
  CommonElementAnimationData(dom::Element *aElement, nsIAtom *aElementProperty,
                             CommonAnimationManager *aManager)
    : mElement(aElement)
    , mElementProperty(aElementProperty)
    , mManager(aManager)
#ifdef DEBUG
    , mCalledPropertyDtor(false)
#endif
  {
    MOZ_COUNT_CTOR(CommonElementAnimationData);
    PR_INIT_CLIST(this);
  }
  ~CommonElementAnimationData()
  {
    NS_ABORT_IF_FALSE(mCalledPropertyDtor,
                      "must call destructor through element property dtor");
    MOZ_COUNT_DTOR(CommonElementAnimationData);
    PR_REMOVE_LINK(this);
    mManager->ElementDataRemoved();
  }

  void Destroy()
  {
    // This will call our destructor.
    mElement->DeleteProperty(mElementProperty);
  }

  dom::Element *mElement;

  // the atom we use in mElement's prop table (must be a static atom,
  // i.e., in an atom list)
  nsIAtom *mElementProperty;

  CommonAnimationManager *mManager;

#ifdef DEBUG
  bool mCalledPropertyDtor;
#endif
};

}
}

#endif /* !defined(mozilla_css_AnimationCommon_h) */
