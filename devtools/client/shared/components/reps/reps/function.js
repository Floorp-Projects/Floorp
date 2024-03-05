/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const {
    button,
    span,
  } = require("devtools/client/shared/vendor/react-dom-factories");

  // Reps
  const {
    getGripType,
    cropString,
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");
  const {
    MODE,
  } = require("devtools/client/shared/components/reps/reps/constants");

  const IGNORED_SOURCE_URLS = ["debugger eval code"];

  /**
   * This component represents a template for Function objects.
   */

  FunctionRep.propTypes = {
    object: PropTypes.object.isRequired,
    onViewSourceInDebugger: PropTypes.func,
    shouldRenderTooltip: PropTypes.bool,
  };

  function FunctionRep(props) {
    const {
      object: grip,
      onViewSourceInDebugger,
      recordTelemetryEvent,
      shouldRenderTooltip,
    } = props;

    let jumpToDefinitionButton;

    // Test to see if we should display the link back to the original function definition
    if (
      onViewSourceInDebugger &&
      grip.location &&
      grip.location.url &&
      !IGNORED_SOURCE_URLS.includes(grip.location.url)
    ) {
      jumpToDefinitionButton = button({
        className: "jump-definition",
        draggable: false,
        title: "Jump to definition",
        onClick: async e => {
          // Stop the event propagation so we don't trigger ObjectInspector
          // expand/collapse.
          e.stopPropagation();
          if (recordTelemetryEvent) {
            recordTelemetryEvent("jump_to_definition");
          }

          onViewSourceInDebugger(grip.location);
        },
      });
    }

    const elProps = {
      "data-link-actor-id": grip.actor,
      className: "objectBox objectBox-function",
      // Set dir="ltr" to prevent parentheses from
      // appearing in the wrong direction
      dir: "ltr",
    };

    const parameterNames = (grip.parameterNames || []).filter(Boolean);
    const fnTitle = getFunctionTitle(grip, props);
    const fnName = getFunctionName(grip, props);

    if (grip.isClassConstructor) {
      const classTitle = getClassTitle(grip, props);
      const classBodyTooltip = getClassBody(parameterNames, true, props);
      const classTooltip = `${classTitle ? classTitle.props.children : ""}${
        fnName ? fnName : ""
      }${classBodyTooltip.join("")}`;

      elProps.title = shouldRenderTooltip ? classTooltip : null;

      return span(
        elProps,
        classTitle,
        fnName,
        ...getClassBody(parameterNames, false, props),
        jumpToDefinitionButton
      );
    }

    const fnTooltip = `${fnTitle ? fnTitle.props.children : ""}${
      fnName ? fnName : ""
    }(${parameterNames.join(", ")})`;

    elProps.title = shouldRenderTooltip ? fnTooltip : null;

    const returnSpan = span(
      elProps,
      fnTitle,
      fnName,
      "(",
      ...getParams(parameterNames),
      ")",
      jumpToDefinitionButton
    );

    return returnSpan;
  }

  function getClassTitle() {
    return span(
      {
        className: "objectTitle",
      },
      "class "
    );
  }

  function getFunctionTitle(grip, props) {
    const { mode } = props;

    if (mode === MODE.TINY && !grip.isGenerator && !grip.isAsync) {
      return null;
    }

    let title = mode === MODE.TINY ? "" : "function ";

    if (grip.isGenerator) {
      title = mode === MODE.TINY ? "* " : "function* ";
    }

    if (grip.isAsync) {
      title = `${"async" + " "}${title}`;
    }

    return span(
      {
        className: "objectTitle",
      },
      title
    );
  }

  /**
   * Returns a ReactElement representing the function name.
   *
   * @param {Object} grip : Function grip
   * @param {Object} props: Function rep props
   */
  function getFunctionName(grip, props = {}) {
    let { functionName } = props;
    let name;

    if (functionName) {
      const end = functionName.length - 1;
      functionName =
        functionName.startsWith('"') && functionName.endsWith('"')
          ? functionName.substring(1, end)
          : functionName;
    }

    if (
      grip.displayName != undefined &&
      functionName != undefined &&
      grip.displayName != functionName
    ) {
      name = `${functionName}:${grip.displayName}`;
    } else {
      name = cleanFunctionName(
        grip.userDisplayName ||
          grip.displayName ||
          grip.name ||
          props.functionName ||
          ""
      );
    }

    return cropString(name, 100);
  }

  const objectProperty = /([\w\d\$]+)$/;
  const arrayProperty = /\[(.*?)\]$/;
  const functionProperty = /([\w\d]+)[\/\.<]*?$/;
  const annonymousProperty = /([\w\d]+)\(\^\)$/;

  /**
   * Decodes an anonymous naming scheme that
   * spider monkey implements based on "Naming Anonymous JavaScript Functions"
   * http://johnjbarton.github.io/nonymous/index.html
   *
   * @param {String} name : Function name to clean up
   * @returns String
   */
  function cleanFunctionName(name) {
    for (const reg of [
      objectProperty,
      arrayProperty,
      functionProperty,
      annonymousProperty,
    ]) {
      const match = reg.exec(name);
      if (match) {
        return match[1];
      }
    }

    return name;
  }

  function getClassBody(constructorParams, textOnly = false, props) {
    const { mode } = props;

    if (mode === MODE.TINY) {
      return [];
    }

    return [" {", ...getClassConstructor(textOnly, constructorParams), "}"];
  }

  function getClassConstructor(textOnly = false, parameterNames) {
    if (parameterNames.length === 0) {
      return [];
    }

    if (textOnly) {
      return [` constructor(${parameterNames.join(", ")}) `];
    }
    return [" constructor(", ...getParams(parameterNames), ") "];
  }

  function getParams(parameterNames) {
    return parameterNames.flatMap((param, index, arr) => {
      return [
        span({ className: "param" }, param),
        index === arr.length - 1 ? "" : span({ className: "delimiter" }, ", "),
      ];
    });
  }

  // Registration
  function supportsObject(grip, noGrip = false) {
    return getGripType(grip, noGrip) === "Function";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(FunctionRep),
    supportsObject,
    cleanFunctionName,
    // exported for testing purpose.
    getFunctionName,
  };
});
