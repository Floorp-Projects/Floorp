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
 *   Aaron Leventhal (aleventh@us.ibm.com)
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

#include <atk/atk.h>
#include "nsAccessibleWrap.h"

/******************************************************************************
The following nsIAccessible states aren't translated, just ignored:
  STATE_READONLY:        Supported indirectly via EXT_STATE_EDITABLE
  STATE_HOTTRACKED:      No ATK equivalent.  No known use case.
                         The nsIAccessible state is not currently supported.
  STATE_FLOATING:        No ATK equivalent.  No known use case.
                         The nsIAccessible state is not currently supported.
  STATE_MOVEABLE:        No ATK equivalent.  No known use case.
                         The nsIAccessible state is not currently supported.
  STATE_SELFVOICING:     No ATK equivalent -- the object has self-TTS.
                         The nsIAccessible state is not currently supported.
  STATE_LINKED:          The object is formatted as a hyperlink. Supported via ATK_ROLE_LINK.
  STATE_EXTSELECTABLE:   Indicates that an object extends its selection.
                         This is supported via STATE_MULTISELECTABLE.
  STATE_PROTECTED:       The object is a password-protected edit control.
                         Supported via ATK_ROLE_PASSWORD_TEXT
  STATE_HASPOPUP:        Object displays a pop-up menu or window when invoked.
                         No ATK equivalent.  The nsIAccessible state is not currently supported.

The following ATK states are not supported:
  ATK_STATE_ARMED:       No clear use case, used briefly when button is activated
  ATK_STATE_HAS_TOOLTIP: No clear use case, no IA2 equivalent
  ATK_STATE_ICONIFIED:   Mozilla does not have elements which are collapsable into icons
  ATK_STATE_TRUNCATED:   No clear use case. Indicates that an object's onscreen content is truncated, 
                         e.g. a text value in a spreadsheet cell. No IA2 state.
******************************************************************************/

enum EStateMapEntryType {
  kMapDirectly,
  kMapOpposite,   // For example, UNAVAILABLE is the opposite of ENABLED
  kNoStateChange, // Don't fire state change event
  kNoSuchState
};

const AtkStateType kNone = ATK_STATE_INVALID;

struct AtkStateMap {
  AtkStateType atkState;
  EStateMapEntryType stateMapEntryType;

  static PRInt32 GetStateIndexFor(PRUint32 aState)
  {
    PRInt32 stateIndex = -1;
    while (aState > 0) {
      ++ stateIndex;
      aState >>= 1;
    }
    return stateIndex;  // Returns -1 if not mapped
  }
};


// Map array from cross platform roles to  ATK roles
static const AtkStateMap gAtkStateMap[] = {                       // Cross Platform States
  { kNone,                                    kMapOpposite },     // STATE_UNAVAILABLE     = 0x00000001
  { ATK_STATE_SELECTED,                       kMapDirectly },     // STATE_SELECTED        = 0x00000002
  { ATK_STATE_FOCUSED,                        kMapDirectly },     // STATE_FOCUSED         = 0x00000004
  { ATK_STATE_PRESSED,                        kMapDirectly },     // STATE_PRESSED         = 0x00000008
  { ATK_STATE_CHECKED,                        kMapDirectly },     // STATE_CHECKED         = 0x00000010
  { ATK_STATE_INDETERMINATE,                  kMapDirectly },     // STATE_MIXED           = 0x00000020
  { kNone,                                    kMapDirectly },     // STATE_READONLY        = 0x00000040
  { kNone,                                    kMapDirectly },     // STATE_HOTTRACKED      = 0x00000080
  { ATK_STATE_DEFAULT,                        kMapDirectly },     // STATE_DEFAULT         = 0x00000100
  { ATK_STATE_EXPANDED,                       kMapDirectly },     // STATE_EXPANDED        = 0x00000200
  { kNone,                                    kNoStateChange },   // STATE_COLLAPSED       = 0x00000400
  { ATK_STATE_BUSY,                           kMapDirectly },     // STATE_BUSY            = 0x00000800
  { kNone,                                    kMapDirectly },     // STATE_FLOATING        = 0x00001000
  { kNone,                                    kMapDirectly },     // STATE_CHECKABLE       = 0x00002000
  { ATK_STATE_ANIMATED,                       kMapDirectly },     // STATE_ANIMATED        = 0x00004000
  { ATK_STATE_VISIBLE,                        kMapOpposite },     // STATE_INVISIBLE       = 0x00008000
  { ATK_STATE_SHOWING,                        kMapOpposite },     // STATE_OFFSCREEN       = 0x00010000
  { ATK_STATE_RESIZABLE,                      kMapDirectly },     // STATE_SIZEABLE        = 0x00020000
  { kNone,                                    kMapDirectly },     // STATE_MOVEABLE        = 0x00040000
  { kNone,                                    kMapDirectly },     // STATE_SELFVOICING     = 0x00080000
  { ATK_STATE_FOCUSABLE,                      kMapDirectly },     // STATE_FOCUSABLE       = 0x00100000
  { ATK_STATE_SELECTABLE,                     kMapDirectly },     // STATE_SELECTABLE      = 0x00200000
  { kNone,                                    kMapDirectly },     // STATE_LINKED          = 0x00400000
  { ATK_STATE_VISITED,                        kMapDirectly },     // STATE_TRAVERSED       = 0x00800000
  { ATK_STATE_MULTISELECTABLE,                kMapDirectly },     // STATE_MULTISELECTABLE = 0x01000000
  { kNone,                                    kMapDirectly },     // STATE_EXTSELECTABLE   = 0x02000000
  { ATK_STATE_REQUIRED,                       kMapDirectly },     // STATE_REQUIRED        = 0x04000000
  { kNone,                                    kMapDirectly },     // STATE_ALERT_MEDIUM    = 0x08000000
  { ATK_STATE_INVALID_ENTRY,                  kMapDirectly },     // STATE_INVALID         = 0x10000000
  { kNone,                                    kMapDirectly },     // STATE_PROTECTED       = 0x20000000
  { kNone,                                    kMapDirectly },     // STATE_HASPOPUP        = 0x40000000
  { kNone,                                    kNoSuchState },     //                       = 0x80000000  
};

static const AtkStateMap gAtkStateMapExt[] = {                    // Cross Platform States
  { ATK_STATE_SUPPORTS_AUTOCOMPLETION,        kMapDirectly },     // EXT_STATE_SUPPORTS_AUTOCOMPLETION = 0x00000001
  { ATK_STATE_DEFUNCT,                        kMapDirectly },     // EXT_STATE_DEFUNCT                 = 0x00000002
  { ATK_STATE_SELECTABLE_TEXT,                kMapDirectly },     // EXT_STATE_SELECTABLE_TEXT         = 0x00000004
  { ATK_STATE_EDITABLE,                       kMapDirectly },     // EXT_STATE_EDITABLE                = 0x00000008
  { ATK_STATE_ACTIVE,                         kMapDirectly },     // EXT_STATE_ACTIVE                  = 0x00000010
  { ATK_STATE_MODAL,                          kMapDirectly },     // EXT_STATE_MODAL                   = 0x00000020
  { ATK_STATE_MULTI_LINE,                     kMapDirectly },     // EXT_STATE_MULTI_LINE              = 0x00000040
  { ATK_STATE_HORIZONTAL,                     kMapDirectly },     // EXT_STATE_HORIZONTAL              = 0x00000080
  { ATK_STATE_OPAQUE,                         kMapDirectly },     // EXT_STATE_OPAQUE                  = 0x00000100
  { ATK_STATE_SINGLE_LINE,                    kMapDirectly },     // EXT_STATE_SINGLE_LINE             = 0x00000200
  { ATK_STATE_TRANSIENT,                      kMapDirectly },     // EXT_STATE_TRANSIENT               = 0x00000400
  { ATK_STATE_VERTICAL,                       kMapDirectly },     // EXT_STATE_VERTICAL                = 0x00000800
  { ATK_STATE_STALE,                          kMapDirectly },     // EXT_STATE_STALE                   = 0x00001000
  { ATK_STATE_ENABLED,                        kMapDirectly },     // EXT_STATE_ENABLED                 = 0x00002000
  { ATK_STATE_SENSITIVE,                      kMapDirectly },     // EXT_STATE_SENSITIVE               = 0x00004000
  { ATK_STATE_EXPANDABLE,                     kMapDirectly },     // EXT_STATE_EXPANDABLE              = 0x00008000
  { kNone,                                    kNoSuchState },     //                                   = 0x00010000
  { kNone,                                    kNoSuchState },     //                                   = 0x00020000
  { kNone,                                    kNoSuchState },     //                                   = 0x00040000
  { kNone,                                    kNoSuchState },     //                                   = 0x00080000
  { kNone,                                    kNoSuchState },     //                                   = 0x00100000
  { kNone,                                    kNoSuchState },     //                                   = 0x00200000
  { kNone,                                    kNoSuchState },     //                                   = 0x00400000
  { kNone,                                    kNoSuchState },     //                                   = 0x00800000
  { kNone,                                    kNoSuchState },     //                                   = 0x01000000
  { kNone,                                    kNoSuchState },     //                                   = 0x02000000
  { kNone,                                    kNoSuchState },     //                                   = 0x04000000
  { kNone,                                    kNoSuchState },     //                                   = 0x08000000
  { kNone,                                    kNoSuchState },     //                                   = 0x10000000
  { kNone,                                    kNoSuchState },     //                                   = 0x20000000
  { kNone,                                    kNoSuchState },     //                                   = 0x40000000
  { kNone,                                    kNoSuchState },     //                                   = 0x80000000
};
