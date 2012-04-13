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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef AccGroupInfo_h_
#define AccGroupInfo_h_

#include "Accessible-inl.h"
#include "nsAccUtils.h"

/**
 * Calculate and store group information.
 */
class AccGroupInfo
{
public:
  AccGroupInfo(nsAccessible* aItem, mozilla::a11y::role aRole);
  ~AccGroupInfo() { MOZ_COUNT_DTOR(AccGroupInfo); }

  PRInt32 PosInSet() const { return mPosInSet; }
  PRUint32 SetSize() const { return mSetSize; }
  nsAccessible* ConceptualParent() const { return mParent; }

  /**
   * Create group info.
   */
  static AccGroupInfo* CreateGroupInfo(nsAccessible* aAccessible)
  {
    mozilla::a11y::role role = aAccessible->Role();
    if (role != mozilla::a11y::roles::ROW &&
        role != mozilla::a11y::roles::GRID_CELL &&
        role != mozilla::a11y::roles::OUTLINEITEM &&
        role != mozilla::a11y::roles::OPTION &&
        role != mozilla::a11y::roles::LISTITEM &&
        role != mozilla::a11y::roles::MENUITEM &&
        role != mozilla::a11y::roles::COMBOBOX_OPTION &&
        role != mozilla::a11y::roles::RICH_OPTION &&
        role != mozilla::a11y::roles::CHECK_RICH_OPTION &&
        role != mozilla::a11y::roles::PARENT_MENUITEM &&
        role != mozilla::a11y::roles::CHECK_MENU_ITEM &&
        role != mozilla::a11y::roles::RADIO_MENU_ITEM &&
        role != mozilla::a11y::roles::RADIOBUTTON &&
        role != mozilla::a11y::roles::PAGETAB)
      return nsnull;

    AccGroupInfo* info = new AccGroupInfo(aAccessible, BaseRole(role));
    return info;
  }

private:
  AccGroupInfo(const AccGroupInfo&);
  AccGroupInfo& operator =(const AccGroupInfo&);

  static mozilla::a11y::role BaseRole(mozilla::a11y::role aRole)
  {
    if (aRole == mozilla::a11y::roles::CHECK_MENU_ITEM ||
        aRole == mozilla::a11y::roles::PARENT_MENUITEM ||
        aRole == mozilla::a11y::roles::RADIO_MENU_ITEM)
      return mozilla::a11y::roles::MENUITEM;

    if (aRole == mozilla::a11y::roles::CHECK_RICH_OPTION)
      return mozilla::a11y::roles::RICH_OPTION;

    return aRole;
  }

  /**
   * Return true if the given parent role is conceptual parent of the given
   * role.
   */
  static bool IsConceptualParent(mozilla::a11y::role aRole,
				 mozilla::a11y::role aParentRole);

  PRUint32 mPosInSet;
  PRUint32 mSetSize;
  nsAccessible* mParent;
};

#endif
