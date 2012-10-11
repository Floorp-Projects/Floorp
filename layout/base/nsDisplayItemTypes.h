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
#include "nsDisplayItemTypesList.h"
#undef DECLARE_DISPLAY_ITEM_TYPE

  TYPE_MAX
};

enum {
  // Number of bits needed to represent all types
  TYPE_BITS = 8
};

static const char* DisplayItemTypeName(Type aType)
{
  switch (aType) {
#define DECLARE_DISPLAY_ITEM_TYPE(name) case TYPE_##name: return #name;
#include "nsDisplayItemTypesList.h"
#undef DECLARE_DISPLAY_ITEM_TYPE

    default: return "TYPE_UNKNOWN";
  }
}
