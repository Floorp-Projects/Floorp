/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A global table that tracks images referenced by CSS variables. */

#ifndef mozilla_CSSVariableImageTable_h
#define mozilla_CSSVariableImageTable_h

#include "nsClassHashtable.h"
#include "nsCSSPropertyID.h"
#include "nsCSSValue.h"
#include "nsStyleContext.h"
#include "nsTArray.h"

/**
 * CSSVariableImageTable maintains a global mapping
 *   (nsStyleContext, nsCSSPropertyID) -> nsTArray<ImageValue>
 * which allows us to track the relationship between CSS property values
 * involving variables and any images they may reference.
 *
 * When properties like background-image contain a normal url(), the
 * Declaration's data block will hold a reference to the ImageValue.  When a
 * token stream is used, the Declaration only holds on to an
 * nsCSSValueTokenStream object, and the ImageValue would only exist for the
 * duration of nsRuleNode::WalkRuleTree, in the AutoCSSValueArray.  So instead
 * when we re-parse a token stream and get an ImageValue, we record it in the
 * CSSVariableImageTable to keep the ImageValue alive. Such ImageValues are
 * eventually freed the next time the token stream is re-parsed, or when the
 * associated style context is destroyed.
 *
 * To add ImageValues to the CSSVariableImageTable, callers should pass a lambda
 * to CSSVariableImageTable::ReplaceAll() that calls
 * CSSVariableImageTable::Add() for each ImageValue that needs to be added to
 * the table. When callers are sure that the ImageValues for a given
 * nsStyleContext won't be needed anymore, they can call
 * CSSVariableImageTable::RemoveAll() to release them.
 */

namespace mozilla {
namespace CSSVariableImageTable {

namespace detail {

typedef nsTArray<RefPtr<css::ImageValue>> ImageValueArray;
typedef nsClassHashtable<nsGenericHashKey<nsCSSPropertyID>, ImageValueArray>
        PerPropertyImageHashtable;
typedef nsClassHashtable<nsPtrHashKey<nsStyleContext>, PerPropertyImageHashtable>
        CSSVariableImageHashtable;

inline CSSVariableImageHashtable& GetTable()
{
  static CSSVariableImageHashtable imageTable;
  return imageTable;
}

#ifdef DEBUG
inline bool& IsReplacing()
{
  static bool isReplacing = false;
  return isReplacing;
}
#endif

} // namespace detail

/**
 * ReplaceAll() allows callers to replace the ImageValues associated with a
 * (nsStyleContext, nsCSSPropertyID) pair. The memory used by the previous list of
 * ImageValues is automatically released.
 *
 * @param aContext The style context the ImageValues are associated with.
 * @param aProp    The CSS property the ImageValues are associated with.
 * @param aFunc    A lambda that calls CSSVariableImageTable::Add() to add new
 *                 ImageValues which will replace the old ones.
 */
template <typename Lambda>
inline void ReplaceAll(nsStyleContext* aContext,
                       nsCSSPropertyID aProp,
                       Lambda aFunc)
{
  MOZ_ASSERT(aContext);

  auto& imageTable = detail::GetTable();

  // Clear the existing image array, if any, for this property.
  {
    auto* perPropertyImageTable = imageTable.Get(aContext);
    auto* imageList = perPropertyImageTable ? perPropertyImageTable->Get(aProp)
                                            : nullptr;
    if (imageList) {
      imageList->ClearAndRetainStorage();
    }
  }

#ifdef DEBUG
  MOZ_ASSERT(!detail::IsReplacing());
  detail::IsReplacing() = true;
#endif

  aFunc();

#ifdef DEBUG
  detail::IsReplacing() = false;
#endif

  // Clean up.
  auto* perPropertyImageTable = imageTable.Get(aContext);
  auto* imageList = perPropertyImageTable ? perPropertyImageTable->Get(aProp)
                                          : nullptr;
  if (imageList) {
    if (imageList->IsEmpty()) {
      // We used to have an image array for this property, but now we don't.
      // Remove the entry in the per-property image table for this property.
      // That may then allow us to remove the entire per-property image table.
      perPropertyImageTable->Remove(aProp);
      if (perPropertyImageTable->Count() == 0) {
        imageTable.Remove(aContext);
      }
    } else {
      // We still have a non-empty image array for this property. Compact the
      // storage it's using if possible.
      imageList->Compact();
    }
  }
}

/**
 * Adds a new ImageValue @aValue to the CSSVariableImageTable, which will be
 * associated with @aContext and @aProp.
 *
 * It's illegal to call this function outside of a lambda passed to
 * CSSVariableImageTable::ReplaceAll().
 */
inline void
Add(nsStyleContext* aContext, nsCSSPropertyID aProp, css::ImageValue* aValue)
{
  MOZ_ASSERT(aValue);
  MOZ_ASSERT(aContext);
  MOZ_ASSERT(detail::IsReplacing());

  auto& imageTable = detail::GetTable();

  // Ensure there's a per-property image table for this style context.
  auto* perPropertyImageTable = imageTable.Get(aContext);
  if (!perPropertyImageTable) {
    perPropertyImageTable = new detail::PerPropertyImageHashtable();
    imageTable.Put(aContext, perPropertyImageTable);
  }

  // Ensure there's an image array for this property.
  auto* imageList = perPropertyImageTable->Get(aProp);
  if (!imageList) {
    imageList = new detail::ImageValueArray();
    perPropertyImageTable->Put(aProp, imageList);
  }

  // Append the provided ImageValue to the list.
  imageList->AppendElement(aValue);
}

/**
 * Removes all ImageValues stored in the CSSVariableImageTable for the provided
 * @aContext.
 */
inline void
RemoveAll(nsStyleContext* aContext)
{
  // Move all ImageValue references into removedImageList so that we can
  // release them outside of any hashtable methods.  (If we just call
  // Remove(aContext) on the table then we can end up calling back
  // re-entrantly into hashtable methods, as other style contexts
  // are released.)
  detail::ImageValueArray removedImages;
  auto& imageTable = detail::GetTable();
  auto* perPropertyImageTable = imageTable.Get(aContext);
  if (perPropertyImageTable) {
    for (auto it = perPropertyImageTable->Iter(); !it.Done(); it.Next()) {
      auto* imageList = it.UserData();
      removedImages.AppendElements(Move(*imageList));
    }
  }
  imageTable.Remove(aContext);
}

} // namespace CSSVariableImageTable
} // namespace mozilla

#endif // mozilla_CSSVariableImageTable_h
