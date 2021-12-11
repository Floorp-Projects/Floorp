/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { prefs } from "../../utils/prefs";
import update, { initialUIState } from "../ui";

describe("ui reducer", () => {
  it("toggle framework grouping to false", () => {
    const state = initialUIState();
    const value = false;
    const updatedState = update(state, {
      type: "TOGGLE_FRAMEWORK_GROUPING",
      value,
    });
    expect(updatedState.frameworkGroupingOn).toBe(value);
    expect(prefs.frameworkGroupingOn).toBe(value);
  });

  it("toggle framework grouping to true", () => {
    const state = initialUIState();
    const value = true;
    const updatedState = update(state, {
      type: "TOGGLE_FRAMEWORK_GROUPING",
      value,
    });
    expect(updatedState.frameworkGroupingOn).toBe(value);
    expect(prefs.frameworkGroupingOn).toBe(value);
  });
});
