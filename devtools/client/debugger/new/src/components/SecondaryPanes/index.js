/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import { connect } from "react-redux";
import { List } from "immutable";

import actions from "../../actions";
import {
  getTopFrame,
  getBreakpoints,
  getBreakpointsDisabled,
  getBreakpointsLoading,
  getExpressions,
  getIsWaitingOnBreak,
  getShouldPauseOnExceptions,
  getShouldPauseOnCaughtExceptions,
  getWorkers,
  getExtra
} from "../../selectors";

import Svg from "../shared/Svg";
import { prefs, features } from "../../utils/prefs";

import Breakpoints from "./Breakpoints";
import Expressions from "./Expressions";
import SplitBox from "devtools-splitter";
import Frames from "./Frames";
import EventListeners from "./EventListeners";
import Workers from "./Workers";
import Accordion from "../shared/Accordion";
import CommandBar from "./CommandBar";
import UtilsBar from "./UtilsBar";
import FrameworkComponent from "./FrameworkComponent";
import XHRBreakpoints from "./XHRBreakpoints";

import Scopes from "./Scopes";

import "./SecondaryPanes.css";

import type { Expression } from "../../types";
import type { WorkersList } from "../../reducers/types";

type AccordionPaneItem = {
  header: string,
  component: any,
  opened?: boolean,
  onToggle?: () => void,
  shouldOpen?: () => boolean,
  buttons?: any
};

function debugBtn(onClick, type, className, tooltip) {
  return (
    <button
      onClick={onClick}
      className={`${type} ${className}`}
      key={type}
      title={tooltip}
    >
      <Svg name={type} title={tooltip} aria-label={tooltip} />
    </button>
  );
}

type State = {
  showExpressionsInput: boolean,
  showXHRInput: boolean
};

type Props = {
  expressions: List<Expression>,
  extra: Object,
  hasFrames: boolean,
  horizontal: boolean,
  breakpoints: Object,
  breakpointsDisabled: boolean,
  breakpointsLoading: boolean,
  isWaitingOnBreak: boolean,
  shouldPauseOnExceptions: boolean,
  shouldPauseOnCaughtExceptions: boolean,
  workers: WorkersList,
  toggleAllBreakpoints: Function,
  toggleShortcutsModal: Function,
  evaluateExpressions: Function,
  pauseOnExceptions: (boolean, boolean) => void,
  breakOnNext: () => void
};

class SecondaryPanes extends Component<Props, State> {
  constructor(props: Props) {
    super(props);

    this.state = {
      showExpressionsInput: false,
      showXHRInput: false
    };
  }

  onExpressionAdded = () => {
    this.setState({ showExpressionsInput: false });
  };

  onXHRAdded = () => {
    this.setState({ showXHRInput: false });
  };

  renderBreakpointsToggle() {
    const {
      toggleAllBreakpoints,
      breakpoints,
      breakpointsDisabled,
      breakpointsLoading
    } = this.props;
    const isIndeterminate =
      !breakpointsDisabled && breakpoints.some(x => x.disabled);

    if (features.skipPausing || breakpoints.size == 0) {
      return null;
    }

    const inputProps = {
      type: "checkbox",
      "aria-label": breakpointsDisabled
        ? L10N.getStr("breakpoints.enable")
        : L10N.getStr("breakpoints.disable"),
      className: "breakpoints-toggle",
      disabled: breakpointsLoading,
      key: "breakpoints-toggle",
      onChange: e => {
        e.stopPropagation();
        toggleAllBreakpoints(!breakpointsDisabled);
      },
      onClick: e => e.stopPropagation(),
      checked: !breakpointsDisabled && !isIndeterminate,
      ref: input => {
        if (input) {
          input.indeterminate = isIndeterminate;
        }
      },
      title: breakpointsDisabled
        ? L10N.getStr("breakpoints.enable")
        : L10N.getStr("breakpoints.disable")
    };

    return <input {...inputProps} />;
  }

  watchExpressionHeaderButtons() {
    const { expressions } = this.props;

    const buttons = [];

    if (expressions.size) {
      buttons.push(
        debugBtn(
          evt => {
            evt.stopPropagation();
            this.props.evaluateExpressions();
          },
          "refresh",
          "refresh",
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
        "plus",
        L10N.getStr("expressions.placeholder")
      )
    );

    return buttons;
  }

  xhrBreakpointsHeaderButtons() {
    const buttons = [];

    buttons.push(
      debugBtn(
        evt => {
          if (prefs.expressionsVisible) {
            evt.stopPropagation();
          }
          this.setState({ showXHRInput: true });
        },
        "plus",
        "plus",
        L10N.getStr("xhrBreakpoints.placeholder")
      )
    );

    return buttons;
  }

  getScopeItem(): AccordionPaneItem {
    return {
      header: L10N.getStr("scopes.header"),
      className: "scopes-pane",
      component: <Scopes />,
      opened: prefs.scopesVisible,
      onToggle: opened => {
        prefs.scopesVisible = opened;
      }
    };
  }

  getComponentItem() {
    const {
      extra: { react }
    } = this.props;

    return {
      header: react.displayName,
      className: "component-pane",
      component: <FrameworkComponent />,
      opened: prefs.componentVisible,
      onToggle: opened => {
        prefs.componentVisible = opened;
      }
    };
  }

  getWatchItem(): AccordionPaneItem {
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
      }
    };
  }

  getXHRItem(): AccordionPaneItem {
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
      opened: prefs.xhrBreakpointsVisible,
      onToggle: opened => {
        prefs.xhrBreakpointsVisible = opened;
      }
    };
  }

  getCallStackItem(): AccordionPaneItem {
    return {
      header: L10N.getStr("callStack.header"),
      className: "call-stack-pane",
      component: <Frames />,
      opened: prefs.callStackVisible,
      onToggle: opened => {
        prefs.callStackVisible = opened;
      }
    };
  }

  getWorkersItem(): AccordionPaneItem {
    return {
      header: L10N.getStr("workersHeader"),
      className: "workers-pane",
      component: <Workers />,
      opened: prefs.workersVisible,
      onToggle: opened => {
        prefs.workersVisible = opened;
      }
    };
  }

  getBreakpointsItem(): AccordionPaneItem {
    const {
      shouldPauseOnExceptions,
      shouldPauseOnCaughtExceptions,
      pauseOnExceptions
    } = this.props;

    return {
      header: L10N.getStr("breakpoints.header"),
      className: "breakpoints-pane",
      buttons: [this.renderBreakpointsToggle()],
      component: (
        <Breakpoints
          shouldPauseOnExceptions={shouldPauseOnExceptions}
          shouldPauseOnCaughtExceptions={shouldPauseOnCaughtExceptions}
          pauseOnExceptions={pauseOnExceptions}
        />
      ),
      opened: prefs.breakpointsVisible,
      onToggle: opened => {
        prefs.breakpointsVisible = opened;
      }
    };
  }

  getStartItems() {
    const { extra, workers } = this.props;

    const items: Array<AccordionPaneItem> = [];
    if (this.props.horizontal) {
      if (features.workers && workers.size > 0) {
        items.push(this.getWorkersItem());
      }

      items.push(this.getWatchItem());
    }

    items.push(this.getBreakpointsItem());

    if (this.props.hasFrames) {
      items.push(this.getCallStackItem());

      if (this.props.horizontal) {
        if (features.componentPane && extra && extra.react) {
          items.push(this.getComponentItem());
        }

        items.push(this.getScopeItem());
      }
    }

    if (features.xhrBreakpoints) {
      items.push(this.getXHRItem());
    }

    if (features.eventListeners) {
      items.push({
        header: L10N.getStr("eventListenersHeader"),
        className: "event-listeners-pane",
        component: <EventListeners />
      });
    }

    return items.filter(item => item);
  }

  renderHorizontalLayout() {
    return <Accordion items={this.getItems()} />;
  }

  getEndItems() {
    const { extra, workers } = this.props;

    let items: Array<AccordionPaneItem> = [];

    if (this.props.horizontal) {
      return [];
    }

    if (features.workers && workers.size > 0) {
      items.push(this.getWorkersItem());
    }

    items.push(this.getWatchItem());

    if (features.componentPane && extra && extra.react) {
      items.push(this.getComponentItem());
    }

    if (this.props.hasFrames) {
      items = [...items, this.getScopeItem()];
    }

    return items;
  }

  getItems() {
    return [...this.getStartItems(), ...this.getEndItems()];
  }

  renderVerticalLayout() {
    return (
      <SplitBox
        initialSize="300px"
        minSize={10}
        maxSize="50%"
        splitterSize={1}
        startPanel={<Accordion items={this.getStartItems()} />}
        endPanel={<Accordion items={this.getEndItems()} />}
      />
    );
  }

  renderUtilsBar() {
    if (!features.shortcuts) {
      return;
    }

    return (
      <UtilsBar
        horizontal={this.props.horizontal}
        toggleShortcutsModal={this.props.toggleShortcutsModal}
      />
    );
  }

  render() {
    return (
      <div className="secondary-panes-wrapper">
        <CommandBar horizontal={this.props.horizontal} />
        <div className="secondary-panes">
          {this.props.horizontal
            ? this.renderHorizontalLayout()
            : this.renderVerticalLayout()}
        </div>
        {this.renderUtilsBar()}
      </div>
    );
  }
}

const mapStateToProps = state => ({
  expressions: getExpressions(state),
  extra: getExtra(state),
  hasFrames: !!getTopFrame(state),
  breakpoints: getBreakpoints(state),
  breakpointsDisabled: getBreakpointsDisabled(state),
  breakpointsLoading: getBreakpointsLoading(state),
  isWaitingOnBreak: getIsWaitingOnBreak(state),
  shouldPauseOnExceptions: getShouldPauseOnExceptions(state),
  shouldPauseOnCaughtExceptions: getShouldPauseOnCaughtExceptions(state),
  workers: getWorkers(state)
});

export default connect(
  mapStateToProps,
  {
    toggleAllBreakpoints: actions.toggleAllBreakpoints,
    toggleShortcutsModal: actions.toggleShortcutsModal,
    evaluateExpressions: actions.evaluateExpressions,
    pauseOnExceptions: actions.pauseOnExceptions,
    breakOnNext: actions.breakOnNext
  }
)(SecondaryPanes);
