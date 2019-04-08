/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getPopupObjectProperties } from "../../selectors";
import type { ThunkArgs } from "../types";
import type { ThreadContext } from "../../types";

/**
 * @memberof actions/pause
 * @static
 */
export function setPopupObjectProperties(
  cx: ThreadContext,
  object: any,
  properties: Object
) {
  return ({ dispatch, client, getState }: ThunkArgs) => {
    const objectId = object.actor || object.objectId;

    if (getPopupObjectProperties(getState(), cx.thread, object.actor)) {
      return;
    }

    dispatch({
      type: "SET_POPUP_OBJECT_PROPERTIES",
      cx,
      thread: cx.thread,
      objectId,
      properties
    });
  };
}
