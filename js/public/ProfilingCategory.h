/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_ProfilingCategory_h
#define js_ProfilingCategory_h

#include "jstypes.h"  // JS_FRIEND_API

// clang-format off

// This higher-order macro lists all categories with their subcategories.
//
// PROFILING_CATEGORY_LIST(BEGIN_CATEGORY, SUBCATEGORY, END_CATEGORY)
//   BEGIN_CATEGORY(name, labelAsString, colorAsString)
//   SUBCATEGORY(category, name, labelAsString)
//   END_CATEGORY
//
// The list of available color names for categories is:
// transparent, grey, purple, yellow, orange, lightblue, green, blue, magenta
//
// Categories and subcategories are used for stack-based instrumentation. They
// are specified in label frames in the profiling stack, see ProfilingStack.h.
// At any point, the category pair of the topmost profiler label frame in the
// label stack determines the category pair of that stack.
// Each category describes a type of workload that the CPU can be busy with.
// Categories should be non-overlapping: the list of categories should be
// chosen in such a way that every possible stack can be mapped to a single
// category unambiguously.

#define PROFILING_CATEGORY_LIST(BEGIN_CATEGORY, SUBCATEGORY, END_CATEGORY)    \
  BEGIN_CATEGORY(IDLE, "Idle", "transparent")                                 \
    SUBCATEGORY(IDLE, IDLE, "Other")                                          \
  END_CATEGORY                                                                \
  BEGIN_CATEGORY(OTHER, "Other", "grey")                                      \
    SUBCATEGORY(OTHER, OTHER, "Other")                                        \
  END_CATEGORY                                                                \
  BEGIN_CATEGORY(LAYOUT, "Layout", "purple")                                  \
    SUBCATEGORY(LAYOUT, LAYOUT, "Other")                                      \
    SUBCATEGORY(LAYOUT, LAYOUT_FrameConstruction, "Frame construction")       \
    SUBCATEGORY(LAYOUT, LAYOUT_Reflow, "Reflow")                              \
    SUBCATEGORY(LAYOUT, LAYOUT_CSSParsing, "CSS parsing")                     \
    SUBCATEGORY(LAYOUT, LAYOUT_SelectorQuery, "Selector query")               \
    SUBCATEGORY(LAYOUT, LAYOUT_StyleComputation, "Style computation")         \
  END_CATEGORY                                                                \
  BEGIN_CATEGORY(JS, "JavaScript", "yellow")                                  \
    SUBCATEGORY(JS, JS, "Other")                                              \
    SUBCATEGORY(JS, JS_Parsing, "JS Parsing")                                 \
    SUBCATEGORY(JS, JS_IonCompilation, "Ion JIT Compilation")                 \
    SUBCATEGORY(JS, JS_BaselineCompilation, "Baseline JIT Compilation")       \
  END_CATEGORY                                                                \
  BEGIN_CATEGORY(GCCC, "GC / CC", "orange")                                   \
    SUBCATEGORY(GCCC, GCCC, "Other")                                          \
  END_CATEGORY                                                                \
  BEGIN_CATEGORY(NETWORK, "Network", "lightblue")                             \
    SUBCATEGORY(NETWORK, NETWORK, "Other")                                    \
  END_CATEGORY                                                                \
  BEGIN_CATEGORY(GRAPHICS, "Graphics", "green")                               \
    SUBCATEGORY(GRAPHICS, GRAPHICS, "Other")                                  \
    SUBCATEGORY(GRAPHICS, GRAPHICS_DisplayListBuilding, "DisplayList building") \
    SUBCATEGORY(GRAPHICS, GRAPHICS_DisplayListMerging, "DisplayList merging") \
    SUBCATEGORY(GRAPHICS, GRAPHICS_LayerBuilding, "Layer building")           \
    SUBCATEGORY(GRAPHICS, GRAPHICS_TileAllocation, "Tile allocation")         \
    SUBCATEGORY(GRAPHICS, GRAPHICS_WRDisplayList, "WebRender display list")   \
    SUBCATEGORY(GRAPHICS, GRAPHICS_Rasterization, "Rasterization")            \
    SUBCATEGORY(GRAPHICS, GRAPHICS_FlushingAsyncPaints, "Flushing async paints") \
    SUBCATEGORY(GRAPHICS, GRAPHICS_ImageDecoding, "Image decoding")           \
  END_CATEGORY                                                                \
  BEGIN_CATEGORY(DOM, "DOM", "blue")                                          \
    SUBCATEGORY(DOM, DOM, "Other")                                            \
  END_CATEGORY

namespace JS {

// An enum that lists all possible category pairs in one list.
// This is the enum that is used in profiler stack labels. Having one list that
// includes subcategories from all categories in one list allows assigning the
// category pair to a stack label with just one number.
#define CATEGORY_ENUM_BEGIN_CATEGORY(name, labelAsString, color)
#define CATEGORY_ENUM_SUBCATEGORY(supercategory, name, labelAsString) name,
#define CATEGORY_ENUM_END_CATEGORY
enum class ProfilingCategoryPair : uint32_t {
  PROFILING_CATEGORY_LIST(CATEGORY_ENUM_BEGIN_CATEGORY,
                          CATEGORY_ENUM_SUBCATEGORY,
                          CATEGORY_ENUM_END_CATEGORY)
  COUNT,
  LAST = COUNT - 1,
};
#undef CATEGORY_ENUM_BEGIN_CATEGORY
#undef CATEGORY_ENUM_SUBCATEGORY
#undef CATEGORY_ENUM_END_CATEGORY

// An enum that lists just the categories without their subcategories.
#define SUPERCATEGORY_ENUM_BEGIN_CATEGORY(name, labelAsString, color) name,
#define SUPERCATEGORY_ENUM_SUBCATEGORY(supercategory, name, labelAsString)
#define SUPERCATEGORY_ENUM_END_CATEGORY
enum class ProfilingCategory : uint32_t {
  PROFILING_CATEGORY_LIST(SUPERCATEGORY_ENUM_BEGIN_CATEGORY,
                          SUPERCATEGORY_ENUM_SUBCATEGORY,
                          SUPERCATEGORY_ENUM_END_CATEGORY)
  COUNT,
  LAST = COUNT - 1,
};
#undef SUPERCATEGORY_ENUM_BEGIN_CATEGORY
#undef SUPERCATEGORY_ENUM_SUBCATEGORY
#undef SUPERCATEGORY_ENUM_END_CATEGORY

// clang-format on

struct ProfilingCategoryPairInfo {
  ProfilingCategory mCategory;
  uint32_t mSubcategoryIndex;
  const char* mLabel;
};

JS_FRIEND_API const ProfilingCategoryPairInfo& GetProfilingCategoryPairInfo(
    ProfilingCategoryPair aCategoryPair);

}  // namespace JS

#endif /* js_ProfilingCategory_h */
