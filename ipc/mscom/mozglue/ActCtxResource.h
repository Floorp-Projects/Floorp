/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ACT_CTX_RESOURCE_H
#define ACT_CTX_RESOURCE_H

#include <stdint.h>
#include <windows.h>
#include "mozilla/Types.h"

namespace mozilla {
namespace mscom {

struct ActCtxResource {
  uint16_t mId;
  HMODULE mModule;

  /**
   * Set the resource ID used by GetAccessibilityResource. This is so that
   * sandboxed child processes can use a value passed down from the parent.
   */
  static MFBT_API void SetAccessibilityResourceId(uint16_t aResourceId);

  /**
   * Get the resource ID used by GetAccessibilityResource.
   */
  static MFBT_API uint16_t GetAccessibilityResourceId();

  /**
   * @return ActCtxResource of a11y manifest resource to be passed to
   * mscom::ActivationContext
   */
  static MFBT_API ActCtxResource GetAccessibilityResource();
};

}  // namespace mscom
}  // namespace mozilla

#endif
