/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
/**
 * This template renders list of parameters within a group.
 * It's essentially a list of name + value pairs.
 */
class NetInfoParams extends Component {
  static get propTypes() {
    return {
      params: PropTypes.arrayOf(PropTypes.shape({
        name: PropTypes.string.isRequired,
        value: PropTypes.string.isRequired
      })).isRequired,
    };
  }

  render() {
    let params = this.props.params || [];

    params.sort(function (a, b) {
      return a.name > b.name ? 1 : -1;
    });

    let rows = [];
    params.forEach((param, index) => {
      rows.push(
        dom.tr({key: index},
          dom.td({className: "netInfoParamName"},
            dom.span({title: param.name}, param.name)
          ),
          dom.td({className: "netInfoParamValue"},
            dom.code({}, param.value)
          )
        )
      );
    });

    return (
      dom.table({cellPadding: 0, cellSpacing: 0},
        dom.tbody({},
          rows
        )
      )
    );
  }
}

// Exports from this module
module.exports = NetInfoParams;
