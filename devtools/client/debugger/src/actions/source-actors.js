/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ThunkArgs } from "./types";
import type { SourceActor } from "../reducers/source-actors";

export function insertSourceActor(item: SourceActor) {
  return insertSourceActors([item]);
}
export function insertSourceActors(items: Array<SourceActor>) {
  return function({ dispatch }: ThunkArgs) {
    dispatch({
      type: "INSERT_SOURCE_ACTORS",
      items,
    });
  };
}

export function removeSourceActor(item: SourceActor) {
  return removeSourceActors([item]);
}
export function removeSourceActors(items: Array<SourceActor>) {
  return function({ dispatch }: ThunkArgs) {
    dispatch({
      type: "REMOVE_SOURCE_ACTORS",
      items,
    });
  };
}
