/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const React = require("devtools/client/shared/vendor/react");

// Shortcuts
const DOM = React.DOM;
const PropTypes = React.PropTypes;

/**
 * This template renders list of parameters within a group.
 * It's essentially a list of name + value pairs.
 */
var NetInfoParams = React.createClass({
  propTypes: {
    params: PropTypes.arrayOf(PropTypes.shape({
      name: PropTypes.string.isRequired,
      value: PropTypes.string.isRequired
    })).isRequired,
  },

  displayName: "NetInfoParams",

  render() {
    let params = this.props.params || [];

    params.sort(function (a, b) {
      return a.name > b.name ? 1 : -1;
    });

    let rows = [];
    params.forEach(param => {
      rows.push(
        DOM.tr({key: param.name},
          DOM.td({className: "netInfoParamName"},
            DOM.span({title: param.name}, param.name)
          ),
          DOM.td({className: "netInfoParamValue"},
            DOM.code({}, param.value)
          )
        )
      );
    });

    return (
      DOM.table({cellPadding: 0, cellSpacing: 0},
        DOM.tbody({},
          rows
        )
      )
    );
  }
});

// Exports from this module
module.exports = NetInfoParams;
