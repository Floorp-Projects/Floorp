/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { ThunkArgs } from "../actions/types";
import { asSettled, type AsyncValue } from "./async-value";

export type MemoizedAction<
  Args,
  Result
> = Args => ThunkArgs => Promise<Result | null>;
type MemoizableActionParams<Args, Result> = {
  getValue: (args: Args, thunkArgs: ThunkArgs) => AsyncValue<Result> | null,
  createKey: (args: Args, thunkArgs: ThunkArgs) => string,
  action: (args: Args, thunkArgs: ThunkArgs) => Promise<mixed>,
};

/*
 * memoizableActon is a utility for actions that should only be performed
 * once per key. It is useful for loading sources, parsing symbols ...
 *
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
  { getValue, createKey, action }: MemoizableActionParams<Args, Result>
): MemoizedAction<Args, Result> {
  const requests = new Map();
  return args => async thunkArgs => {
    let result = asSettled(getValue(args, thunkArgs));
    if (!result) {
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

      result = asSettled(getValue(args, thunkArgs));

      if (!result) {
        // Returning null here is not ideal. This means that the action
        // resolved but 'getValue' didn't return a loaded value, for instance
        // if the data the action was meant to store was deleted. In a perfect
        // world we'd throw a ContextError here or handle cancellation somehow.
        // Throwing will also allow us to change the return type on the action
        // to always return a promise for the getValue AsyncValue type, but
        // for now we have to add an additional '| null' for this case.
        return null;
      }
    }

    if (result.state === "rejected") {
      throw result.value;
    }
    return result.value;
  };
}
