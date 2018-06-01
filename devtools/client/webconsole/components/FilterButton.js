/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const actions = require("devtools/client/webconsole/actions/index");

FilterButton.displayName = "FilterButton";

FilterButton.propTypes = {
  label: PropTypes.string.isRequired,
  filterKey: PropTypes.string.isRequired,
  active: PropTypes.bool.isRequired,
  dispatch: PropTypes.func.isRequired,
};

function FilterButton(props) {
  const {active, label, filterKey, dispatch} = props;
  const classList = [
    "devtools-button",
    filterKey,
  ];
  if (active) {
    classList.push("checked");
  }

  return dom.button({
    "aria-pressed": active === true,
    className: classList.join(" "),
    onClick: () => {
      dispatch(actions.filterToggle(filterKey));
    },
  }, label);
}

module.exports = FilterButton;
