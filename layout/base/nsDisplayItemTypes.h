/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * It's useful to be able to dynamically check the type of certain items.
 * Every subclass of nsDisplayItem must have a new type added here for the purposes
 * of easy comparison and matching of items in different display lists.
 * 
 * This is #included inside nsDisplayItem.
 */


enum Type {
  TYPE_ZERO = 0, /** Spacer so that the first item starts at 1 */

#define DECLARE_DISPLAY_ITEM_TYPE(name) TYPE_##name,
#define DECLARE_DISPLAY_ITEM_TYPE_FLAGS(name,flags) TYPE_##name,
#include "nsDisplayItemTypesList.h"
#undef DECLARE_DISPLAY_ITEM_TYPE
#undef DECLARE_DISPLAY_ITEM_TYPE_FLAGS

  TYPE_MAX
};

enum {
  // Number of bits needed to represent all types
  TYPE_BITS = 8
};

enum DisplayItemFlags {
  TYPE_RENDERS_NO_IMAGES = 1 << 0
};

static const char* DisplayItemTypeName(Type aType)
{
  switch (aType) {
#define DECLARE_DISPLAY_ITEM_TYPE(name) case TYPE_##name: return #name;
#define DECLARE_DISPLAY_ITEM_TYPE_FLAGS(name,flags) case TYPE_##name: return #name;
#include "nsDisplayItemTypesList.h"
#undef DECLARE_DISPLAY_ITEM_TYPE
#undef DECLARE_DISPLAY_ITEM_TYPE_FLAGS

    default: return "TYPE_UNKNOWN";
  }
}

static uint8_t GetDisplayItemFlagsForType(Type aType)
{
  static const uint8_t flags[TYPE_MAX] = {
    0
#define DECLARE_DISPLAY_ITEM_TYPE(name) ,0
#define DECLARE_DISPLAY_ITEM_TYPE_FLAGS(name,flags) ,flags
#include "nsDisplayItemTypesList.h"
#undef DECLARE_DISPLAY_ITEM_TYPE
#undef DECLARE_DISPLAY_ITEM_TYPE_FLAGS
  };

  return flags[aType];
}

static Type GetDisplayItemTypeFromKey(uint32_t aDisplayItemKey)
{
  static const uint32_t typeMask = (1 << TYPE_BITS) - 1;
  Type type = static_cast<Type>(aDisplayItemKey & typeMask);
  NS_ASSERTION(type > TYPE_ZERO && type < TYPE_MAX, "Invalid display item type!");
  return type;
}
