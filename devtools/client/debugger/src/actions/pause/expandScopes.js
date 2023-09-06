/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getScopeItemPath } from "../../utils/pause/scopes";

export function setExpandedScope(selectedFrame, item, expanded) {
  return function ({ dispatch, getState }) {
    return dispatch({
      type: "SET_EXPANDED_SCOPE",
      selectedFrame,
      path: getScopeItemPath(item),
      expanded,
    });
  };
}
