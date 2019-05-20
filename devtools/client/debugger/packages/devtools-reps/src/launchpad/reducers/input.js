/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const constants = require("../constants");
const Immutable = require("immutable");

const initialState = Immutable.Map({
  currentValue: "",
  currentNavigationKey: null,
  history: Immutable.OrderedMap(),
});

function update(state = initialState, action) {
  const { type, value, key } = action;

  const currentValue = state.get("currentValue");
  const currentNavigationKey = state.get("currentNavigationKey");
  const history = state.get("history");

  switch (type) {
    case constants.ADD_INPUT:
      return state.withMutations(map => {
        map
          .set("history", history.set(key, value))
          .set("currentValue", "")
          .set("currentNavigationKey", null);
      });

    case constants.CHANGE_CURRENT_INPUT:
      return state.set("currentValue", value);

    case constants.NAVIGATE_INPUT_HISTORY:
      const keys = history.reverse().keySeq();
      const navigationIndex = keys.indexOf(currentNavigationKey);
      const dir = value;

      const newNavigationIndex =
        dir === constants.DIR_BACKWARD
          ? navigationIndex + 1
          : navigationIndex - 1;

      const newNavigationKey =
        newNavigationIndex >= 0 ? keys.get(newNavigationIndex) : null;

      const fallbackValue = dir === constants.DIR_BACKWARD ? currentValue : "";
      const fallbackNavigationKey =
        dir === constants.DIR_BACKWARD ? currentNavigationKey : -1;

      return state.withMutations(map => {
        map
          .set("currentValue", history.get(newNavigationKey) || fallbackValue)
          .set(
            "currentNavigationKey",
            newNavigationKey || fallbackNavigationKey
          );
      });
  }

  return state;
}

module.exports = update;
