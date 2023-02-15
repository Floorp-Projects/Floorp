/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useEffect, useGlobals, addons } from "@storybook/addons";
import {
  DIRECTIONS,
  DIRECTION_BY_STRATEGY,
  UPDATE_STRATEGY_EVENT,
} from "./constants.mjs";

/**
 * withPseudoLocalization is a Storybook decorator that handles emitting an
 * event to update translations when a new pseudo localization strategy is
 * applied. It also handles setting a "dir" attribute on the root element in the
 * Storybook iframe.
 *
 * @param {Function} StoryFn - Provided by Storybook, used to render the story.
 * @param {Object} context - Provided by Storybook, data about the story.
 * @returns {Function} StoryFn with a modified "dir" attr set.
 */
export const withPseudoLocalization = (StoryFn, context) => {
  const [{ pseudoStrategy }] = useGlobals();
  const direction = DIRECTION_BY_STRATEGY[pseudoStrategy] || DIRECTIONS.ltr;
  const isInDocs = context.viewMode === "docs";
  const channel = addons.getChannel();

  useEffect(() => {
    if (pseudoStrategy) {
      channel.emit(UPDATE_STRATEGY_EVENT, pseudoStrategy);
    }
  }, [pseudoStrategy]);

  useEffect(() => {
    if (isInDocs) {
      document.documentElement.setAttribute("dir", DIRECTIONS.ltr);
      let storyElements = document.querySelectorAll(".docs-story");
      storyElements.forEach(element => element.setAttribute("dir", direction));
    } else {
      document.documentElement.setAttribute("dir", direction);
    }
  }, [direction, isInDocs]);

  return StoryFn();
};
