/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { Ci, Cu } = require("chrome");

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Subprocess.jsm");

const Runtime = require("sdk/system/runtime");
const Environment = require("sdk/system/environment").env;
const DEFAULT_ENVIRONMENT = [];
if (Runtime.OS == "Linux" && "DISPLAY" in Environment) {
  DEFAULT_ENVIRONMENT.push("DISPLAY=" + Environment.DISPLAY);
}

function awaitPromise(promise) {
  let value;
  let resolved = null;
  promise.then(val => {
    resolved = true;
    value = val;
  }, val => {
    resolved = false;
    value = val;
  });

  Services.tm.spinEventLoopUntil(() => resolved !== null);

  if (resolved === true)
    return value;
  throw value;
}

let readAllData = async function(pipe, read, callback) {
  let string;
  while (string = await read(pipe))
    callback(string);
};

let write = (pipe, data) => {
  let buffer = new Uint8Array(Array.from(data, c => c.charCodeAt(0)));
  return pipe.write(data);
};

var subprocess = {
  call: function(options) {
    var result;

    let procPromise = (async function() {
      let opts = {};

      if (options.mergeStderr) {
        opts.stderr = "stdout"
      } else if (options.stderr) {
        opts.stderr = "pipe";
      }

      if (options.command instanceof Ci.nsIFile) {
        opts.command = options.command.path;
      } else {
        opts.command = await Subprocess.pathSearch(options.command);
      }

      if (options.workdir) {
        opts.workdir = options.workdir;
      }

      opts.arguments = options.arguments || [];


      // Set up environment

      let envVars = options.environment || DEFAULT_ENVIRONMENT;
      if (envVars.length) {
        let environment = {};
        for (let val of envVars) {
          let idx = val.indexOf("=");
          if (idx >= 0)
            environment[val.slice(0, idx)] = val.slice(idx + 1);
        }

        opts.environment = environment;
      }


      let proc = await Subprocess.call(opts);

      Object.defineProperty(result, "pid", {
        value: proc.pid,
        enumerable: true,
        configurable: true,
      });


      let promises = [];

      // Set up IO handlers.

      let read = pipe => pipe.readString();
      if (options.charset === null) {
        read = pipe => {
          return pipe.read().then(buffer => {
            return String.fromCharCode(...buffer);
          });
        };
      }

      if (options.stdout)
        promises.push(readAllData(proc.stdout, read, options.stdout));

      if (options.stderr && proc.stderr)
        promises.push(readAllData(proc.stderr, read, options.stderr));

      // Process stdin

      if (typeof options.stdin === "string") {
        write(proc.stdin, options.stdin);
        proc.stdin.close();
      }


      // Handle process completion

      if (options.done)
        Promise.all(promises)
          .then(() => proc.wait())
          .then(options.done);

      return proc;
    })();

    procPromise.catch(e => {
      if (options.done)
        options.done({exitCode: -1}, e);
      else
        Cu.reportError(e instanceof Error ? e : e.message || e);
    });

    if (typeof options.stdin === "function") {
      // Unfortunately, some callers (child_process.js) depend on this
      // being called synchronously.
      options.stdin({
        write(val) {
          procPromise.then(proc => {
            write(proc.stdin, val);
          });
        },

        close() {
          procPromise.then(proc => {
            proc.stdin.close();
          });
        },
      });
    }

    result = {
      get pid() {
        return awaitPromise(procPromise.then(proc => {
          return proc.pid;
        }));
      },

      wait() {
        return awaitPromise(procPromise.then(proc => {
          return proc.wait().then(({exitCode}) => exitCode);
        }));
      },

      kill(hard = false) {
        procPromise.then(proc => {
          proc.kill(hard ? 0 : undefined);
        });
      },
    };

    return result;
  },
};

module.exports = subprocess;
