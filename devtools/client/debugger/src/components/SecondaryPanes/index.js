/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const SplitBox = require("resource://devtools/client/shared/components/splitter/SplitBox.js");

import React, { Component } from "devtools/client/shared/vendor/react";
import {
  div,
  input,
  label,
  button,
  a,
} from "devtools/client/shared/vendor/react-dom-factories";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";
import { connect } from "devtools/client/shared/vendor/react-redux";

import actions from "../../actions/index";
import {
  getTopFrame,
  getExpressions,
  getPauseCommand,
  isMapScopesEnabled,
  getSelectedFrame,
  getSelectedSource,
  getThreads,
  getCurrentThread,
  getPauseReason,
  getShouldBreakpointsPaneOpenOnPause,
  getSkipPausing,
  shouldLogEventBreakpoints,
} from "../../selectors/index";

import AccessibleImage from "../shared/AccessibleImage";
import { prefs } from "../../utils/prefs";

import Breakpoints from "./Breakpoints/index";
import Expressions from "./Expressions";
import Frames from "./Frames/index";
import Threads from "./Threads";
import Accordion from "../shared/Accordion";
import CommandBar from "./CommandBar";
import XHRBreakpoints from "./XHRBreakpoints";
import EventListeners from "./EventListeners";
import DOMMutationBreakpoints from "./DOMMutationBreakpoints";
import WhyPaused from "./WhyPaused";

import Scopes from "./Scopes";

const classnames = require("resource://devtools/client/shared/classnames.js");

function debugBtn(onClick, type, className, tooltip) {
  return button(
    {
      onClick: onClick,
      className: `${type} ${className}`,
      key: type,
      title: tooltip,
    },
    React.createElement(AccessibleImage, {
      className: type,
      title: tooltip,
      "aria-label": tooltip,
    })
  );
}

const mdnLink =
  "https://firefox-source-docs.mozilla.org/devtools-user/debugger/using_the_debugger_map_scopes_feature/";

class SecondaryPanes extends Component {
  constructor(props) {
    super(props);

    this.state = {
      showExpressionsInput: false,
      showXHRInput: false,
    };
  }

  static get propTypes() {
    return {
      evaluateExpressionsForCurrentContext: PropTypes.func.isRequired,
      expressions: PropTypes.array.isRequired,
      hasFrames: PropTypes.bool.isRequired,
      horizontal: PropTypes.bool.isRequired,
      logEventBreakpoints: PropTypes.bool.isRequired,
      mapScopesEnabled: PropTypes.bool.isRequired,
      pauseReason: PropTypes.string.isRequired,
      shouldBreakpointsPaneOpenOnPause: PropTypes.bool.isRequired,
      thread: PropTypes.string.isRequired,
      renderWhyPauseDelay: PropTypes.number.isRequired,
      selectedFrame: PropTypes.object,
      skipPausing: PropTypes.bool.isRequired,
      source: PropTypes.object,
      toggleEventLogging: PropTypes.func.isRequired,
      resetBreakpointsPaneState: PropTypes.func.isRequired,
      toggleMapScopes: PropTypes.func.isRequired,
      threads: PropTypes.array.isRequired,
      removeAllBreakpoints: PropTypes.func.isRequired,
      removeAllXHRBreakpoints: PropTypes.func.isRequired,
    };
  }

  onExpressionAdded = () => {
    this.setState({ showExpressionsInput: false });
  };

  onXHRAdded = () => {
    this.setState({ showXHRInput: false });
  };

  watchExpressionHeaderButtons() {
    const { expressions } = this.props;
    const buttons = [];

    if (expressions.length) {
      buttons.push(
        debugBtn(
          () => {
            this.props.evaluateExpressionsForCurrentContext();
          },
          "refresh",
          "active",
          L10N.getStr("watchExpressions.refreshButton")
        )
      );
    }
    buttons.push(
      debugBtn(
        () => {
          if (!prefs.expressionsVisible) {
            this.onWatchExpressionPaneToggle(true);
          }
          this.setState({ showExpressionsInput: true });
        },
        "plus",
        "active",
        L10N.getStr("expressions.placeholder")
      )
    );
    return buttons;
  }

  xhrBreakpointsHeaderButtons() {
    return [
      debugBtn(
        () => {
          if (!prefs.xhrBreakpointsVisible) {
            this.onXHRPaneToggle(true);
          }
          this.setState({ showXHRInput: true });
        },
        "plus",
        "active",
        L10N.getStr("xhrBreakpoints.label")
      ),

      debugBtn(
        () => {
          this.props.removeAllXHRBreakpoints();
        },
        "removeAll",
        "active",
        L10N.getStr("xhrBreakpoints.removeAll.tooltip")
      ),
    ];
  }

  breakpointsHeaderButtons() {
    return [
      debugBtn(
        () => {
          this.props.removeAllBreakpoints();
        },
        "removeAll",
        "active",
        L10N.getStr("breakpointMenuItem.deleteAll")
      ),
    ];
  }

  getScopeItem() {
    return {
      header: L10N.getStr("scopes.header"),
      className: "scopes-pane",
      component: React.createElement(Scopes, null),
      opened: prefs.scopesVisible,
      buttons: this.getScopesButtons(),
      onToggle: opened => {
        prefs.scopesVisible = opened;
      },
    };
  }

  getScopesButtons() {
    const { selectedFrame, mapScopesEnabled, source } = this.props;

    if (!selectedFrame || !source?.isOriginal || source?.isPrettyPrinted) {
      return null;
    }

    return [
      div(
        {
          key: "scopes-buttons",
        },
        label(
          {
            className: "map-scopes-header",
            title: L10N.getStr("scopes.showOriginalScopesTooltip"),
            onClick: e => e.stopPropagation(),
          },
          input({
            type: "checkbox",
            checked: mapScopesEnabled ? "checked" : "",
            onChange: e => this.props.toggleMapScopes(),
          }),
          L10N.getStr("scopes.showOriginalScopes")
        ),
        a(
          {
            className: "mdn",
            target: "_blank",
            href: mdnLink,
            onClick: e => e.stopPropagation(),
            title: L10N.getStr("scopes.showOriginalScopesHelpTooltip"),
          },
          React.createElement(AccessibleImage, {
            className: "shortcuts",
          })
        )
      ),
    ];
  }

  getEventButtons() {
    const { logEventBreakpoints } = this.props;
    return [
      div(
        {
          key: "events-buttons",
        },
        label(
          {
            className: "events-header",
            title: L10N.getStr("eventlisteners.log.label"),
          },
          input({
            type: "checkbox",
            checked: logEventBreakpoints ? "checked" : "",
            onChange: e => this.props.toggleEventLogging(),
          }),
          L10N.getStr("eventlisteners.log")
        )
      ),
    ];
  }

  onWatchExpressionPaneToggle(opened) {
    prefs.expressionsVisible = opened;
  }

  getWatchItem() {
    return {
      header: L10N.getStr("watchExpressions.header"),
      id: "watch-expressions-pane",
      className: "watch-expressions-pane",
      buttons: this.watchExpressionHeaderButtons(),
      component: React.createElement(Expressions, {
        showInput: this.state.showExpressionsInput,
        onExpressionAdded: this.onExpressionAdded,
      }),
      opened: prefs.expressionsVisible,
      onToggle: this.onWatchExpressionPaneToggle,
    };
  }

  onXHRPaneToggle(opened) {
    prefs.xhrBreakpointsVisible = opened;
  }

  getXHRItem() {
    const { pauseReason } = this.props;

    return {
      header: L10N.getStr("xhrBreakpoints.header"),
      id: "xhr-breakpoints-pane",
      className: "xhr-breakpoints-pane",
      buttons: this.xhrBreakpointsHeaderButtons(),
      component: React.createElement(XHRBreakpoints, {
        showInput: this.state.showXHRInput,
        onXHRAdded: this.onXHRAdded,
      }),
      opened: prefs.xhrBreakpointsVisible || pauseReason === "XHR",
      onToggle: this.onXHRPaneToggle,
    };
  }

  getCallStackItem() {
    return {
      header: L10N.getStr("callStack.header"),
      id: "call-stack-pane",
      className: "call-stack-pane",
      component: React.createElement(Frames, {
        panel: "debugger",
      }),
      opened: prefs.callStackVisible,
      onToggle: opened => {
        prefs.callStackVisible = opened;
      },
    };
  }

  getThreadsItem() {
    return {
      header: L10N.getStr("threadsHeader"),
      id: "threads-pane",
      className: "threads-pane",
      component: React.createElement(Threads, null),
      opened: prefs.threadsVisible,
      onToggle: opened => {
        prefs.threadsVisible = opened;
      },
    };
  }

  getBreakpointsItem() {
    const { pauseReason, shouldBreakpointsPaneOpenOnPause, thread } =
      this.props;

    return {
      header: L10N.getStr("breakpoints.header"),
      id: "breakpoints-pane",
      className: "breakpoints-pane",
      buttons: this.breakpointsHeaderButtons(),
      component: React.createElement(Breakpoints),
      opened:
        prefs.breakpointsVisible ||
        (pauseReason === "breakpoint" && shouldBreakpointsPaneOpenOnPause),
      onToggle: opened => {
        prefs.breakpointsVisible = opened;
        //  one-shot flag used to force open the Breakpoints Pane only
        //  when hitting a breakpoint, but not when selecting frames etc...
        if (shouldBreakpointsPaneOpenOnPause) {
          this.props.resetBreakpointsPaneState(thread);
        }
      },
    };
  }

  getEventListenersItem() {
    const { pauseReason } = this.props;

    return {
      header: L10N.getStr("eventListenersHeader1"),
      id: "event-listeners-pane",
      className: "event-listeners-pane",
      buttons: this.getEventButtons(),
      component: React.createElement(EventListeners, null),
      opened: prefs.eventListenersVisible || pauseReason === "eventBreakpoint",
      onToggle: opened => {
        prefs.eventListenersVisible = opened;
      },
    };
  }

  getDOMMutationsItem() {
    const { pauseReason } = this.props;

    return {
      header: L10N.getStr("domMutationHeader"),
      id: "dom-mutations-pane",
      className: "dom-mutations-pane",
      buttons: [],
      component: React.createElement(DOMMutationBreakpoints, null),
      opened:
        prefs.domMutationBreakpointsVisible ||
        pauseReason === "mutationBreakpoint",
      onToggle: opened => {
        prefs.domMutationBreakpointsVisible = opened;
      },
    };
  }

  getStartItems() {
    const items = [];
    const { horizontal, hasFrames } = this.props;

    if (horizontal) {
      if (this.props.threads.length) {
        items.push(this.getThreadsItem());
      }

      items.push(this.getWatchItem());
    }

    items.push(this.getBreakpointsItem());

    if (hasFrames) {
      items.push(this.getCallStackItem());
      if (horizontal) {
        items.push(this.getScopeItem());
      }
    }

    items.push(this.getXHRItem());

    items.push(this.getEventListenersItem());

    items.push(this.getDOMMutationsItem());

    return items;
  }

  getEndItems() {
    if (this.props.horizontal) {
      return [];
    }

    const items = [];
    if (this.props.threads.length) {
      items.push(this.getThreadsItem());
    }

    items.push(this.getWatchItem());

    if (this.props.hasFrames) {
      items.push(this.getScopeItem());
    }

    return items;
  }

  getItems() {
    return [...this.getStartItems(), ...this.getEndItems()];
  }

  renderHorizontalLayout() {
    const { renderWhyPauseDelay } = this.props;
    return div(
      null,
      React.createElement(WhyPaused, {
        delay: renderWhyPauseDelay,
      }),
      React.createElement(Accordion, {
        items: this.getItems(),
      })
    );
  }

  renderVerticalLayout() {
    return React.createElement(SplitBox, {
      initialSize: "300px",
      minSize: 10,
      maxSize: "50%",
      splitterSize: 1,
      startPanel: div(
        {
          style: {
            width: "inherit",
          },
        },
        React.createElement(WhyPaused, {
          delay: this.props.renderWhyPauseDelay,
        }),
        React.createElement(Accordion, {
          items: this.getStartItems(),
        })
      ),
      endPanel: React.createElement(Accordion, {
        items: this.getEndItems(),
      }),
    });
  }

  render() {
    const { skipPausing } = this.props;
    return div(
      {
        className: "secondary-panes-wrapper",
      },
      React.createElement(CommandBar, {
        horizontal: this.props.horizontal,
      }),
      React.createElement(
        "div",
        {
          className: classnames(
            "secondary-panes",
            skipPausing && "skip-pausing"
          ),
        },
        this.props.horizontal
          ? this.renderHorizontalLayout()
          : this.renderVerticalLayout()
      )
    );
  }
}

// Checks if user is in debugging mode and adds a delay preventing
// excessive vertical 'jumpiness'
function getRenderWhyPauseDelay(state, thread) {
  const inPauseCommand = !!getPauseCommand(state, thread);

  if (!inPauseCommand) {
    return 100;
  }

  return 0;
}

const mapStateToProps = state => {
  const thread = getCurrentThread(state);
  const selectedFrame = getSelectedFrame(state, thread);
  const pauseReason = getPauseReason(state, thread);
  const shouldBreakpointsPaneOpenOnPause = getShouldBreakpointsPaneOpenOnPause(
    state,
    thread
  );

  return {
    expressions: getExpressions(state),
    hasFrames: !!getTopFrame(state, thread),
    renderWhyPauseDelay: getRenderWhyPauseDelay(state, thread),
    selectedFrame,
    mapScopesEnabled: isMapScopesEnabled(state),
    threads: getThreads(state),
    skipPausing: getSkipPausing(state),
    logEventBreakpoints: shouldLogEventBreakpoints(state),
    source: getSelectedSource(state),
    pauseReason: pauseReason?.type ?? "",
    shouldBreakpointsPaneOpenOnPause,
    thread,
  };
};

export default connect(mapStateToProps, {
  evaluateExpressionsForCurrentContext:
    actions.evaluateExpressionsForCurrentContext,
  toggleMapScopes: actions.toggleMapScopes,
  breakOnNext: actions.breakOnNext,
  toggleEventLogging: actions.toggleEventLogging,
  removeAllBreakpoints: actions.removeAllBreakpoints,
  removeAllXHRBreakpoints: actions.removeAllXHRBreakpoints,
  resetBreakpointsPaneState: actions.resetBreakpointsPaneState,
})(SecondaryPanes);
