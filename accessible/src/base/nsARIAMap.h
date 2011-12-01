/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * The Initial Developer of the Original Code is IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Leventhal <aleventh@us.ibm.com>
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

#ifndef _nsARIAMap_H_
#define _nsARIAMap_H_

#include "prtypes.h"

class nsIAtom;
class nsIContent;

////////////////////////////////////////////////////////////////////////////////
// Value constants

/**
 * Used to define if role requires to expose nsIAccessibleValue.
 */
enum EValueRule
{
  /**
   * nsIAccessibleValue isn't exposed.
   */
  eNoValue,

  /**
   * nsIAccessibleValue is implemented, supports value, min and max from
   * aria-valuenow, aria-valuemin and aria-valuemax.
   */
  eHasValueMinMax
};


////////////////////////////////////////////////////////////////////////////////
// Action constants

/**
 * Used to define if the role requires to expose action.
 */
enum EActionRule
{
  eNoAction,
  eActivateAction,
  eClickAction,
  eCheckUncheckAction,
  eExpandAction,
  eJumpAction,
  eOpenCloseAction,
  eSelectAction,
  eSortAction,
  eSwitchAction
};


////////////////////////////////////////////////////////////////////////////////
// Live region constants

/**
 * Used to define if role exposes default value of aria-live attribute.
 */
enum ELiveAttrRule
{
  eNoLiveAttr,
  eOffLiveAttr,
  ePoliteLiveAttr
};


////////////////////////////////////////////////////////////////////////////////
// Role constants

/**
 * ARIA role overrides role from native markup.
 */
const bool kUseMapRole = true;

/**
 * ARIA role doesn't override the role from native markup.
 */
const bool kUseNativeRole = false;


////////////////////////////////////////////////////////////////////////////////
// ARIA attribute characteristic masks

/**
 * This mask indicates the attribute should not be exposed as an object
 * attribute via the catch-all logic in nsAccessible::GetAttributes.
 * This means it either isn't mean't to be exposed as an object attribute, or
 * that it should, but is already handled in other code.
 */
const PRUint8 ATTR_BYPASSOBJ  = 0x0001;

/**
 * This mask indicates the attribute is expected to have an NMTOKEN or bool value.
 * (See for example usage in nsAccessible::GetAttributes)
 */
const PRUint8 ATTR_VALTOKEN   = 0x0010;

/**
 * Small footprint storage of persistent aria attribute characteristics.
 */
struct nsAttributeCharacteristics
{
  nsIAtom** attributeName;
  const PRUint8 characteristics;
};


////////////////////////////////////////////////////////////////////////////////
// State map entry

/**
 * Used in nsRoleMapEntry.state if no nsIAccessibleStates are automatic for
 * a given role.
 */
#define kNoReqStates 0

enum eStateValueType
{
  kBoolType,
  kMixedType
};

enum EDefaultStateRule
{
  //eNoDefaultState,
  eUseFirstState
};

/**
 * ID for state map entry, used in nsRoleMapEntry.
 */
enum eStateMapEntryID
{
  eARIANone,
  eARIAAutoComplete,
  eARIABusy,
  eARIACheckableBool,
  eARIACheckableMixed,
  eARIACheckedMixed,
  eARIADisabled,
  eARIAExpanded,
  eARIAHasPopup,
  eARIAInvalid,
  eARIAMultiline,
  eARIAMultiSelectable,
  eARIAOrientation,
  eARIAPressed,
  eARIAReadonly,
  eARIAReadonlyOrEditable,
  eARIARequired,
  eARIASelectable
};

class nsStateMapEntry
{
public:
  /**
   * Used to create stub.
   */
  nsStateMapEntry();

  /**
   * Used for ARIA attributes having boolean or mixed values.
   */
  nsStateMapEntry(nsIAtom** aAttrName, eStateValueType aType,
                  PRUint64 aPermanentState,
                  PRUint64 aTrueState,
                  PRUint64 aFalseState = 0,
                  bool aDefinedIfAbsent = false);

  /**
   * Used for ARIA attributes having enumerated values.
   */
  nsStateMapEntry(nsIAtom** aAttrName,
                  const char* aValue1, PRUint64 aState1,
                  const char* aValue2, PRUint64 aState2,
                  const char* aValue3 = 0, PRUint64 aState3 = 0);

  /**
   * Used for ARIA attributes having enumerated values, and where a default
   * attribute state should be assumed when not supplied by the author.
   */
  nsStateMapEntry(nsIAtom** aAttrName, EDefaultStateRule aDefaultStateRule,
                  const char* aValue1, PRUint64 aState1,
                  const char* aValue2, PRUint64 aState2,
                  const char* aValue3 = 0, PRUint64 aState3 = 0);

  /**
   * Maps ARIA state map pointed by state map entry ID to accessible states.
   *
   * @param  aContent         [in] node of the accessible
   * @param  aState           [in/out] accessible states
   * @param  aStateMapEntryID [in] state map entry ID
   * @return                   true if state map entry ID is valid
   */
  static bool MapToStates(nsIContent* aContent, PRUint64* aState,
                            eStateMapEntryID aStateMapEntryID);

private:
  // ARIA attribute name
  nsIAtom** mAttributeName;

  // Indicates if attribute is token (can be undefined)
  bool mIsToken;

  // State applied always if attribute is defined
  PRUint64 mPermanentState;

  // States applied if attribute value is matched to the stored value
  const char* mValue1;
  PRUint64 mState1;

  const char* mValue2;
  PRUint64 mState2;

  const char* mValue3;
  PRUint64 mState3;

  // States applied if no stored values above are matched
  PRUint64 mDefaultState;

  // Permanent and false states are applied if attribute is absent
  bool mDefinedIfAbsent;
};


////////////////////////////////////////////////////////////////////////////////
// Role map entry

/**
 * For each ARIA role, this maps the nsIAccessible information.
 */
struct nsRoleMapEntry
{
  // ARIA role: string representation such as "button"
  const char *roleString;
  
  // Role mapping rule: maps to this nsIAccessibleRole
  PRUint32 role;
  
  // Role rule: whether to use mapped role or native semantics
  bool roleRule;
  
  // Value mapping rule: how to compute nsIAccessible value
  EValueRule valueRule;

  // Action mapping rule, how to expose nsIAccessible action
  EActionRule actionRule;

  // 'live' and 'container-live' object attributes mapping rule: how to expose
  // these object attributes if ARIA 'live' attribute is missed.
  ELiveAttrRule liveAttRule;

  // Automatic state mapping rule: always include in nsIAccessibleStates
  PRUint64 state;   // or kNoReqStates if no nsIAccessibleStates are automatic for this role.
  
  // ARIA properties supported for this role
  // (in other words, the aria-foo attribute to nsIAccessibleStates mapping rules)
  // Currently you cannot have unlimited mappings, because
  // a variable sized array would not allow the use of
  // C++'s struct initialization feature.
  eStateMapEntryID attributeMap1;
  eStateMapEntryID attributeMap2;
  eStateMapEntryID attributeMap3;
};


////////////////////////////////////////////////////////////////////////////////
// ARIA map

/**
 *  These are currently initialized (hardcoded) in nsARIAMap.cpp, 
 *  and provide the mappings for WAI-ARIA roles and properties using the 
 *  structs defined in this file.
 */
struct nsARIAMap
{
  /**
   * Array of supported ARIA role map entries and its length.
   */
  static nsRoleMapEntry gWAIRoleMap[];
  static PRUint32 gWAIRoleMapLength;

  /**
   * Landmark role map entry. Used when specified ARIA role isn't mapped to
   * accessibility API.
   */
  static nsRoleMapEntry gLandmarkRoleMap;

  /**
   * Empty role map entry. Used by accessibility service to create an accessible
   * if the accessible can't use role of used accessible class. For example,
   * it is used for table cells that aren't contained by table.
   */
  static nsRoleMapEntry gEmptyRoleMap;

  /**
   * State map of ARIA state attributes.
   */
  static nsStateMapEntry gWAIStateMap[];

  /**
   * State map of ARIA states applied to any accessible not depending on
   * the role.
   */
  static eStateMapEntryID gWAIUnivStateMap[];
  
  /**
   * Map of attribute to attribute characteristics.
   */
  static nsAttributeCharacteristics gWAIUnivAttrMap[];
  static PRUint32 gWAIUnivAttrMapLength;

  /**
   * Return accessible state from ARIA universal states applied to the given
   * element.
   */
  static PRUint64 UniversalStatesFor(nsIContent* aContent)
  {
    PRUint64 state = 0;
    PRUint32 index = 0;
    while (nsStateMapEntry::MapToStates(aContent, &state, gWAIUnivStateMap[index]))
      index++;

    return state;
  }
};

#endif
