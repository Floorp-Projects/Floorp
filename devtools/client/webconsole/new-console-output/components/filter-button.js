/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  createClass,
  DOM: dom,
  PropTypes
} = require("devtools/client/shared/vendor/react");
const actions = require("devtools/client/webconsole/new-console-output/actions/index");

const FilterButton = createClass({

  displayName: "FilterButton",

  propTypes: {
    label: PropTypes.string.isRequired,
    filterKey: PropTypes.string.isRequired,
    active: PropTypes.bool.isRequired,
    dispatch: PropTypes.func.isRequired,
  },

  onClick: function () {
    this.props.dispatch(actions.filterToggle(this.props.filterKey));
  },

  render() {
    const {active, label, filterKey} = this.props;

    let classList = [
      "menu-filter-button",
      filterKey,
    ];
    if (active) {
      classList.push("checked");
    }

    return dom.button({
      className: classList.join(" "),
      onClick: this.onClick
    }, label);
  }
});

exports.FilterButton = FilterButton;
