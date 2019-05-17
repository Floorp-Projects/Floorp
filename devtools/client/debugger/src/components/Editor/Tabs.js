/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { PureComponent } from "react";
import { connect } from "../../utils/connect";

import {
  getSelectedSource,
  getSourcesForTabs,
  getIsPaused,
  getCurrentThread,
  getContext,
} from "../../selectors";
import { isVisible } from "../../utils/ui";

import { getHiddenTabs } from "../../utils/tabs";
import { getFilename, isPretty } from "../../utils/source";
import actions from "../../actions";

import { debounce } from "lodash";
import "./Tabs.css";

import Tab from "./Tab";
import { PaneToggleButton } from "../shared/Button";
import Dropdown from "../shared/Dropdown";
import AccessibleImage from "../shared/AccessibleImage";
import CommandBar from "../SecondaryPanes/CommandBar";

import type { Source, Context } from "../../types";

type SourcesList = Source[];

type Props = {
  cx: Context,
  tabSources: SourcesList,
  selectedSource: ?Source,
  horizontal: boolean,
  startPanelCollapsed: boolean,
  endPanelCollapsed: boolean,
  moveTab: typeof actions.moveTab,
  closeTab: typeof actions.closeTab,
  togglePaneCollapse: typeof actions.togglePaneCollapse,
  showSource: typeof actions.showSource,
  selectSource: typeof actions.selectSource,
  isPaused: boolean,
};

type State = {
  dropdownShown: boolean,
  hiddenTabs: SourcesList,
};

class Tabs extends PureComponent<Props, State> {
  onTabContextMenu: Function;
  showContextMenu: Function;
  updateHiddenTabs: Function;
  toggleSourcesDropdown: Function;
  renderDropdownSource: Function;
  renderTabs: Function;
  renderDropDown: Function;
  renderStartPanelToggleButton: Function;
  renderEndPanelToggleButton: Function;
  onResize: Function;

  constructor(props) {
    super(props);
    this.state = {
      dropdownShown: false,
      hiddenTabs: [],
    };

    this.onResize = debounce(() => {
      this.updateHiddenTabs();
    });
  }

  componentDidUpdate(prevProps) {
    if (!(prevProps === this.props)) {
      this.updateHiddenTabs();
    }
  }

  componentDidMount() {
    window.requestIdleCallback(this.updateHiddenTabs);
    window.addEventListener("resize", this.onResize);
    window.document
      .querySelector(".editor-pane")
      .addEventListener("resizeend", this.onResize);
  }

  componentWillUnmount() {
    window.removeEventListener("resize", this.onResize);
    window.document
      .querySelector(".editor-pane")
      .removeEventListener("resizeend", this.onResize);
  }

  /*
   * Updates the hiddenSourceTabs state, by
   * finding the source tabs which are wrapped and are not on the top row.
   */
  updateHiddenTabs = () => {
    if (!this.refs.sourceTabs) {
      return;
    }
    const { selectedSource, tabSources, moveTab } = this.props;
    const sourceTabEls = this.refs.sourceTabs.children;
    const hiddenTabs = getHiddenTabs(tabSources, sourceTabEls);

    if (
      selectedSource &&
      isVisible() &&
      hiddenTabs.find(tab => tab.id == selectedSource.id)
    ) {
      return moveTab(selectedSource.url, 0);
    }

    this.setState({ hiddenTabs });
  };

  toggleSourcesDropdown(e) {
    this.setState(prevState => ({
      dropdownShown: !prevState.dropdownShown,
    }));
  }

  getIconClass(source: Source) {
    if (isPretty(source)) {
      return "prettyPrint";
    }
    if (source.isBlackBoxed) {
      return "blackBox";
    }
    return "file";
  }

  renderDropdownSource = (source: Source) => {
    const { cx, selectSource } = this.props;
    const filename = getFilename(source);

    const onClick = () => selectSource(cx, source.id);
    return (
      <li key={source.id} onClick={onClick}>
        <AccessibleImage
          className={`dropdown-icon ${this.getIconClass(source)}`}
        />
        <span className="dropdown-label">{filename}</span>
      </li>
    );
  };

  renderTabs() {
    const { tabSources } = this.props;
    if (!tabSources) {
      return;
    }

    return (
      <div className="source-tabs" ref="sourceTabs">
        {tabSources.map((source, index) => (
          <Tab key={index} source={source} />
        ))}
      </div>
    );
  }

  renderDropdown() {
    const hiddenTabs = this.state.hiddenTabs;
    if (!hiddenTabs || hiddenTabs.length == 0) {
      return null;
    }

    const Panel = <ul>{hiddenTabs.map(this.renderDropdownSource)}</ul>;
    const icon = <AccessibleImage className="more-tabs" />;

    return <Dropdown panel={Panel} icon={icon} />;
  }

  renderCommandBar() {
    const { horizontal, endPanelCollapsed, isPaused } = this.props;
    if (!endPanelCollapsed || !isPaused) {
      return;
    }

    return <CommandBar horizontal={horizontal} />;
  }

  renderStartPanelToggleButton() {
    return (
      <PaneToggleButton
        position="start"
        collapsed={this.props.startPanelCollapsed}
        handleClick={(this.props.togglePaneCollapse: any)}
      />
    );
  }

  renderEndPanelToggleButton() {
    const { horizontal, endPanelCollapsed, togglePaneCollapse } = this.props;
    if (!horizontal) {
      return;
    }

    return (
      <PaneToggleButton
        position="end"
        collapsed={endPanelCollapsed}
        handleClick={(togglePaneCollapse: any)}
        horizontal={horizontal}
      />
    );
  }

  render() {
    return (
      <div className="source-header">
        {this.renderStartPanelToggleButton()}
        {this.renderTabs()}
        {this.renderDropdown()}
        {this.renderEndPanelToggleButton()}
        {this.renderCommandBar()}
      </div>
    );
  }
}

const mapStateToProps = state => ({
  cx: getContext(state),
  selectedSource: getSelectedSource(state),
  tabSources: getSourcesForTabs(state),
  isPaused: getIsPaused(state, getCurrentThread(state)),
});

export default connect(
  mapStateToProps,
  {
    selectSource: actions.selectSource,
    moveTab: actions.moveTab,
    closeTab: actions.closeTab,
    togglePaneCollapse: actions.togglePaneCollapse,
    showSource: actions.showSource,
  }
)(Tabs);
