/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

export type Message = {
  data: {
    id: string,
    method: string,
    args: Array<any>,
  },
};

let msgId = 1;
/**
 * @memberof utils/utils
 * @static
 */
function workerTask(worker: any, method: string): Function {
  return function(...args: any): Promise<any> {
    return new Promise((resolve, reject) => {
      const id = msgId++;
      worker.postMessage({ id, method, args });

      const listener = ({ data: result }) => {
        if (result.id !== id) {
          return;
        }

        worker.removeEventListener("message", listener);
        if (result.error) {
          reject(result.error);
        } else {
          resolve(result.response);
        }
      };

      worker.addEventListener("message", listener);
    });
  };
}

function workerHandler(publicInterface: any): Function {
  return function onTask(msg: Message) {
    const { id, method, args } = msg.data;
    const response = publicInterface[method].apply(null, args);

    if (response instanceof Promise) {
      response
        .then(val => self.postMessage({ id, response: val }))
        .catch(error => self.postMessage({ id, error }));
    } else {
      self.postMessage({ id, response });
    }
  };
}

export { workerTask, workerHandler };
