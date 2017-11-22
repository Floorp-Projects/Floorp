/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

class SideBar extends Component {
  static get propTypes() {
    return {
      sidebarVisible: PropTypes.bool
    };
  }

  render() {
    let {
      sidebarVisible,
    } = this.props;

    return (
      sidebarVisible ?
        dom.aside({
          className: "sidebar",
        }, "Sidebar WIP")
        : null
    );
  }
}

function mapStateToProps(state, props) {
  return {
    sidebarVisible: state.ui.sidebarVisible,
  };
}

module.exports = connect(mapStateToProps)(SideBar);
