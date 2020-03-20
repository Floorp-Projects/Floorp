/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  webExtensionInspectedWindowSpec,
} = require("devtools/shared/specs/addon/webextension-inspected-window");

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

const {
  getAdHocFrontOrPrimitiveGrip,
} = require("devtools/shared/fronts/object");

/**
 * The corresponding Front object for the WebExtensionInspectedWindowActor.
 */
class WebExtensionInspectedWindowFront extends FrontClassWithSpec(
  webExtensionInspectedWindowSpec
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "webExtensionInspectedWindowActor";
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

    const response = await super.eval(
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
}

exports.WebExtensionInspectedWindowFront = WebExtensionInspectedWindowFront;
registerFront(WebExtensionInspectedWindowFront);
