/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
"use strict";

// A command in a menu.

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { button, li, span } = dom;

const MenuItem = props => {
  const attr = {
    className: "command"
  };

  if (props.id) {
    attr.id = props.id;
  }

  if (props.className) {
    attr.className += " " + props.className;
  }

  if (props.onClick) {
    attr.onClick = props.onClick;
  }

  if (typeof props.checked !== "undefined") {
    attr.role = "menuitemcheckbox";
    if (props.checked) {
      attr["aria-checked"] = true;
    }
  } else {
    attr.role = "menuitem";
  }

  const textLabel = span({ className: "label" }, props.label);
  const children = [textLabel];

  if (typeof props.accelerator !== "undefined") {
    const acceleratorLabel = span(
      { className: "accelerator" },
      props.accelerator
    );
    children.push(acceleratorLabel);
  }

  return li({ className: "menuitem" }, button(attr, children));
};

MenuItem.propTypes = {
  // An optional keyboard shortcut to display next to the item.
  // (This does not actually register the event listener for the key.)
  accelerator: PropTypes.string,

  // A tri-state value that may be true/false if item should be checkable, and
  // undefined otherwise.
  checked: PropTypes.bool,

  // Any additional classes to assign to the button specified as
  // a space-separated string.
  className: PropTypes.string,

  // An optional ID to be assigned to the item.
  id: PropTypes.string,

  // The item label.
  label: PropTypes.string.isRequired,

  // An optional callback to be invoked when the item is selected.
  onClick: PropTypes.func,
};

module.exports = MenuItem;
