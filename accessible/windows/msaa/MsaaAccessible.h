/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_MsaaAccessible_h_
#define mozilla_a11y_MsaaAccessible_h_

#include "ia2Accessible.h"
#include "ia2AccessibleComponent.h"
#include "ia2AccessibleHyperlink.h"
#include "ia2AccessibleValue.h"
#include "mozilla/a11y/MsaaIdGenerator.h"
#include "mozilla/dom/ipc/IdType.h"

namespace mozilla {
namespace a11y {
class AccessibleWrap;
class LocalAccessible;
class sdnAccessible;

class MsaaAccessible : public ia2Accessible,
                       public ia2AccessibleComponent,
                       public ia2AccessibleHyperlink,
                       public ia2AccessibleValue {
 public:
  MsaaAccessible();

  AccessibleWrap* LocalAcc();

  uint32_t GetExistingID() const { return mID; }
  static const uint32_t kNoID = 0;
  void SetID(uint32_t aID);

  static int32_t GetChildIDFor(LocalAccessible* aAccessible);
  static uint32_t GetContentProcessIdFor(dom::ContentParentId aIPCContentId);
  static void ReleaseContentProcessIdFor(dom::ContentParentId aIPCContentId);
  static void AssignChildIDTo(NotNull<sdnAccessible*> aSdnAcc);
  static void ReleaseChildID(NotNull<sdnAccessible*> aSdnAcc);

  /**
   * Find an accessible by the given child ID in cached documents.
   */
  [[nodiscard]] already_AddRefed<IAccessible> GetIAccessibleFor(
      const VARIANT& aVarChild, bool* aIsDefunct);

 protected:
  virtual ~MsaaAccessible();

  uint32_t mID;
  static MsaaIdGenerator sIDGen;

  HRESULT
  ResolveChild(const VARIANT& aVarChild, IAccessible** aOutInterface);

 private:
  /**
   * Find a remote accessible by the given child ID.
   */
  [[nodiscard]] already_AddRefed<IAccessible> GetRemoteIAccessibleFor(
      const VARIANT& aVarChild);
};

}  // namespace a11y
}  // namespace mozilla

#ifdef XP_WIN
// Undo the windows.h damage
#  undef GetMessage
#  undef CreateEvent
#  undef GetClassName
#  undef GetBinaryType
#  undef RemoveDirectory
#endif

#endif
