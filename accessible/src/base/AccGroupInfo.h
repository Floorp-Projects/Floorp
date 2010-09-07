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

#include "nsAccessible.h"
#include "nsAccUtils.h"

/**
 * Calculate and store group information.
 */
class AccGroupInfo
{
public:
  AccGroupInfo(nsAccessible* aItem, PRUint32 aRole);
  ~AccGroupInfo() { MOZ_COUNT_DTOR(AccGroupInfo); }

  PRInt32 PosInSet() const { return mPosInSet; }
  PRUint32 SetSize() const { return mSetSize; }
  nsAccessible* GetConceptualParent() const { return mParent; }

  /**
   * Create group info.
   */
  static AccGroupInfo* CreateGroupInfo(nsAccessible* aAccessible)
  {
    PRUint32 role = aAccessible->Role();
    if (role != nsIAccessibleRole::ROLE_ROW &&
        role != nsIAccessibleRole::ROLE_GRID_CELL &&
        role != nsIAccessibleRole::ROLE_OUTLINEITEM &&
        role != nsIAccessibleRole::ROLE_OPTION &&
        role != nsIAccessibleRole::ROLE_LISTITEM &&
        role != nsIAccessibleRole::ROLE_MENUITEM &&
        role != nsIAccessibleRole::ROLE_CHECK_MENU_ITEM &&
        role != nsIAccessibleRole::ROLE_RADIO_MENU_ITEM &&
        role != nsIAccessibleRole::ROLE_RADIOBUTTON &&
        role != nsIAccessibleRole::ROLE_PAGETAB)
      return nsnull;

    AccGroupInfo* info = new AccGroupInfo(aAccessible, BaseRole(role));
    return info;
  }

private:
  AccGroupInfo(const AccGroupInfo&);
  AccGroupInfo& operator =(const AccGroupInfo&);

  static PRUint32 BaseRole(PRUint32 aRole)
  {
    if (aRole == nsIAccessibleRole::ROLE_CHECK_MENU_ITEM ||
        aRole == nsIAccessibleRole::ROLE_RADIO_MENU_ITEM)
      return nsIAccessibleRole::ROLE_MENUITEM;
    return aRole;
  }

  PRUint32 mPosInSet;
  PRUint32 mSetSize;
  nsAccessible* mParent;
};

#endif
