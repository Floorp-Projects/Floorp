/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  DOM: dom,
  PropTypes
} = require("devtools/client/shared/vendor/react");

FilterCheckbox.displayName = "FilterCheckbox";

FilterCheckbox.propTypes = {
  label: PropTypes.string.isRequired,
  title: PropTypes.string,
  checked: PropTypes.bool.isRequired,
  onChange: PropTypes.func.isRequired,
};

function FilterCheckbox(props) {
  const {checked, label, title, onChange} = props;
  return dom.label({ title, className: "filter-checkbox" }, dom.input({
    type: "checkbox",
    checked,
    onChange,
  }), label);
}

module.exports = FilterCheckbox;
