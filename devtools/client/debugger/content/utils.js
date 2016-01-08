/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { promiseInvoke } = require("devtools/shared/async-utils");
const { reportException } = require("devtools/shared/DevToolsUtils");

// RDP utils

function rdpInvoke(client, method, ...args) {
  return (promiseInvoke(client, method, ...args)
    .then((packet) => {
      if (packet.error) {
        let { error, message } = packet;
        const err = new Error(error + ": " + message);
        err.rdpError = error;
        err.rdpMessage = message;
        throw err;
      }

      return packet;
    }));
}

function asPaused(client, func) {
  if (client.state != "paused") {
    return Task.spawn(function*() {
      yield rdpInvoke(client, client.interrupt);
      let result;

      try {
        result = yield func();
      }
      catch(e) {
        // Try to put the debugger back in a working state by resuming
        // it
        yield rdpInvoke(client, client.resume);
        throw e;
      }

      yield rdpInvoke(client, client.resume);
      return result;
    });
  } else {
    return func();
  }
}

function handleError(err) {
  reportException("promise", err.toString());
}

function onReducerEvents(controller, listeners, thisContext) {
  Object.keys(listeners).forEach(name => {
    const listener = listeners[name];
    controller.onChange(name, payload => {
      listener.call(thisContext, payload);
    });
  });
}

function _getIn(destObj, path) {
  return path.reduce(function(acc, name) {
    return acc[name];
  }, destObj);
}

function mergeIn(destObj, path, value) {
  path = [...path];
  path.reverse();
  var obj = path.reduce(function(acc, name) {
    return { [name]: acc };
  }, value);

  return destObj.merge(obj, { deep: true });
}

function setIn(destObj, path, value) {
  destObj = mergeIn(destObj, path, null);
  return mergeIn(destObj, path, value);
}

function updateIn(destObj, path, fn) {
  return setIn(destObj, path, fn(_getIn(destObj, path)));
}

function deleteIn(destObj, path) {
  const objPath = path.slice(0, -1);
  const propName = path[path.length - 1];
  const obj = _getIn(destObj, objPath);
  return setIn(destObj, objPath, obj.without(propName));
}

module.exports = {
  rdpInvoke,
  asPaused,
  handleError,
  onReducerEvents,
  mergeIn,
  setIn,
  updateIn,
  deleteIn
};
