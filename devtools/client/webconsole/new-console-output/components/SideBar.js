/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { getObjectInspector } = require("devtools/client/webconsole/new-console-output/utils/object-inspector");
const actions = require("devtools/client/webconsole/new-console-output/actions/index");
const SplitBox = createFactory(require("devtools/client/shared/components/splitter/SplitBox"));

const reps = require("devtools/client/shared/components/reps/reps");
const { MODE } = reps;

class SideBar extends Component {
  static get propTypes() {
    return {
      serviceContainer: PropTypes.object,
      dispatch: PropTypes.func.isRequired,
      sidebarVisible: PropTypes.bool,
      grip: PropTypes.object,
    };
  }

  constructor(props) {
    super(props);
    this.onClickSidebarClose = this.onClickSidebarClose.bind(this);
  }

  onClickSidebarClose() {
    this.props.dispatch(actions.sidebarClose());
  }

  render() {
    let {
      sidebarVisible,
      grip,
      serviceContainer,
    } = this.props;

    let objectInspector = getObjectInspector(grip, serviceContainer, {
      autoExpandDepth: 1,
      mode: MODE.SHORT,
    });

    let endPanel = dom.aside({
      className: "sidebar-wrapper"
    },
      dom.header({
        className: "devtools-toolbar webconsole-sidebar-toolbar"
      },
        dom.button({
          className: "devtools-button sidebar-close-button",
          onClick: this.onClickSidebarClose
        })
      ),
      dom.aside({
        className: "sidebar-contents"
      }, objectInspector)
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
    grip: state.ui.gripInSidebar,
  };
}

module.exports = connect(mapStateToProps)(SideBar);
