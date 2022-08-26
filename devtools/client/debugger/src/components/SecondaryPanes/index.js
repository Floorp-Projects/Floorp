/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const SplitBox = require("devtools/client/shared/components/splitter/SplitBox");

import React, { Component } from "react";
import PropTypes from "prop-types";
import classnames from "classnames";
import { isGeneratedId } from "devtools-source-map";
import { connect } from "../../utils/connect";

import actions from "../../actions";
import {
  getTopFrame,
  getExpressions,
  getPauseCommand,
  isMapScopesEnabled,
  getSelectedFrame,
  getShouldPauseOnExceptions,
  getShouldPauseOnCaughtExceptions,
  getThreads,
  getCurrentThread,
  getThreadContext,
  getPauseReason,
  getShouldBreakpointsPaneOpenOnPause,
  getLocationSource,
  getSkipPausing,
  shouldLogEventBreakpoints,
} from "../../selectors";

import AccessibleImage from "../shared/AccessibleImage";
import { prefs, features } from "../../utils/prefs";

import Breakpoints from "./Breakpoints";
import Expressions from "./Expressions";
import Frames from "./Frames";
import Threads from "./Threads";
import Accordion from "../shared/Accordion";
import CommandBar from "./CommandBar";
import XHRBreakpoints from "./XHRBreakpoints";
import EventListeners from "./EventListeners";
import DOMMutationBreakpoints from "./DOMMutationBreakpoints";
import WhyPaused from "./WhyPaused";

import Scopes from "./Scopes";

import "./SecondaryPanes.css";

function debugBtn(onClick, type, className, tooltip) {
  return (
    <button
      onClick={onClick}
      className={`${type} ${className}`}
      key={type}
      title={tooltip}
    >
      <AccessibleImage className={type} title={tooltip} aria-label={tooltip} />
    </button>
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
      cx: PropTypes.object.isRequired,
      evaluateExpressions: PropTypes.func.isRequired,
      expressions: PropTypes.array.isRequired,
      hasFrames: PropTypes.bool.isRequired,
      horizontal: PropTypes.bool.isRequired,
      logEventBreakpoints: PropTypes.bool.isRequired,
      mapScopesEnabled: PropTypes.bool.isRequired,
      pauseOnExceptions: PropTypes.func.isRequired,
      pauseReason: PropTypes.string.isRequired,
      shouldBreakpointsPaneOpenOnPause: PropTypes.bool.isRequired,
      thread: PropTypes.string.isRequired,
      renderWhyPauseDelay: PropTypes.number.isRequired,
      selectedFrame: PropTypes.object,
      shouldPauseOnCaughtExceptions: PropTypes.bool.isRequired,
      shouldPauseOnExceptions: PropTypes.bool.isRequired,
      skipPausing: PropTypes.bool.isRequired,
      source: PropTypes.object,
      toggleEventLogging: PropTypes.func.isRequired,
      resetBreakpointsPaneState: PropTypes.func.isRequired,
      toggleMapScopes: PropTypes.func.isRequired,
      workers: PropTypes.array.isRequired,
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
          evt => {
            evt.stopPropagation();
            this.props.evaluateExpressions(this.props.cx);
          },
          "refresh",
          "active",
          L10N.getStr("watchExpressions.refreshButton")
        )
      );
    }
    buttons.push(
      debugBtn(
        evt => {
          if (prefs.expressionsVisible) {
            evt.stopPropagation();
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
        evt => {
          if (prefs.xhrBreakpointsVisible) {
            evt.stopPropagation();
          }
          this.setState({ showXHRInput: true });
        },
        "plus",
        "active",
        L10N.getStr("xhrBreakpoints.label")
      ),

      debugBtn(
        evt => {
          evt.stopPropagation();
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
        evt => {
          evt.stopPropagation();
          this.props.removeAllBreakpoints(this.props.cx);
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
      component: <Scopes />,
      opened: prefs.scopesVisible,
      buttons: this.getScopesButtons(),
      onToggle: opened => {
        prefs.scopesVisible = opened;
      },
    };
  }

  getScopesButtons() {
    const { selectedFrame, mapScopesEnabled, source } = this.props;

    if (
      !selectedFrame ||
      isGeneratedId(selectedFrame.location.sourceId) ||
      source?.isPrettyPrinted
    ) {
      return null;
    }

    return [
      <div key="scopes-buttons">
        <label
          className="map-scopes-header"
          title={L10N.getStr("scopes.mapping.label")}
          onClick={e => e.stopPropagation()}
        >
          <input
            type="checkbox"
            checked={mapScopesEnabled ? "checked" : ""}
            onChange={e => this.props.toggleMapScopes()}
          />
          {L10N.getStr("scopes.map.label")}
        </label>
        <a
          className="mdn"
          target="_blank"
          href={mdnLink}
          onClick={e => e.stopPropagation()}
          title={L10N.getStr("scopes.helpTooltip.label")}
        >
          <AccessibleImage className="shortcuts" />
        </a>
      </div>,
    ];
  }

  getEventButtons() {
    const { logEventBreakpoints } = this.props;
    return [
      <div key="events-buttons">
        <label
          className="events-header"
          title={L10N.getStr("eventlisteners.log.label")}
          onClick={e => e.stopPropagation()}
        >
          <input
            type="checkbox"
            checked={logEventBreakpoints ? "checked" : ""}
            onChange={e => this.props.toggleEventLogging()}
            onKeyDown={e => e.stopPropagation()}
          />
          {L10N.getStr("eventlisteners.log")}
        </label>
      </div>,
    ];
  }

  getWatchItem() {
    return {
      header: L10N.getStr("watchExpressions.header"),
      className: "watch-expressions-pane",
      buttons: this.watchExpressionHeaderButtons(),
      component: (
        <Expressions
          showInput={this.state.showExpressionsInput}
          onExpressionAdded={this.onExpressionAdded}
        />
      ),
      opened: prefs.expressionsVisible,
      onToggle: opened => {
        prefs.expressionsVisible = opened;
      },
    };
  }

  getXHRItem() {
    const { pauseReason } = this.props;

    return {
      header: L10N.getStr("xhrBreakpoints.header"),
      className: "xhr-breakpoints-pane",
      buttons: this.xhrBreakpointsHeaderButtons(),
      component: (
        <XHRBreakpoints
          showInput={this.state.showXHRInput}
          onXHRAdded={this.onXHRAdded}
        />
      ),
      opened: prefs.xhrBreakpointsVisible || pauseReason === "XHR",
      onToggle: opened => {
        prefs.xhrBreakpointsVisible = opened;
      },
    };
  }

  getCallStackItem() {
    return {
      header: L10N.getStr("callStack.header"),
      className: "call-stack-pane",
      component: <Frames panel="debugger" />,
      opened: prefs.callStackVisible,
      onToggle: opened => {
        prefs.callStackVisible = opened;
      },
    };
  }

  getThreadsItem() {
    return {
      header: L10N.getStr("threadsHeader"),
      className: "threads-pane",
      component: <Threads />,
      opened: prefs.workersVisible,
      onToggle: opened => {
        prefs.workersVisible = opened;
      },
    };
  }

  getBreakpointsItem() {
    const {
      shouldPauseOnExceptions,
      shouldPauseOnCaughtExceptions,
      pauseOnExceptions,
      pauseReason,
      shouldBreakpointsPaneOpenOnPause,
      thread,
    } = this.props;

    return {
      header: L10N.getStr("breakpoints.header"),
      className: "breakpoints-pane",
      buttons: this.breakpointsHeaderButtons(),
      component: (
        <Breakpoints
          shouldPauseOnExceptions={shouldPauseOnExceptions}
          shouldPauseOnCaughtExceptions={shouldPauseOnCaughtExceptions}
          pauseOnExceptions={pauseOnExceptions}
        />
      ),
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
      className: "event-listeners-pane",
      buttons: this.getEventButtons(),
      component: <EventListeners />,
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
      className: "dom-mutations-pane",
      buttons: [],
      component: <DOMMutationBreakpoints />,
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
      if (features.workers && this.props.workers.length) {
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

    if (features.xhrBreakpoints) {
      items.push(this.getXHRItem());
    }

    if (features.eventListenersBreakpoints) {
      items.push(this.getEventListenersItem());
    }

    if (features.domMutationBreakpoints) {
      items.push(this.getDOMMutationsItem());
    }

    return items;
  }

  getEndItems() {
    if (this.props.horizontal) {
      return [];
    }

    const items = [];
    if (features.workers && this.props.workers.length) {
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

    return (
      <div>
        <WhyPaused delay={renderWhyPauseDelay} />
        <Accordion items={this.getItems()} />
      </div>
    );
  }

  renderVerticalLayout() {
    return (
      <SplitBox
        initialSize="300px"
        minSize={10}
        maxSize="50%"
        splitterSize={1}
        startPanel={
          <div style={{ width: "inherit" }}>
            <WhyPaused delay={this.props.renderWhyPauseDelay} />
            <Accordion items={this.getStartItems()} />
          </div>
        }
        endPanel={<Accordion items={this.getEndItems()} />}
      />
    );
  }

  render() {
    const { skipPausing } = this.props;
    return (
      <div className="secondary-panes-wrapper">
        <CommandBar horizontal={this.props.horizontal} />
        <div
          className={classnames(
            "secondary-panes",
            skipPausing && "skip-pausing"
          )}
        >
          {this.props.horizontal
            ? this.renderHorizontalLayout()
            : this.renderVerticalLayout()}
        </div>
      </div>
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
    cx: getThreadContext(state),
    expressions: getExpressions(state),
    hasFrames: !!getTopFrame(state, thread),
    renderWhyPauseDelay: getRenderWhyPauseDelay(state, thread),
    selectedFrame,
    mapScopesEnabled: isMapScopesEnabled(state),
    shouldPauseOnExceptions: getShouldPauseOnExceptions(state),
    shouldPauseOnCaughtExceptions: getShouldPauseOnCaughtExceptions(state),
    workers: getThreads(state),
    skipPausing: getSkipPausing(state),
    logEventBreakpoints: shouldLogEventBreakpoints(state),
    source: selectedFrame && getLocationSource(state, selectedFrame.location),
    pauseReason: pauseReason?.type ?? "",
    shouldBreakpointsPaneOpenOnPause,
    thread,
  };
};

export default connect(mapStateToProps, {
  evaluateExpressions: actions.evaluateExpressions,
  pauseOnExceptions: actions.pauseOnExceptions,
  toggleMapScopes: actions.toggleMapScopes,
  breakOnNext: actions.breakOnNext,
  toggleEventLogging: actions.toggleEventLogging,
  removeAllBreakpoints: actions.removeAllBreakpoints,
  removeAllXHRBreakpoints: actions.removeAllXHRBreakpoints,
  resetBreakpointsPaneState: actions.resetBreakpointsPaneState,
})(SecondaryPanes);
