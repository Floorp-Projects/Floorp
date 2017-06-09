/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Redux middleware for debouncing actions.
 *
 * Schedules actions with { meta: { debounce: true } } to be delayed
 * by wait milliseconds. If another action is fired during this
 * time-frame both actions are inserted into a queue and delayed.
 * Maximum delay is defined by maxWait argument.
 *
 * Handling more actions at once results in better performance since
 * components need to be re-rendered less often.
 *
 * @param string wait Wait for specified amount of milliseconds
 *                    before executing an action. The time is used
 *                    to collect more actions and handle them all
 *                    at once.
 * @param string maxWait Max waiting time. It's used in case of
 *                       a long stream of actions.
 */
function debounceActions(wait, maxWait) {
  let queuedActions = [];

  return store => next => {
    let debounced = debounce(() => {
      next(batchActions(queuedActions));
      queuedActions = [];
    }, wait, maxWait);

    return action => {
      if (!action.meta || !action.meta.debounce) {
        return next(action);
      }

      if (action.type == BATCH_ACTIONS) {
        queuedActions.push(...action.actions);
      } else {
        queuedActions.push(action);
      }

      return debounced();
    };
  };
}

function debounce(cb, wait, maxWait) {
  let timeout, maxTimeout;
  let doFunction = () => {
    clearTimeout(timeout);
    clearTimeout(maxTimeout);
    timeout = maxTimeout = null;
    cb();
  };

  return () => {
    clearTimeout(timeout);
    timeout = setTimeout(doFunction, wait);
    if (!maxTimeout) {
      maxTimeout = setTimeout(doFunction, maxWait);
    }
  };
}

const BATCH_ACTIONS = Symbol("BATCH_ACTIONS");

/**
 * Action creator for action-batching.
 */
function batchActions(batchedActions, debounceFlag = true) {
  return {
    type: BATCH_ACTIONS,
    meta: { debounce: debounceFlag },
    actions: batchedActions,
  };
}

module.exports = {
  BATCH_ACTIONS,
  batchActions,
  debounceActions,
};
