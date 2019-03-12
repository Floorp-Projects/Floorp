/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getPopupObjectProperties, getCurrentThread } from "../../selectors";
import type { ThunkArgs } from "../types";

/**
 * @memberof actions/pause
 * @static
 */
export function setPopupObjectProperties(object: any, properties: Object) {
  return ({ dispatch, client, getState }: ThunkArgs) => {
    const objectId = object.actor || object.objectId;
    const thread = getCurrentThread(getState());

    if (getPopupObjectProperties(getState(), thread, object.actor)) {
      return;
    }

    dispatch({
      type: "SET_POPUP_OBJECT_PROPERTIES",
      thread,
      objectId,
      properties
    });
  };
}
