/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  button,
  span,
} = require("devtools/client/shared/vendor/react-dom-factories");

// Reps
const { getGripType, isGrip, cropString, wrapRender } = require("./rep-utils");
const { MODE } = require("./constants");

const IGNORED_SOURCE_URLS = ["debugger eval code"];

/**
 * This component represents a template for Function objects.
 */
FunctionRep.propTypes = {
  object: PropTypes.object.isRequired,
  onViewSourceInDebugger: PropTypes.func,
  sourceMapService: PropTypes.object,
};

function FunctionRep(props) {
  const {
    object: grip,
    onViewSourceInDebugger,
    recordTelemetryEvent,
    sourceMapService,
  } = props;

  let jumpToDefinitionButton;
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

        const sourceLocation = await getSourceLocation(
          grip.location,
          sourceMapService
        );
        onViewSourceInDebugger(sourceLocation);
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

  if (grip.isClassConstructor) {
    return span(
      elProps,
      getClassTitle(grip, props),
      getFunctionName(grip, props),
      ...getClassBody(parameterNames, props),
      jumpToDefinitionButton
    );
  }

  return span(
    elProps,
    getFunctionTitle(grip, props),
    getFunctionName(grip, props),
    "(",
    ...getParams(parameterNames),
    ")",
    jumpToDefinitionButton
  );
}

function getClassTitle(grip) {
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

function getClassBody(constructorParams, props) {
  const { mode } = props;

  if (mode === MODE.TINY) {
    return [];
  }

  return [" {", ...getClassConstructor(constructorParams), "}"];
}

function getClassConstructor(parameterNames) {
  if (parameterNames.length === 0) {
    return [];
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
  const type = getGripType(grip, noGrip);
  if (noGrip === true || !isGrip(grip)) {
    return type == "function";
  }

  return type == "Function";
}

async function getSourceLocation(location, sourceMapService) {
  if (!sourceMapService) {
    return location;
  }
  try {
    const originalLocation = await sourceMapService.originalPositionFor(
      location.url,
      location.line,
      location.column
    );
    if (originalLocation) {
      const { sourceUrl, line, column } = originalLocation;
      return { url: sourceUrl, line, column };
    }
  } catch (e) {}
  return location;
}

// Exports from this module

module.exports = {
  rep: wrapRender(FunctionRep),
  supportsObject,
  cleanFunctionName,
  // exported for testing purpose.
  getFunctionName,
};
