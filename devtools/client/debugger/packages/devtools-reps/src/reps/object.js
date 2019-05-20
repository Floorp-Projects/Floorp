/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = require("prop-types");
const { wrapRender, ellipsisElement } = require("./rep-utils");
const PropRep = require("./prop-rep");
const { MODE } = require("./constants");

const dom = require("react-dom-factories");
const { span } = dom;

const DEFAULT_TITLE = "Object";

/**
 * Renders an object. An object is represented by a list of its
 * properties enclosed in curly brackets.
 */
ObjectRep.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  title: PropTypes.string,
};

function ObjectRep(props) {
  const object = props.object;
  const propsArray = safePropIterator(props, object);

  if (props.mode === MODE.TINY) {
    const tinyModeItems = [];
    if (getTitle(props, object) !== DEFAULT_TITLE) {
      tinyModeItems.push(getTitleElement(props, object));
    } else {
      tinyModeItems.push(
        span(
          {
            className: "objectLeftBrace",
          },
          "{"
        ),
        propsArray.length > 0 ? ellipsisElement : null,
        span(
          {
            className: "objectRightBrace",
          },
          "}"
        )
      );
    }

    return span({ className: "objectBox objectBox-object" }, ...tinyModeItems);
  }

  return span(
    { className: "objectBox objectBox-object" },
    getTitleElement(props, object),
    span(
      {
        className: "objectLeftBrace",
      },
      " { "
    ),
    ...propsArray,
    span(
      {
        className: "objectRightBrace",
      },
      " }"
    )
  );
}

function getTitleElement(props, object) {
  return span({ className: "objectTitle" }, getTitle(props, object));
}

function getTitle(props, object) {
  return props.title || DEFAULT_TITLE;
}

function safePropIterator(props, object, max) {
  max = typeof max === "undefined" ? 3 : max;
  try {
    return propIterator(props, object, max);
  } catch (err) {
    console.error(err);
  }
  return [];
}

function propIterator(props, object, max) {
  // Work around https://bugzilla.mozilla.org/show_bug.cgi?id=945377
  if (Object.prototype.toString.call(object) === "[object Generator]") {
    object = Object.getPrototypeOf(object);
  }

  const elements = [];
  const unimportantProperties = [];
  let propertiesNumber = 0;
  const propertiesNames = Object.keys(object);

  const pushPropRep = (name, value) => {
    elements.push(
      PropRep({
        ...props,
        key: name,
        mode: MODE.TINY,
        name,
        object: value,
        equal: ": ",
      })
    );
    propertiesNumber++;

    if (propertiesNumber < propertiesNames.length) {
      elements.push(", ");
    }
  };

  try {
    for (const name of propertiesNames) {
      if (propertiesNumber >= max) {
        break;
      }

      let value;
      try {
        value = object[name];
      } catch (exc) {
        continue;
      }

      // Object members with non-empty values are preferred since it gives the
      // user a better overview of the object.
      if (isInterestingProp(value)) {
        pushPropRep(name, value);
      } else {
        // If the property is not important, put its name on an array for later
        // use.
        unimportantProperties.push(name);
      }
    }
  } catch (err) {
    console.error(err);
  }

  if (propertiesNumber < max) {
    for (const name of unimportantProperties) {
      if (propertiesNumber >= max) {
        break;
      }

      let value;
      try {
        value = object[name];
      } catch (exc) {
        continue;
      }

      pushPropRep(name, value);
    }
  }

  if (propertiesNumber < propertiesNames.length) {
    elements.push(ellipsisElement);
  }

  return elements;
}

function isInterestingProp(value) {
  const type = typeof value;
  return type == "boolean" || type == "number" || (type == "string" && value);
}

function supportsObject(object, noGrip = false) {
  return noGrip;
}

// Exports from this module
module.exports = {
  rep: wrapRender(ObjectRep),
  supportsObject,
};
