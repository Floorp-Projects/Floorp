/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* values to identify particular subtrees of native anonymous content */

#ifndef mozilla_AnonymousContentKey_h
#define mozilla_AnonymousContentKey_h

namespace mozilla {

// clang-format off

// We currently use cached anonymous content styles only for scrollbar parts,
// and we can fit the type of scrollbar part element along with its different
// options (such as orientation, and other attribute values that can affect
// styling) into a uint8_t.
//
// The lower three bits hold a Type_* value identifying the type of
// element, and the remaining bits store Flag_* values.
//
// A value of 0 is used to represent an anonymous content subtree that we don't
// cache styles for.
enum class AnonymousContentKey : uint8_t {
  None                           = 0x00,

  // all
  Type_ScrollCorner              = 0x01,
  Type_Resizer                   = 0x02,
  Type_Scrollbar                 = 0x03,
  Type_ScrollbarButton           = 0x04,
  Type_Slider                    = 0x05,

  // scrollbar, scrollbarbutton, slider
  Flag_Vertical                  = 0x08,

  // resizer
  // 3-bit bitfield in 0x38 storing dir="" and flip=""
  Flag_Resizer_Right             = 0x08,
  Flag_Resizer_Bottom            = 0x10,
  Flag_Resizer_BottomLeft        = 0x18,
  Flag_Resizer_BottomRight       = 0x20,
  Flag_Resizer_Bottom_Flip       = 0x28,

  // scrollbarbutton
  Flag_ScrollbarButton_Down      = 0x10,
  Flag_ScrollbarButton_Bottom    = 0x20,
  Flag_ScrollbarButton_Decrement = 0x40,
};

// clang-format on

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(AnonymousContentKey)

}  // namespace mozilla

#endif  // mozilla_AnonymousContentKey_h
