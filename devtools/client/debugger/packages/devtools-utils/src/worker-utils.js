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

function WorkerDispatcher() {
  this.msgId = 1;
  this.worker = null;
}

WorkerDispatcher.prototype = {
  start(url: string, win = window) {
    this.worker = new win.Worker(url);
    this.worker.onerror = () => {
      console.error(`Error in worker ${url}`);
    };
  },

  stop() {
    if (!this.worker) {
      return;
    }

    this.worker.terminate();
    this.worker = null;
  },

  task(
    method: string,
    { queue = false } = {}
  ): (...args: any[]) => Promise<any> {
    const calls = [];
    const push = (args: Array<any>) => {
      return new Promise((resolve, reject) => {
        if (queue && calls.length === 0) {
          Promise.resolve().then(flush);
        }

        calls.push([args, resolve, reject]);

        if (!queue) {
          flush();
        }
      });
    };

    const flush = () => {
      const items = calls.slice();
      calls.length = 0;

      if (!this.worker) {
        return;
      }

      const id = this.msgId++;
      this.worker.postMessage({
        id,
        method,
        calls: items.map(item => item[0]),
      });

      const listener = ({ data: result }) => {
        if (result.id !== id) {
          return;
        }

        if (!this.worker) {
          return;
        }

        this.worker.removeEventListener("message", listener);

        result.results.forEach((resultData, i) => {
          const [, resolve, reject] = items[i];

          if (resultData.error) {
            const err = new Error(resultData.message);
            (err: any).metadata = resultData.metadata;
            reject(err);
          } else {
            resolve(resultData.response);
          }
        });
      };

      this.worker.addEventListener("message", listener);
    };

    return (...args: any) => push(args);
  },

  invoke(method: string, ...args: any[]): Promise<any> {
    return this.task(method)(...args);
  },
};

function workerHandler(publicInterface: Object) {
  return function(msg: Message) {
    const { id, method, calls } = (msg.data: any);

    Promise.all(
      calls.map(args => {
        try {
          const response = publicInterface[method].apply(undefined, args);
          if (response instanceof Promise) {
            return response.then(
              val => ({ response: val }),
              err => asErrorMessage(err)
            );
          }
          return { response };
        } catch (error) {
          return asErrorMessage(error);
        }
      })
    ).then(results => {
      self.postMessage({ id, results });
    });
  };
}

function asErrorMessage(error) {
  if (typeof error === "object" && error && "message" in error) {
    // Error can't be sent via postMessage, so be sure to convert to
    // string.
    return {
      error: true,
      message: error.message,
      metadata: error.metadata,
    };
  }

  return {
    error: true,
    message: error == null ? error : error.toString(),
    metadata: undefined,
  };
}

module.exports = {
  WorkerDispatcher,
  workerHandler,
};
