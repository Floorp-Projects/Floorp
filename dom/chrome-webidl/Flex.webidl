/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * These objects support visualization of flex containers by the
 * dev tools.
 */

/**
 * A flex container's main and cross axes are either horizontal or
 * vertical, each with two possible directions.
 */
enum FlexPhysicalDirection {
  "horizontal-lr",
  "horizontal-rl",
  "vertical-tb",
  "vertical-bt",
};

[ChromeOnly, Exposed=Window]
interface Flex
{
  sequence<FlexLineValues> getLines();

  /**
   * The physical direction in which successive flex items are placed,
   * within a flex line in this flex container.
   */
  readonly attribute FlexPhysicalDirection mainAxisDirection;

  /**
   * The physical direction in which successive flex lines are placed
   * in this flex container (if it is or were multi-line).
   */
  readonly attribute FlexPhysicalDirection crossAxisDirection;
};

/**
 * This indicates which flex factor (flex-grow vs. flex-shrink) the
 * flex layout algorithm uses internally when resolving flexible sizes
 * in a given flex line, per flexbox spec section 9.7 step 1. Note that
 * this value doesn't necessarily mean that any items on this line
 * are *actually* growing (or shrinking).  This simply indicates what
 * the layout algorithm "wants" to do, based on the free space --
 * and items will stretch from their flex base size in the corresponding
 * direction, if permitted by their min/max constraints and their
 * corresponding flex factor.
 */
enum FlexLineGrowthState { "shrinking", "growing" };

[ChromeOnly, Exposed=Window]
interface FlexLineValues
{
  readonly attribute FlexLineGrowthState growthState;
  readonly attribute double crossStart;
  readonly attribute double crossSize;

  // firstBaselineOffset measures from flex-start edge.
  readonly attribute double firstBaselineOffset;

  // lastBaselineOffset measures from flex-end edge.
  readonly attribute double lastBaselineOffset;

  /**
   * getItems() returns FlexItemValues only for the Elements in
   * this Flex container -- ignoring struts and abs-pos Elements.
   */
  sequence<FlexItemValues> getItems();
};

/**
 * Item main sizes have either been unclamped, clamped to the minimum,
 * or clamped to the maximum.
 */
enum FlexItemClampState {
  "unclamped", "clamped_to_min", "clamped_to_max"
};

[ChromeOnly, Exposed=Window]
interface FlexItemValues
{
  readonly attribute Node? node;
  readonly attribute DOMRectReadOnly frameRect;
  readonly attribute double mainBaseSize;
  readonly attribute double mainDeltaSize;
  readonly attribute double mainMinSize;
  readonly attribute double mainMaxSize;
  readonly attribute double crossMinSize;
  readonly attribute double crossMaxSize;
  readonly attribute FlexItemClampState clampState;
};
