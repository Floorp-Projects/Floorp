/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScreenOrientation_h
#define mozilla_dom_ScreenOrientation_h

#include "IPC/IPCMessageUtils.h"

namespace mozilla {
namespace dom {

// Make sure that any change here is also made in
// * mobile/android/base/GeckoScreenOrientationListener.java
// * embedding/android/GeckoScreenOrientationListener.java
enum ScreenOrientation {
  eScreenOrientation_None               = 0,
  eScreenOrientation_PortraitPrimary    = 1,  // 00000001
  eScreenOrientation_PortraitSecondary  = 2,  // 00000010
  eScreenOrientation_Portrait           = 3,  // 00000011
  eScreenOrientation_LandscapePrimary   = 4,  // 00000100
  eScreenOrientation_LandscapeSecondary = 8,  // 00001000
  eScreenOrientation_Landscape          = 12, // 00001100
  eScreenOrientation_EndGuard
};

} // namespace dom
} // namespace mozilla

namespace IPC {

/**
 * Screen orientation serializer.
 * Note that technically, 5, 6, 7, 9, 10 and 11 are illegal values but will
 * not make the serializer to fail. We might want to write our own serializer.
 */
template <>
struct ParamTraits<mozilla::dom::ScreenOrientation>
  : public EnumSerializer<mozilla::dom::ScreenOrientation,
                          mozilla::dom::eScreenOrientation_None,
                          mozilla::dom::eScreenOrientation_EndGuard>
{};

} // namespace IPC

#endif // mozilla_dom_ScreenOrientation_h
