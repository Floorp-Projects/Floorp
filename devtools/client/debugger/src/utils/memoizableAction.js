/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ThunkArgs } from "../actions/types";

export type MemoizedAction<Args, Result> = Args => ThunkArgs => Promise<Result>;
type MemoizableActionParams<Args, Result> = {
  hasValue: (args: Args, thunkArgs: ThunkArgs) => boolean,
  getValue: (args: Args, thunkArgs: ThunkArgs) => Result,
  createKey: (args: Args, thunkArgs: ThunkArgs) => string,
  action: (args: Args, thunkArgs: ThunkArgs) => Promise<Result>,
};

/*
 * memoizableActon is a utility for actions that should only be performed
 * once per key. It is useful for loading sources, parsing symbols ...
 *
 * @hasValue - checks to see if the result is in the redux store
 * @getValue - gets the result from the redux store
 * @createKey - creates a key for the requests map
 * @action - kicks off the async work for the action
 *
 *
 * For Example
 *
 * export const setItem = memoizeableAction(
 *   "setItem",
 *   {
 *     hasValue: ({ a }, { getState }) => hasItem(getState(), a),
 *     getValue: ({ a }, { getState }) => getItem(getState(), a),
 *     createKey: ({ a }) => a,
 *     action: ({ a }, thunkArgs) => doSetItem(a, thunkArgs)
 *   }
 * );
 *
 */
export function memoizeableAction<Args, Result>(
  name: string,
  {
    hasValue,
    getValue,
    createKey,
    action,
  }: MemoizableActionParams<Args, Result>
): MemoizedAction<Args, Result> {
  const requests = new Map();
  return args => async (thunkArgs: ThunkArgs) => {
    if (hasValue(args, thunkArgs)) {
      return getValue(args, thunkArgs);
    }

    const key = createKey(args, thunkArgs);
    if (!requests.has(key)) {
      requests.set(
        key,
        (async () => {
          try {
            await action(args, thunkArgs);
          } catch (e) {
            console.warn(`Action ${name} had an exception:`, e);
          } finally {
            requests.delete(key);
          }
        })()
      );
    }

    await requests.get(key);
    return getValue(args, thunkArgs);
  };
}
