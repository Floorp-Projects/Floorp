/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { reportException } = require("devtools/shared/DevToolsUtils");

function asPaused(client, func) {
  if (client.state != "paused") {
    return Task.spawn(function* () {
      yield client.interrupt();
      let result;

      try {
        result = yield func();
      }
      catch (e) {
        // Try to put the debugger back in a working state by resuming
        // it
        yield client.resume();
        throw e;
      }

      yield client.resume();
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
  return path.reduce(function (acc, name) {
    return acc[name];
  }, destObj);
}

function mergeIn(destObj, path, value) {
  path = [...path];
  path.reverse();
  var obj = path.reduce(function (acc, name) {
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
  asPaused,
  handleError,
  onReducerEvents,
  mergeIn,
  setIn,
  updateIn,
  deleteIn
};
