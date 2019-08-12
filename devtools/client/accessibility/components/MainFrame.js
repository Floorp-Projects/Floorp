/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React & Redux
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const {
  span,
  div,
} = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { reset } = require("../actions/ui");

// Localization
const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

// Constants
const { SIDEBAR_WIDTH, PORTRAIT_MODE_WIDTH } = require("../constants");

// Accessibility Panel
const AccessibilityTree = createFactory(require("./AccessibilityTree"));
const AuditProgressOverlay = createFactory(require("./AuditProgressOverlay"));
const Description = createFactory(require("./Description").Description);
const RightSidebar = createFactory(require("./RightSidebar"));
const Toolbar = createFactory(require("./Toolbar"));
const SplitBox = createFactory(
  require("devtools/client/shared/components/splitter/SplitBox")
);

/**
 * Renders basic layout of the Accessibility panel. The Accessibility panel
 * content consists of two main parts: tree and sidebar.
 */
class MainFrame extends Component {
  static get propTypes() {
    return {
      accessibility: PropTypes.object.isRequired,
      fluentBundles: PropTypes.array.isRequired,
      accessibilityWalker: PropTypes.object.isRequired,
      enabled: PropTypes.bool.isRequired,
      dispatch: PropTypes.func.isRequired,
      auditing: PropTypes.array.isRequired,
      supports: PropTypes.object,
      getDOMWalker: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.resetAccessibility = this.resetAccessibility.bind(this);
    this.onPanelWindowResize = this.onPanelWindowResize.bind(this);
  }

  componentWillMount() {
    this.props.accessibility.on("init", this.resetAccessibility);
    this.props.accessibility.on("shutdown", this.resetAccessibility);
    this.props.accessibilityWalker.on(
      "document-ready",
      this.resetAccessibility
    );

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
    this.props.accessibilityWalker.off(
      "document-ready",
      this.resetAccessibility
    );

    window.removeEventListener("resize", this.onPanelWindowResize, true);
  }

  resetAccessibility() {
    const { dispatch, accessibility, supports } = this.props;
    dispatch(reset(accessibility, supports));
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
    const {
      accessibility,
      accessibilityWalker,
      getDOMWalker,
      fluentBundles,
      enabled,
      auditing,
    } = this.props;

    if (!enabled) {
      return Description({ accessibility });
    }

    // Audit is currently running.
    const isAuditing = auditing.length > 0;

    return LocalizationProvider(
      { bundles: fluentBundles },
      div(
        { className: "mainFrame", role: "presentation" },
        Toolbar({ accessibility, accessibilityWalker }),
        isAuditing && AuditProgressOverlay(),
        span(
          {
            "aria-hidden": isAuditing,
            role: "presentation",
            style: { display: "contents" },
          },
          SplitBox({
            ref: "splitBox",
            initialSize: SIDEBAR_WIDTH,
            minSize: "10px",
            maxSize: "80%",
            splitterSize: 1,
            endPanelControl: true,
            startPanel: div(
              {
                className: "main-panel",
                role: "presentation",
              },
              AccessibilityTree({ accessibilityWalker, getDOMWalker })
            ),
            endPanel: RightSidebar({ accessibilityWalker, getDOMWalker }),
            vert: this.useLandscapeMode,
          })
        )
      )
    );
  }
}

const mapStateToProps = ({
  ui: { enabled, supports },
  audit: { auditing },
}) => ({
  enabled,
  supports,
  auditing,
});

// Exports from this module
module.exports = connect(mapStateToProps)(MainFrame);
