/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = require("prop-types");
// Utils
const { getGripType, isGrip, wrapRender } = require("./rep-utils");
const { cleanFunctionName } = require("./function");
const { isLongString } = require("./string");
const { MODE } = require("./constants");

const dom = require("react-dom-factories");
const { span } = dom;
const IGNORED_SOURCE_URLS = ["debugger eval code"];

/**
 * Renders Error objects.
 */
ErrorRep.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  // An optional function that will be used to render the Error stacktrace.
  renderStacktrace: PropTypes.func,
};

function ErrorRep(props) {
  const object = props.object;
  const preview = object.preview;
  const mode = props.mode;

  let name;
  if (preview && preview.name && preview.kind) {
    switch (preview.kind) {
      case "Error":
        name = preview.name;
        break;
      case "DOMException":
        name = preview.kind;
        break;
      default:
        throw new Error("Unknown preview kind for the Error rep.");
    }
  } else {
    name = "Error";
  }

  const content = [];

  if (mode === MODE.TINY) {
    content.push(name);
  } else {
    content.push(`${name}: "${preview.message}"`);
  }

  if (preview.stack && (mode !== MODE.TINY && mode !== MODE.SHORT)) {
    const stacktrace = props.renderStacktrace
      ? props.renderStacktrace(parseStackString(preview.stack))
      : getStacktraceElements(props, preview);
    content.push(stacktrace);
  }

  return span(
    {
      "data-link-actor-id": object.actor,
      className: "objectBox-stackTrace",
    },
    content
  );
}

/**
 * Returns a React element reprensenting the Error stacktrace, i.e.
 * transform error.stack from:
 *
 * semicolon@debugger eval code:1:109
 * jkl@debugger eval code:1:63
 * asdf@debugger eval code:1:28
 * @debugger eval code:1:227
 *
 * Into a column layout:
 *
 * semicolon  (<anonymous>:8:10)
 * jkl        (<anonymous>:5:10)
 * asdf       (<anonymous>:2:10)
 *            (<anonymous>:11:1)
 */
function getStacktraceElements(props, preview) {
  const stack = [];
  if (!preview.stack) {
    return stack;
  }

  parseStackString(preview.stack).forEach((frame, index, frames) => {
    let onLocationClick;
    const {
      filename,
      lineNumber,
      columnNumber,
      functionName,
      location,
    } = frame;

    if (
      props.onViewSourceInDebugger &&
      !IGNORED_SOURCE_URLS.includes(filename)
    ) {
      onLocationClick = e => {
        // Don't trigger ObjectInspector expand/collapse.
        e.stopPropagation();
        props.onViewSourceInDebugger({
          url: filename,
          line: lineNumber,
          column: columnNumber,
        });
      };
    }

    stack.push(
      "\t",
      span(
        {
          key: `fn${index}`,
          className: "objectBox-stackTrace-fn",
        },
        cleanFunctionName(functionName)
      ),
      " ",
      span(
        {
          key: `location${index}`,
          className: "objectBox-stackTrace-location",
          onClick: onLocationClick,
          title: onLocationClick
            ? `View source in debugger → ${location}`
            : undefined,
        },
        location
      ),
      "\n"
    );
  });

  return span(
    {
      key: "stack",
      className: "objectBox-stackTrace-grid",
    },
    stack
  );
}

/**
 * Parse a string that should represent a stack trace and returns an array of
 * the frames. The shape of the frames are extremely important as they can then
 * be processed here or in the toolbox by other components.
 * @param {String} stack
 * @returns {Array} Array of frames, which are object with the following shape:
 *                  - {String} filename
 *                  - {String} functionName
 *                  - {String} location
 *                  - {Number} columnNumber
 *                  - {Number} lineNumber
 */
function parseStackString(stack) {
  const res = [];
  if (!stack) {
    return res;
  }

  const isStacktraceALongString = isLongString(stack);
  const stackString = isStacktraceALongString ? stack.initial : stack;

  stackString.split("\n").forEach((frame, index, frames) => {
    if (!frame) {
      // Skip any blank lines
      return;
    }

    // If the stacktrace is a longString, don't include the last frame in the
    // array, since it is certainly incomplete.
    // Can be removed when https://bugzilla.mozilla.org/show_bug.cgi?id=1448833
    // is fixed.
    if (isStacktraceALongString && index === frames.length - 1) {
      return;
    }

    let functionName;
    let location;

    // Given the input: "functionName@scriptLocation:2:100"
    // Result: [
    //   "functionName@scriptLocation:2:100",
    //   "functionName",
    //   "scriptLocation:2:100"
    // ]
    const result = frame.match(/^(.*)@(.*)$/);
    if (result && result.length === 3) {
      functionName = result[1];

      // If the resource was loaded by base-loader.js, the location looks like:
      // resource://devtools/shared/base-loader.js -> resource://path/to/file.js .
      // What's needed is only the last part after " -> ".
      location = result[2].split(" -> ").pop();
    }

    if (!functionName) {
      functionName = "<anonymous>";
    }

    // Given the input: "scriptLocation:2:100"
    // Result:
    // ["scriptLocation:2:100", "scriptLocation", "2", "100"]
    const locationParts = location
      ? location.match(/^(.*):(\d+):(\d+)$/)
      : null;

    if (location && locationParts) {
      const [, filename, line, column] = locationParts;
      res.push({
        filename,
        functionName,
        location,
        columnNumber: Number(column),
        lineNumber: Number(line),
      });
    }
  });

  return res;
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }
  return (
    (object.preview && getGripType(object, noGrip) === "Error") ||
    object.class === "DOMException"
  );
}

// Exports from this module
module.exports = {
  rep: wrapRender(ErrorRep),
  supportsObject,
};
