/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getMainThread } from "../selectors/index";

export function setExpandedState(expanded) {
  return { type: "SET_EXPANDED_STATE", expanded };
}

export function focusItem(item) {
  return { type: "SET_FOCUSED_SOURCE_ITEM", item };
}

export function setProjectDirectoryRoot(newRootItemUniquePath, newName) {
  return ({ dispatch, getState }) => {
    // If the new project root is against the top level thread,
    // replace its thread ID with "top-level", so that later,
    // getDirectoryForUniquePath could match the project root,
    // even after a page reload where the new top level thread actor ID
    // will be different.
    const mainThread = getMainThread(getState());
    if (mainThread && newRootItemUniquePath.startsWith(mainThread.actor)) {
      newRootItemUniquePath = newRootItemUniquePath.replace(
        mainThread.actor,
        "top-level"
      );
    }
    dispatch({
      type: "SET_PROJECT_DIRECTORY_ROOT",
      uniquePath: newRootItemUniquePath,
      name: newName,
    });
  };
}

export function clearProjectDirectoryRoot() {
  return {
    type: "SET_PROJECT_DIRECTORY_ROOT",
    uniquePath: "",
    name: "",
  };
}
