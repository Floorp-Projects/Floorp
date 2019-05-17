/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// ReactJS
const PropTypes = require("prop-types");

// Dependencies
const { interleave, isGrip, wrapRender } = require("./rep-utils");
const PropRep = require("./prop-rep");
const { MODE } = require("./constants");

const dom = require("react-dom-factories");
const { span } = dom;

/**
 * Renders generic grip. Grip is client representation
 * of remote JS object and is used as an input object
 * for this rep component.
 */
GripRep.propTypes = {
  object: PropTypes.object.isRequired,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  isInterestingProp: PropTypes.func,
  title: PropTypes.string,
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func,
  noGrip: PropTypes.bool,
};

const DEFAULT_TITLE = "Object";

function GripRep(props) {
  const { mode = MODE.SHORT, object } = props;

  const config = {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-object",
  };

  if (mode === MODE.TINY) {
    const propertiesLength = getPropertiesLength(object);

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
        propertiesLength > 0
          ? span(
              {
                key: "more",
                className: "more-ellipsis",
                title: "more…",
              },
              "…"
            )
          : null,
        span(
          {
            className: "objectRightBrace",
          },
          "}"
        )
      );
    }

    return span(config, ...tinyModeItems);
  }

  const propsArray = safePropIterator(props, object, maxLengthMap.get(mode));

  return span(
    config,
    getTitleElement(props, object),
    span(
      {
        className: "objectLeftBrace",
      },
      " { "
    ),
    ...interleave(propsArray, ", "),
    span(
      {
        className: "objectRightBrace",
      },
      " }"
    )
  );
}

function getTitleElement(props, object) {
  return span(
    {
      className: "objectTitle",
    },
    getTitle(props, object)
  );
}

function getTitle(props, object) {
  return props.title || object.class || DEFAULT_TITLE;
}

function getPropertiesLength(object) {
  let propertiesLength =
    object.preview && object.preview.ownPropertiesLength
      ? object.preview.ownPropertiesLength
      : object.ownPropertyLength;

  if (object.preview && object.preview.safeGetterValues) {
    propertiesLength += Object.keys(object.preview.safeGetterValues).length;
  }

  if (object.preview && object.preview.ownSymbols) {
    propertiesLength += object.preview.ownSymbolsLength;
  }

  return propertiesLength;
}

function safePropIterator(props, object, max) {
  max = typeof max === "undefined" ? maxLengthMap.get(MODE.SHORT) : max;
  try {
    return propIterator(props, object, max);
  } catch (err) {
    console.error(err);
  }
  return [];
}

function propIterator(props, object, max) {
  if (object.preview && Object.keys(object.preview).includes("wrappedValue")) {
    const { Rep } = require("./rep");

    return [
      Rep({
        object: object.preview.wrappedValue,
        mode: props.mode || MODE.TINY,
        defaultRep: Grip,
      }),
    ];
  }

  // Property filter. Show only interesting properties to the user.
  const isInterestingProp =
    props.isInterestingProp ||
    ((type, value) => {
      return (
        type == "boolean" ||
        type == "number" ||
        (type == "string" && value.length != 0)
      );
    });

  let properties = object.preview ? object.preview.ownProperties || {} : {};

  const propertiesLength = getPropertiesLength(object);

  if (object.preview && object.preview.safeGetterValues) {
    properties = { ...properties, ...object.preview.safeGetterValues };
  }

  let indexes = getPropIndexes(properties, max, isInterestingProp);
  if (indexes.length < max && indexes.length < propertiesLength) {
    // There are not enough props yet.
    // Then add uninteresting props to display them.
    indexes = indexes.concat(
      getPropIndexes(properties, max - indexes.length, (t, value, name) => {
        return !isInterestingProp(t, value, name);
      })
    );
  }

  // The server synthesizes some property names for a Proxy, like
  // <target> and <handler>; we don't want to quote these because,
  // as synthetic properties, they appear more natural when
  // unquoted.
  const suppressQuotes = object.class === "Proxy";
  const propsArray = getProps(props, properties, indexes, suppressQuotes);

  // Show symbols.
  if (object.preview && object.preview.ownSymbols) {
    const { ownSymbols } = object.preview;
    const length = max - indexes.length;

    const symbolsProps = ownSymbols.slice(0, length).map(symbolItem => {
      return PropRep({
        ...props,
        mode: MODE.TINY,
        name: symbolItem,
        object: symbolItem.descriptor.value,
        equal: ": ",
        defaultRep: Grip,
        title: null,
        suppressQuotes,
      });
    });

    propsArray.push(...symbolsProps);
  }

  if (
    Object.keys(properties).length > max ||
    propertiesLength > max ||
    // When the object has non-enumerable properties, we don't have them in the
    // packet, but we might want to show there's something in the object.
    propertiesLength > propsArray.length
  ) {
    // There are some undisplayed props. Then display "more...".
    propsArray.push(
      span(
        {
          key: "more",
          className: "more-ellipsis",
          title: "more…",
        },
        "…"
      )
    );
  }

  return propsArray;
}

/**
 * Get props ordered by index.
 *
 * @param {Object} componentProps Grip Component props.
 * @param {Object} properties Properties of the object the Grip describes.
 * @param {Array} indexes Indexes of properties.
 * @param {Boolean} suppressQuotes true if we should suppress quotes
 *                  on property names.
 * @return {Array} Props.
 */
function getProps(componentProps, properties, indexes, suppressQuotes) {
  // Make indexes ordered by ascending.
  indexes.sort(function(a, b) {
    return a - b;
  });

  const propertiesKeys = Object.keys(properties);
  return indexes.map(i => {
    const name = propertiesKeys[i];
    const value = getPropValue(properties[name]);

    return PropRep({
      ...componentProps,
      mode: MODE.TINY,
      name,
      object: value,
      equal: ": ",
      defaultRep: Grip,
      title: null,
      suppressQuotes,
    });
  });
}

/**
 * Get the indexes of props in the object.
 *
 * @param {Object} properties Props object.
 * @param {Number} max The maximum length of indexes array.
 * @param {Function} filter Filter the props you want.
 * @return {Array} Indexes of interesting props in the object.
 */
function getPropIndexes(properties, max, filter) {
  const indexes = [];

  try {
    let i = 0;
    for (const name in properties) {
      if (indexes.length >= max) {
        return indexes;
      }

      // Type is specified in grip's "class" field and for primitive
      // values use typeof.
      const value = getPropValue(properties[name]);
      let type = value.class || typeof value;
      type = type.toLowerCase();

      if (filter(type, value, name)) {
        indexes.push(i);
      }
      i++;
    }
  } catch (err) {
    console.error(err);
  }
  return indexes;
}

/**
 * Get the actual value of a property.
 *
 * @param {Object} property
 * @return {Object} Value of the property.
 */
function getPropValue(property) {
  let value = property;
  if (typeof property === "object") {
    const keys = Object.keys(property);
    if (keys.includes("value")) {
      value = property.value;
    } else if (keys.includes("getterValue")) {
      value = property.getterValue;
    }
  }
  return value;
}

// Registration
function supportsObject(object, noGrip = false) {
  if (noGrip === true || !isGrip(object)) {
    return false;
  }

  if (object.class === "DeadObject") {
    return true;
  }

  return object.preview
    ? typeof object.preview.ownProperties !== "undefined"
    : typeof object.ownPropertyLength !== "undefined";
}

const maxLengthMap = new Map();
maxLengthMap.set(MODE.SHORT, 3);
maxLengthMap.set(MODE.LONG, 10);

// Grip is used in propIterator and has to be defined here.
const Grip = {
  rep: wrapRender(GripRep),
  supportsObject,
  maxLengthMap,
};

// Exports from this module
module.exports = Grip;
