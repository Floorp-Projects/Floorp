/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AccGroupInfo_h_
#define AccGroupInfo_h_

#include "nsISupportsImpl.h"
#include "Role.h"

namespace mozilla {
namespace a11y {

class Accessible;

/**
 * Calculate and store group information.
 */
class AccGroupInfo {
 public:
  MOZ_COUNTED_DTOR(AccGroupInfo)

  AccGroupInfo() = default;
  AccGroupInfo(AccGroupInfo&&) = default;
  AccGroupInfo& operator=(AccGroupInfo&&) = default;

  /**
   * Return 1-based position in the group.
   */
  uint32_t PosInSet() const { return mPosInSet; }

  /**
   * Return a number of items in the group.
   */
  uint32_t SetSize() const { return mSetSize; }

  /**
   * Return a direct or logical parent of the accessible that this group info is
   * created for.
   */
  Accessible* ConceptualParent() const { return mParent; }

  /**
   * Update group information.
   */
  void Update();

  /**
   * Create group info.
   */
  static AccGroupInfo* CreateGroupInfo(const Accessible* aAccessible);

  /**
   * Return a first item for the given container.
   */
  static Accessible* FirstItemOf(const Accessible* aContainer);

  /**
   * Return total number of items in container, and if it is has nested
   * collections.
   */
  static uint32_t TotalItemCount(Accessible* aContainer, bool* aIsHierarchical);

  /**
   * Return next item of the same group to the given item.
   */
  static Accessible* NextItemTo(Accessible* aItem);

 protected:
  AccGroupInfo(const Accessible* aItem, a11y::role aRole);

 private:
  AccGroupInfo(const AccGroupInfo&) = delete;
  AccGroupInfo& operator=(const AccGroupInfo&) = delete;

  static mozilla::a11y::role BaseRole(mozilla::a11y::role aRole) {
    if (aRole == mozilla::a11y::roles::CHECK_MENU_ITEM ||
        aRole == mozilla::a11y::roles::PARENT_MENUITEM ||
        aRole == mozilla::a11y::roles::RADIO_MENU_ITEM) {
      return mozilla::a11y::roles::MENUITEM;
    }

    if (aRole == mozilla::a11y::roles::CHECK_RICH_OPTION) {
      return mozilla::a11y::roles::RICH_OPTION;
    }

    return aRole;
  }

  /**
   * Return true if the given parent and child roles should have their node
   * relations reported.
   */
  static bool ShouldReportRelations(a11y::role aRole, a11y::role aParentRole);

  /**
   * Return ARIA level value or the default one if ARIA is missed for the
   * given accessible.
   */
  static int32_t GetARIAOrDefaultLevel(const Accessible* aAccessible);

  uint32_t mPosInSet;
  uint32_t mSetSize;
  Accessible* mParent;
  const Accessible* mItem;
  a11y::role mRole;
};

}  // namespace a11y
}  // namespace mozilla

#endif
