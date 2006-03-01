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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * Novell, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Allan Beaufour <abeaufour@novell.com>
 *  David Landwehr <dlandwehr@novell.com>
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

#ifndef __NSXFORMSNODESTATE_H__
#define __NSXFORMSNODESTATE_H__

#include "nscore.h"

#ifdef DEBUG
//#define DEBUG_XF_NODESTATE
#endif

/** Flags used in nsXFormsNodeState */
enum eFlag_t {
  // The different states
  eFlag_READONLY                    = 1 << 1,
  eFlag_CONSTRAINT                  = 1 << 2,
  eFlag_CONSTRAINT_SCHEMA           = 1 << 3,
  eFlag_RELEVANT                    = 1 << 4,
  eFlag_REQUIRED                    = 1 << 5,
  eFlag_INHERITED_RELEVANT          = 1 << 6,
  eFlag_INHERITED_READONLY          = 1 << 7,
  // Events to be dispatched
  eFlag_DISPATCH_VALUE_CHANGED      = 1 << 8,
  eFlag_DISPATCH_READONLY_CHANGED   = 1 << 9,
  eFlag_DISPATCH_VALID_CHANGED      = 1 << 10,
  eFlag_DISPATCH_RELEVANT_CHANGED   = 1 << 11,
  eFlag_DISPATCH_REQUIRED_CHANGED   = 1 << 12,
  eFlag_DISPATCH_CONSTRAINT_CHANGED = 1 << 13
};

// Default flags set for new states
const PRUint16 kFlags_DEFAULT =
  (eFlag_CONSTRAINT         | // The node is valid
   eFlag_CONSTRAINT_SCHEMA  | // The node is valid (schema)
   eFlag_RELEVANT           | // The node is relevant
   eFlag_INHERITED_RELEVANT); // Children should inherit relevant

// The events dispatched for newly created models
const PRUint16 kFlags_INITIAL_DISPATCH =
  (eFlag_DISPATCH_READONLY_CHANGED |
   eFlag_DISPATCH_VALID_CHANGED    |
   eFlag_DISPATCH_RELEVANT_CHANGED |
   eFlag_DISPATCH_REQUIRED_CHANGED);

// All dispatch flags
const PRUint16 kFlags_ALL_DISPATCH =
  (eFlag_DISPATCH_READONLY_CHANGED   |
   eFlag_DISPATCH_VALID_CHANGED      |
   eFlag_DISPATCH_RELEVANT_CHANGED   |
   eFlag_DISPATCH_REQUIRED_CHANGED   |
   eFlag_DISPATCH_VALUE_CHANGED      |
   eFlag_DISPATCH_CONSTRAINT_CHANGED);

// All but dispatch flags
const PRUint16 kFlags_NOT_DISPATCH = ~kFlags_ALL_DISPATCH;

/**
 * Holds the current state of a MDG node.
 *
 * That is, whether the node is readonly, relevant, etc., and also information
 * about which events that should be dispatched to any controls bound to the
 * node.
 */
class nsXFormsNodeState
{
private:
  /** The node state, bit vector of eFlag_t */
  PRUint16 mState;
  
public:
  /**
   * Constructor.
   *
   * @param aInitialState     The initial state of this
   */
  nsXFormsNodeState(PRUint16 aInitialState = kFlags_DEFAULT)
    : mState(aInitialState) {};

  /**
   * Sets flag(s) on or off
   * 
   * @param aFlags           The flag(s) to set
   * @param aVal             The flag value
   */
  void Set(PRUint16 aFlags,
           PRBool   aVal);

  /**
   * And own state with a bit mask.
   *
   * @param aMask             Bit mask
   * @return                  New value
   */
  nsXFormsNodeState& operator&=(PRUint16 aMask);
  
  /**
   * Comparator.
   *
   * @param aCmp              Object to compare with
   * @return                  Objects equal?
   */
  PRBool operator==(nsXFormsNodeState& aCmp) const;

  /**
   * Get flag state and clear flag.
   * 
   * @param aFlag            The flag
   * @return                 The flag state
   */
  PRBool TestAndClear(eFlag_t aFlag);

  /**
   * Get flag state
   * 
   * @param aFlag            The flag   
   * @return                 The flag state
   */
  PRBool Test(eFlag_t aFlag) const; 

  // Check states
  PRBool IsValid() const
    { return Test(eFlag_CONSTRAINT) &&
             Test(eFlag_CONSTRAINT_SCHEMA); };
  PRBool IsConstraint() const
    { return Test(eFlag_CONSTRAINT); };
  PRBool IsConstraintSchema() const
    { return Test(eFlag_CONSTRAINT_SCHEMA); };
  PRBool IsReadonly() const
    { return Test(eFlag_READONLY) |
             Test(eFlag_INHERITED_READONLY); };
  PRBool IsRelevant() const
    { return Test(eFlag_RELEVANT) &&
             Test(eFlag_INHERITED_RELEVANT); };
  PRBool IsRequired() const
    { return Test(eFlag_REQUIRED); };

  // Check events
  PRBool ShouldDispatchReadonly() const
    { return Test(eFlag_DISPATCH_READONLY_CHANGED); };
  PRBool ShouldDispatchRelevant() const
    { return Test(eFlag_DISPATCH_RELEVANT_CHANGED); };
  PRBool ShouldDispatchValid() const
    { return Test(eFlag_DISPATCH_VALID_CHANGED); };
  PRBool ShouldDispatchRequired() const
    { return Test(eFlag_DISPATCH_REQUIRED_CHANGED); };
  PRBool ShouldDispatchValueChanged() const
    { return Test(eFlag_DISPATCH_VALUE_CHANGED); };

  PRUint32 GetIntrinsicState() const;

#ifdef DEBUG_XF_NODESTATE
  /** Print the flags currently set to stdout */
  void PrintFlags() const;
#endif
};

#endif
