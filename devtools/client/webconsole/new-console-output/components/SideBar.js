/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const actions = require("devtools/client/webconsole/new-console-output/actions/index");
const SplitBox = createFactory(require("devtools/client/shared/components/splitter/SplitBox"));

class SideBar extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      sidebarVisible: PropTypes.bool
    };
  }

  constructor(props) {
    super(props);
    this.onClickSidebarToggle = this.onClickSidebarToggle.bind(this);
  }

  onClickSidebarToggle() {
    this.props.dispatch(actions.sidebarToggle());
  }

  render() {
    let {
      sidebarVisible,
    } = this.props;

    let endPanel = dom.aside({
      className: "sidebar-wrapper"
    },
      dom.header({
        className: "devtools-toolbar webconsole-sidebar-toolbar"
      },
        dom.button({
          className: "devtools-button sidebar-close-button",
          onClick: this.onClickSidebarToggle
        })
      ),
      dom.aside({
        className: "sidebar-contents"
      }, "Sidebar WIP")
    );

    return (
      sidebarVisible ?
        SplitBox({
          className: "sidebar",
          endPanel,
          endPanelControl: true,
          initialSize: "200px",
          minSize: "100px",
          vert: true,
        })
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
