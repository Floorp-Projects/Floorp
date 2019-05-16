/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
// IWYU pragma: private, include "nsDisplayList.h"

/**
 * It's useful to be able to dynamically check the type of certain items.
 * Every subclass of nsDisplayItem must have a new type added here for the
 * purposes of easy comparison and matching of items in different display lists.
 */

#ifndef NSDISPLAYITEMTYPES_H_
#define NSDISPLAYITEMTYPES_H_

enum class DisplayItemType : uint8_t {
  TYPE_ZERO = 0, /** Spacer so that the first item starts at 1 */

#define DECLARE_DISPLAY_ITEM_TYPE(name, flags) TYPE_##name,
#include "nsDisplayItemTypesList.h"
#undef DECLARE_DISPLAY_ITEM_TYPE

  TYPE_MAX
};

enum {
  // Number of bits needed to represent all types
  TYPE_BITS = 8
};

enum DisplayItemFlags {
  TYPE_RENDERS_NO_IMAGES = 1 << 0,
  TYPE_IS_CONTENTFUL = 1 << 1,
  TYPE_IS_CONTAINER = 1 << 2
};

inline const char* DisplayItemTypeName(DisplayItemType aType) {
  switch (aType) {
#define DECLARE_DISPLAY_ITEM_TYPE(name, flags) \
  case DisplayItemType::TYPE_##name:           \
    return #name;
#include "nsDisplayItemTypesList.h"
#undef DECLARE_DISPLAY_ITEM_TYPE

    default:
      return "TYPE_UNKNOWN";
  }
}

inline uint8_t GetDisplayItemFlagsForType(DisplayItemType aType) {
  static const uint8_t flags[static_cast<uint32_t>(DisplayItemType::TYPE_MAX)] =
      {0
#define DECLARE_DISPLAY_ITEM_TYPE(name, flags) , flags
#include "nsDisplayItemTypesList.h"
#undef DECLARE_DISPLAY_ITEM_TYPE
      };

  return flags[static_cast<uint32_t>(aType)];
}

inline DisplayItemType GetDisplayItemTypeFromKey(uint32_t aDisplayItemKey) {
  static const uint32_t typeMask = (1 << TYPE_BITS) - 1;
  DisplayItemType type =
      static_cast<DisplayItemType>(aDisplayItemKey & typeMask);
  NS_ASSERTION(
      type >= DisplayItemType::TYPE_ZERO && type < DisplayItemType::TYPE_MAX,
      "Invalid display item type!");
  return type;
}

#endif /*NSDISPLAYITEMTYPES_H_*/
