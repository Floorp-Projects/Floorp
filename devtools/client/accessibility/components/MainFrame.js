/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global gToolbox */

// React & Redux
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { reset } = require("../actions/ui");

// Constants
const { SIDEBAR_WIDTH, PORTRAIT_MODE_WIDTH } = require("../constants");

// Accessibility Panel
const AccessibilityTree = createFactory(require("./AccessibilityTree"));
const Description = createFactory(require("./Description").Description);
const RightSidebar = createFactory(require("./RightSidebar"));
const Toolbar = createFactory(require("./Toolbar"));
const SplitBox = createFactory(require("devtools/client/shared/components/splitter/SplitBox"));

/**
 * Renders basic layout of the Accessibility panel. The Accessibility panel
 * content consists of two main parts: tree and sidebar.
 */
class MainFrame extends Component {
  static get propTypes() {
    return {
      accessibility: PropTypes.object.isRequired,
      walker: PropTypes.object.isRequired,
      enabled: PropTypes.bool.isRequired,
      dispatch: PropTypes.func.isRequired
    };
  }

  constructor(props) {
    super(props);

    this.resetAccessibility = this.resetAccessibility.bind(this);
    this.onPanelWindowResize = this.onPanelWindowResize.bind(this);
  }

  componentWillMount() {
    // Need inspector for many things such as dom node preview etc.
    gToolbox.loadTool("inspector");
    this.props.accessibility.on("init", this.resetAccessibility);
    this.props.accessibility.on("shutdown", this.resetAccessibility);
    this.props.walker.on("document-ready", this.resetAccessibility);

    window.addEventListener("resize", this.onPanelWindowResize, true);
  }

  componentWillReceiveProps({ enabled }) {
    if (this.props.enabled && !enabled) {
      this.resetAccessibility();
    }
  }

  componentWillUnmount() {
    this.props.accessibility.off("init", this.resetAccessibility);
    this.props.accessibility.off("shutdown", this.resetAccessibility);
    this.props.walker.off("document-ready", this.resetAccessibility);

    window.removeEventListener("resize", this.onPanelWindowResize, true);
  }

  resetAccessibility() {
    const { dispatch, accessibility } = this.props;
    dispatch(reset(accessibility));
  }

  get useLandscapeMode() {
    const { clientWidth } = document.getElementById("content");
    return clientWidth > PORTRAIT_MODE_WIDTH;
  }

  /**
   * If panel width is less than PORTRAIT_MODE_WIDTH px, the splitter changes
   * its mode to `horizontal` to support portrait view.
   */
  onPanelWindowResize() {
    if (this.refs.splitBox) {
      this.refs.splitBox.setState({ vert: this.useLandscapeMode });
    }
  }

  /**
   * Render Accessibility panel content
   */
  render() {
    const { accessibility, walker, enabled } = this.props;

    if (!enabled) {
      return Description({ accessibility });
    }

    return (
      div({ className: "mainFrame", role: "presentation" },
        Toolbar({ accessibility, walker }),
        SplitBox({
          ref: "splitBox",
          initialSize: SIDEBAR_WIDTH,
          minSize: "10px",
          maxSize: "80%",
          splitterSize: 1,
          endPanelControl: true,
          startPanel: div({
            className: "main-panel",
            role: "presentation"
          }, AccessibilityTree({ walker })),
          endPanel: RightSidebar(),
          vert: this.useLandscapeMode,
        })
      ));
  }
}

const mapStateToProps = ({ ui }) => ({
  enabled: ui.enabled
});

// Exports from this module
module.exports = connect(mapStateToProps)(MainFrame);
