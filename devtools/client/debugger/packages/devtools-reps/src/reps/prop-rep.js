/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = require("prop-types");
const { maybeEscapePropertyName, wrapRender } = require("./rep-utils");
const { MODE } = require("./constants");

const { span } = require("react-dom-factories");

/**
 * Property for Obj (local JS objects), Grip (remote JS objects)
 * and GripMap (remote JS maps and weakmaps) reps.
 * It's used to render object properties.
 */
PropRep.propTypes = {
  // Property name.
  name: PropTypes.oneOfType([PropTypes.string, PropTypes.object]).isRequired,
  // Equal character rendered between property name and value.
  equal: PropTypes.string,
  // @TODO Change this to Object.values when supported in Node's version of V8
  mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func,
  // Normally a PropRep will quote a property name that isn't valid
  // when unquoted; but this flag can be used to suppress the
  // quoting.
  suppressQuotes: PropTypes.bool,
};

/**
 * Function that given a name, a delimiter and an object returns an array
 * of React elements representing an object property (e.g. `name: value`)
 *
 * @param {Object} props
 * @return {Array} Array of React elements.
 */
function PropRep(props) {
  const Grip = require("./grip");
  const { Rep } = require("./rep");

  let { name, mode, equal, suppressQuotes } = props;

  let key;
  // The key can be a simple string, for plain objects,
  // or another object for maps and weakmaps.
  if (typeof name === "string") {
    if (!suppressQuotes) {
      name = maybeEscapePropertyName(name);
    }
    key = span({ className: "nodeName" }, name);
  } else {
    key = Rep({
      ...props,
      className: "nodeName",
      object: name,
      mode: mode || MODE.TINY,
      defaultRep: Grip,
    });
  }

  return [
    key,
    span(
      {
        className: "objectEqual",
      },
      equal
    ),
    Rep({ ...props }),
  ];
}

// Exports from this module
module.exports = wrapRender(PropRep);
