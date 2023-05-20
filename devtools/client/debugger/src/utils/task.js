/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This object provides the public module functions.
 */
export const Task = {
  // XXX: Not sure if this works in all cases...
  async: function (task) {
    return function () {
      return Task.spawn(task, this, arguments);
    };
  },

  /**
   * Creates and starts a new task.
   * @param task A generator function
   * @return A promise, resolved when the task terminates
   */
  spawn: function (task, scope, args) {
    return new Promise(function (resolve, reject) {
      const iterator = task.apply(scope, args);

      const callNext = lastValue => {
        const iteration = iterator.next(lastValue);
        Promise.resolve(iteration.value)
          .then(value => {
            if (iteration.done) {
              resolve(value);
            } else {
              callNext(value);
            }
          })
          .catch(error => {
            reject(error);
            iterator.throw(error);
          });
      };

      callNext(undefined);
    });
  },
};
