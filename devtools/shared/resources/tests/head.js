/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../client/shared/test/shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

const { DevToolsClient } = require("devtools/client/devtools-client");
const { DevToolsServer } = require("devtools/server/devtools-server");

async function _initResourceCommandFromCommands(
  commands,
  { listenForWorkers = false } = {}
) {
  const targetCommand = commands.targetCommand;
  if (listenForWorkers) {
    targetCommand.listenForWorkers = true;
  }
  await targetCommand.startListening();

  //Bug 1709065: Stop exporting resourceCommand and use commands.resourceCommand
  //And rename all these methods
  return {
    client: commands.client,
    commands,
    resourceCommand: commands.resourceCommand,
    targetCommand,
  };
}

/**
 * Instantiate a ResourceCommand for the given tab.
 *
 * @param {Tab} tab
 *        The browser frontend's tab to connect to.
 * @param {Object} options
 * @param {Boolean} options.listenForWorkers
 * @return {Object} object
 * @return {ResourceCommand} object.resourceCommand
 *         The underlying resource command interface.
 * @return {Object} object.commands
 *         The commands object defined by modules from devtools/shared/commands.
 * @return {DevToolsClient} object.client
 *         The underlying client instance.
 * @return {TargetCommand} object.targetCommand
 *         The underlying target list instance.
 */
async function initResourceCommand(tab, options) {
  const commands = await CommandsFactory.forTab(tab);
  return _initResourceCommandFromCommands(commands, options);
}

/**
 * Instantiate a multi-process ResourceCommand, watching all type of targets.
 *
 * @return {Object} object
 * @return {ResourceCommand} object.resourceCommand
 *         The underlying resource command interface.
 * @return {Object} object.commands
 *         The commands object defined by modules from devtools/shared/commands.
 * @return {DevToolsClient} object.client
 *         The underlying client instance.
 * @return {DevToolsClient} object.targetCommand
 *         The underlying target list instance.
 */
async function initMultiProcessResourceCommand() {
  const commands = await CommandsFactory.forMainProcess();
  return _initResourceCommandFromCommands(commands);
}

// Copied from devtools/shared/webconsole/test/chrome/common.js
function checkObject(object, expected) {
  if (object && object.getGrip) {
    object = object.getGrip();
  }

  for (const name of Object.keys(expected)) {
    const expectedValue = expected[name];
    const value = object[name];
    checkValue(name, value, expectedValue);
  }
}

function checkValue(name, value, expected) {
  if (expected === null) {
    is(value, null, `'${name}' is null`);
  } else if (value === undefined) {
    is(value, undefined, `'${name}' is undefined`);
  } else if (value === null) {
    is(value, expected, `'${name}' has expected value`);
  } else if (
    typeof expected == "string" ||
    typeof expected == "number" ||
    typeof expected == "boolean"
  ) {
    is(value, expected, "property '" + name + "'");
  } else if (expected instanceof RegExp) {
    ok(expected.test(value), name + ": " + expected + " matched " + value);
  } else if (Array.isArray(expected)) {
    info("checking array for property '" + name + "'");
    ok(Array.isArray(value), `property '${name}' is an array`);

    is(value.length, expected.length, "Array has expected length");
    if (value.length !== expected.length) {
      is(JSON.stringify(value, null, 2), JSON.stringify(expected, null, 2));
    } else {
      checkObject(value, expected);
    }
  } else if (typeof expected == "object") {
    info("checking object for property '" + name + "'");
    checkObject(value, expected);
  }
}

async function triggerNetworkRequests(browser, commands) {
  for (let i = 0; i < commands.length; i++) {
    await SpecialPowers.spawn(browser, [commands[i]], async function(code) {
      const script = content.document.createElement("script");
      script.append(
        content.document.createTextNode(
          `async function triggerRequest() {${code}}`
        )
      );
      content.document.body.append(script);
      await content.wrappedJSObject.triggerRequest();
      script.remove();
    });
  }
}
