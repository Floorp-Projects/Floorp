/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getAdHocFrontOrPrimitiveGrip,
  // eslint-disable-next-line mozilla/reject-some-requires
} = require("devtools/client/fronts/object");

/**
 * For now, this class is mostly a wrapper around webExtInspectedWindow actor.
 */
class InspectedWindowCommand {
  constructor({ commands }) {
    this.commands = commands;
  }

  /**
   * Return a promise that resolves to the related target actor's front.
   * The Web Extension inspected window actor.
   *
   * @return {Promise<WebExtensionInspectedWindowFront>}
   */
  getFront() {
    return this.commands.targetCommand.targetFront.getFront(
      "webExtensionInspectedWindow"
    );
  }

  /**
   * Evaluate the provided javascript code in a target window.
   *
   * @param {Object} webExtensionCallerInfo - The addonId and the url (the addon base url
   *        or the url of the actual caller filename and lineNumber) used to log useful
   *        debugging information in the produced error logs and eval stack trace.
   * @param {String} expression - The expression to evaluate.
   * @param {Object} options - An option object. Check the actor method definition to see
   *        what properties it can hold (minus the `consoleFront` property which is defined
   *        below).
   * @param {WebConsoleFront} options.consoleFront - An optional webconsole front. When
   *         set, the result will be either a primitive, a LongStringFront or an
   *         ObjectFront, and the WebConsoleActor corresponding to the console front will
   *         be used to generate those, which is needed if we want to handle ObjectFronts
   *         on the client.
   */
  async eval(webExtensionCallerInfo, expression, options = {}) {
    const { consoleFront } = options;

    if (consoleFront) {
      options.evalResultAsGrip = true;
      options.toolboxConsoleActorID = consoleFront.actor;
      delete options.consoleFront;
    }

    const front = await this.getFront();
    const response = await front.eval(
      webExtensionCallerInfo,
      expression,
      options
    );

    // If no consoleFront was provided, we can directly return the response.
    if (!consoleFront) {
      return response;
    }

    if (
      !response.hasOwnProperty("exceptionInfo") &&
      !response.hasOwnProperty("valueGrip")
    ) {
      throw new Error(
        "Response does not have `exceptionInfo` or `valueGrip` property"
      );
    }

    if (response.exceptionInfo) {
      console.error(
        response.exceptionInfo.description,
        ...(response.exceptionInfo.details || [])
      );
      return response;
    }

    // On the server, the valueGrip is created from the toolbox webconsole actor.
    // If we want since the ObjectFront connection is inherited from the parent front, we
    // need to set the console front as the parent front.
    return getAdHocFrontOrPrimitiveGrip(
      response.valueGrip,
      consoleFront || this
    );
  }

  /**
   * For more information about the arguments, please have a look at
   * the actor specification: devtools/shared/specs/addon/webextension-inspected-window.js
   * or actor: devtools/server/actors/addon/webextension-inspected-window.js
   */
  async reload(callerInfo, options) {
    const front = await this.getFront();
    return front.reload(callerInfo, options);
  }
}

module.exports = InspectedWindowCommand;
