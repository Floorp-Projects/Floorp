/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Corporation
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsChangeHint_h___
#define nsChangeHint_h___

#include "prtypes.h"

// Defines for various style related constants

enum nsChangeHint {
  nsChangeHint_None = 0,          // change has no impact
  nsChangeHint_Unknown = 0x01,    // change has unknown impact
  nsChangeHint_AttrChange = 0x02, // change should cause notification to frame but nothing else
  nsChangeHint_Aural = 0x04,      // change was aural
  nsChangeHint_Content = 0x08,    // change was contentual (e.g., SRC=)
  nsChangeHint_RepaintFrame = 0x10,  // change was visual only (e.g., COLOR=)
  nsChangeHint_ReflowFrame = 0x20,   // change requires reflow (e.g., WIDTH=)
  nsChangeHint_SyncFrameView = 0x40, // change requires view to be updated, if there is one (e.g., clip:)
  nsChangeHint_ReconstructFrame = 0x80,   // change requires frame change (e.g., display:)
                                         // This subsumes all the above
  nsChangeHint_ReconstructDoc = 0x100
  // change requires reconstruction of entire document (e.g., style sheet change)
  // This subsumes all the above

  // TBD: add nsChangeHint_ForceFrameView to force frame reconstruction if the frame doesn't yet
  // have a view
};

#ifdef DEBUG_roc
// Redefine these operators to return nothing. This will catch any use
// of these operators on hints. We should not be using these operators
// on nsChangeHints
inline void operator<(nsChangeHint s1, nsChangeHint s2) {}
inline void operator>(nsChangeHint s1, nsChangeHint s2) {}
inline void operator!=(nsChangeHint s1, nsChangeHint s2) {}
inline void operator==(nsChangeHint s1, nsChangeHint s2) {}
inline void operator<=(nsChangeHint s1, nsChangeHint s2) {}
inline void operator>=(nsChangeHint s1, nsChangeHint s2) {}
#endif

// Operators on nsChangeHints

// Merge two hints, taking the union
inline nsChangeHint NS_CombineHint(nsChangeHint aH1, nsChangeHint aH2) {
  return (nsChangeHint)(aH1 | aH2);
}

// Merge two hints, taking the union
inline nsChangeHint NS_SubtractHint(nsChangeHint aH1, nsChangeHint aH2) {
  return (nsChangeHint)(aH1 & ~aH2);
}

// Merge the "src" hint into the "dst" hint
// Returns true iff the destination changed
inline PRBool NS_UpdateHint(nsChangeHint& aDest, nsChangeHint aSrc) {
  nsChangeHint r = (nsChangeHint)(aDest | aSrc);
  PRBool changed = (int)r != (int)aDest;
  aDest = r;
  return changed;
}

// Returns true iff the second hint contains all the hints of the first hint
inline PRBool NS_IsHintSubset(nsChangeHint aSubset, nsChangeHint aSuperSet) {
  return (aSubset & aSuperSet) == aSubset;
}

// Redefine the old NS_STYLE_HINT constants in terms of the new hint structure
const nsChangeHint NS_STYLE_HINT_UNKNOWN = nsChangeHint_Unknown;
const nsChangeHint NS_STYLE_HINT_NONE = nsChangeHint_None;
const nsChangeHint NS_STYLE_HINT_ATTRCHANGE = nsChangeHint_AttrChange;
const nsChangeHint NS_STYLE_HINT_AURAL = (nsChangeHint)
  (nsChangeHint_AttrChange | nsChangeHint_Aural);
const nsChangeHint NS_STYLE_HINT_CONTENT = (nsChangeHint)
  (nsChangeHint_AttrChange | nsChangeHint_Aural | nsChangeHint_Content);
const nsChangeHint NS_STYLE_HINT_VISUAL = (nsChangeHint)
  (nsChangeHint_AttrChange | nsChangeHint_Aural | nsChangeHint_Content | nsChangeHint_RepaintFrame
   | nsChangeHint_SyncFrameView);
const nsChangeHint NS_STYLE_HINT_REFLOW = (nsChangeHint)
  (nsChangeHint_AttrChange | nsChangeHint_Aural | nsChangeHint_Content | nsChangeHint_RepaintFrame
   | nsChangeHint_SyncFrameView | nsChangeHint_ReflowFrame);
const nsChangeHint NS_STYLE_HINT_FRAMECHANGE = (nsChangeHint)
  (nsChangeHint_AttrChange | nsChangeHint_Aural | nsChangeHint_Content | nsChangeHint_RepaintFrame
   | nsChangeHint_SyncFrameView | nsChangeHint_ReflowFrame | nsChangeHint_ReconstructFrame);
const nsChangeHint NS_STYLE_HINT_RECONSTRUCT_ALL = (nsChangeHint)
  (nsChangeHint_AttrChange | nsChangeHint_Aural | nsChangeHint_Content | nsChangeHint_RepaintFrame
   | nsChangeHint_SyncFrameView | nsChangeHint_ReflowFrame | nsChangeHint_ReconstructFrame
   | nsChangeHint_ReconstructDoc);

#endif /* nsChangeHint_h___ */
