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
   * Reload the target tab, optionally bypass cache, customize the userAgent and/or
   * inject a script in targeted document or any of its sub-frame.
   *
   * @param {WebExtensionCallerInfo} callerInfo
   *   the addonId and the url (the addon base url or the url of the actual caller
   *   filename and lineNumber) used to log useful debugging information in the
   *   produced error logs and eval stack trace.
   * @param {Object} options
   * @param {boolean|undefined} options.ignoreCache
   *        Enable/disable the cache bypass headers.
   * @param {string|undefined} options.injectedScript
   *        Evaluate the provided javascript code in the top level and every sub-frame
   *        created during the page reload, before any other script in the page has been
   *        executed.
   * @param {string|undefined} options.userAgent
   *        Customize the userAgent during the page reload.
   * @returns {Promise} A promise that resolves once the page is done loading when userAgent
   *          or injectedScript option are passed. If those options are not provided, the
   *          Promise will resolve after the reload was initiated.
   */
  async reload(callerInfo, options = {}) {
    if (this._reloadPending) {
      return null;
    }

    this._reloadPending = true;

    try {
      // If this is called with a `userAgent` property, we need to update the target configuration
      // so the custom user agent will be set on the parent process.
      if (typeof options.userAgent !== undefined) {
        await this.commands.targetConfigurationCommand.updateConfiguration({
          customUserAgent: options.userAgent,
        });
      }

      const front = await this.getFront();
      const result = await front.reload(callerInfo, options);
      this._reloadPending = false;

      return result;
    } catch (e) {
      this._reloadPending = false;
      Cu.reportError(e);
      return Promise.reject({
        message: "An unexpected error occurred",
      });
    }
  }
}

module.exports = InspectedWindowCommand;
