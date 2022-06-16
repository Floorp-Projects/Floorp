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
const {
  enable,
  reset,
  updateCanBeEnabled,
  updateCanBeDisabled,
} = require("devtools/client/accessibility/actions/ui");

// Localization
const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

// Constants
const {
  SIDEBAR_WIDTH,
  PORTRAIT_MODE_WIDTH,
} = require("devtools/client/accessibility/constants");

// Accessibility Panel
const AccessibilityTree = createFactory(
  require("devtools/client/accessibility/components/AccessibilityTree")
);
const AuditProgressOverlay = createFactory(
  require("devtools/client/accessibility/components/AuditProgressOverlay")
);
const Description = createFactory(
  require("devtools/client/accessibility/components/Description").Description
);
const RightSidebar = createFactory(
  require("devtools/client/accessibility/components/RightSidebar")
);
const Toolbar = createFactory(
  require("devtools/client/accessibility/components/Toolbar").Toolbar
);
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
      fluentBundles: PropTypes.array.isRequired,
      enabled: PropTypes.bool.isRequired,
      dispatch: PropTypes.func.isRequired,
      auditing: PropTypes.array.isRequired,
      supports: PropTypes.object,
      toolbox: PropTypes.object.isRequired,
      getAccessibilityTreeRoot: PropTypes.func.isRequired,
      startListeningForAccessibilityEvents: PropTypes.func.isRequired,
      stopListeningForAccessibilityEvents: PropTypes.func.isRequired,
      audit: PropTypes.func.isRequired,
      simulate: PropTypes.func,
      enableAccessibility: PropTypes.func.isRequired,
      resetAccessiblity: PropTypes.func.isRequired,
      startListeningForLifecycleEvents: PropTypes.func.isRequired,
      stopListeningForLifecycleEvents: PropTypes.func.isRequired,
      startListeningForParentLifecycleEvents: PropTypes.func.isRequired,
      stopListeningForParentLifecycleEvents: PropTypes.func.isRequired,
      highlightAccessible: PropTypes.func.isRequired,
      unhighlightAccessible: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.resetAccessibility = this.resetAccessibility.bind(this);
    this.onPanelWindowResize = this.onPanelWindowResize.bind(this);
    this.onCanBeEnabledChange = this.onCanBeEnabledChange.bind(this);
    this.onCanBeDisabledChange = this.onCanBeDisabledChange.bind(this);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillMount() {
    this.props.startListeningForLifecycleEvents({
      init: this.resetAccessibility,
      shutdown: this.resetAccessibility,
    });
    this.props.startListeningForParentLifecycleEvents({
      "can-be-enabled-change": this.onCanBeEnabledChange,
      "can-be-disabled-change": this.onCanBeDisabledChange,
    });
    this.props.startListeningForAccessibilityEvents({
      "top-level-document-ready": this.resetAccessibility,
    });
    window.addEventListener("resize", this.onPanelWindowResize, true);
  }

  componentWillUnmount() {
    this.props.stopListeningForLifecycleEvents({
      init: this.resetAccessibility,
      shutdown: this.resetAccessibility,
    });
    this.props.stopListeningForParentLifecycleEvents({
      "can-be-enabled-change": this.onCanBeEnabledChange,
      "can-be-disabled-change": this.onCanBeDisabledChange,
    });
    this.props.stopListeningForAccessibilityEvents({
      "top-level-document-ready": this.resetAccessibility,
    });
    window.removeEventListener("resize", this.onPanelWindowResize, true);
  }

  resetAccessibility() {
    const { dispatch, resetAccessiblity, supports } = this.props;
    dispatch(reset(resetAccessiblity, supports));
  }

  onCanBeEnabledChange(canBeEnabled) {
    const { enableAccessibility, dispatch } = this.props;
    dispatch(updateCanBeEnabled(canBeEnabled));
    if (canBeEnabled) {
      dispatch(enable(enableAccessibility));
    }
  }

  onCanBeDisabledChange(canBeDisabled) {
    this.props.dispatch(updateCanBeDisabled(canBeDisabled));
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
      fluentBundles,
      enabled,
      auditing,
      simulate,
      toolbox,
      getAccessibilityTreeRoot,
      startListeningForAccessibilityEvents,
      stopListeningForAccessibilityEvents,
      audit,
      highlightAccessible,
      unhighlightAccessible,
    } = this.props;

    if (!enabled) {
      return Description();
    }

    // Audit is currently running.
    const isAuditing = auditing.length > 0;

    return LocalizationProvider(
      { bundles: fluentBundles },
      div(
        { className: "mainFrame", role: "presentation", tabIndex: "-1" },
        Toolbar({
          audit,
          simulate,
          toolboxDoc: toolbox.doc,
        }),
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
            minSize: "10%",
            maxSize: "80%",
            splitterSize: 1,
            endPanelControl: true,
            startPanel: div(
              {
                className: "main-panel",
                role: "presentation",
                tabIndex: "-1",
              },
              AccessibilityTree({
                toolboxDoc: toolbox.doc,
                getAccessibilityTreeRoot,
                startListeningForAccessibilityEvents,
                stopListeningForAccessibilityEvents,
                highlightAccessible,
                unhighlightAccessible,
              })
            ),
            endPanel: RightSidebar({
              highlightAccessible,
              unhighlightAccessible,
              toolbox,
            }),
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
