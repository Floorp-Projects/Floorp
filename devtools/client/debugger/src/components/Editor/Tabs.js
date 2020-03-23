/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { PureComponent } from "react";
import ReactDOM from "react-dom";
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
import { getFilename, isPretty, getFileURL } from "../../utils/source";
import actions from "../../actions";

import { debounce } from "lodash";
import "./Tabs.css";

import Tab from "./Tab";
import { PaneToggleButton } from "../shared/Button";
import Dropdown from "../shared/Dropdown";
import AccessibleImage from "../shared/AccessibleImage";
import CommandBar from "../SecondaryPanes/CommandBar";

import type { Source, Context } from "../../types";
import type { TabsSources } from "../../reducers/types";

type OwnProps = {|
  horizontal: boolean,
  startPanelCollapsed: boolean,
  endPanelCollapsed: boolean,
|};
type Props = {
  cx: Context,
  tabSources: TabsSources,
  selectedSource: ?Source,
  horizontal: boolean,
  startPanelCollapsed: boolean,
  endPanelCollapsed: boolean,
  moveTab: typeof actions.moveTab,
  moveTabBySourceId: typeof actions.moveTabBySourceId,
  closeTab: typeof actions.closeTab,
  togglePaneCollapse: typeof actions.togglePaneCollapse,
  showSource: typeof actions.showSource,
  selectSource: typeof actions.selectSource,
  isPaused: boolean,
};

type State = {
  dropdownShown: boolean,
  hiddenTabs: TabsSources,
};

function haveTabSourcesChanged(
  tabSources: TabsSources,
  prevTabSources: TabsSources
): boolean {
  if (tabSources.length !== prevTabSources.length) {
    return true;
  }

  for (let i = 0; i < tabSources.length; ++i) {
    if (tabSources[i].id !== prevTabSources[i].id) {
      return true;
    }
  }

  return false;
}

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
  _draggedSource: ?Source;
  _draggedSourceIndex: ?number;

  constructor(props: Props) {
    super(props);
    this.state = {
      dropdownShown: false,
      hiddenTabs: [],
    };

    this.onResize = debounce(() => {
      this.updateHiddenTabs();
    });
  }

  get draggedSource() {
    return this._draggedSource == null
      ? { url: null, id: null }
      : this._draggedSource;
  }

  set draggedSource(source: ?Source) {
    this._draggedSource = source;
  }

  get draggedSourceIndex() {
    return this._draggedSourceIndex == null ? -1 : this._draggedSourceIndex;
  }

  set draggedSourceIndex(index: ?number) {
    this._draggedSourceIndex = index;
  }

  componentDidUpdate(prevProps: Props) {
    if (
      this.props.selectedSource !== prevProps.selectedSource ||
      haveTabSourcesChanged(this.props.tabSources, prevProps.tabSources)
    ) {
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

  toggleSourcesDropdown() {
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
      <li key={source.id} onClick={onClick} title={getFileURL(source, false)}>
        <AccessibleImage
          className={`dropdown-icon ${this.getIconClass(source)}`}
        />
        <span className="dropdown-label">{filename}</span>
      </li>
    );
  };

  onTabDragStart = (source: Source, index: number) => {
    this.draggedSource = source;
    this.draggedSourceIndex = index;
  };

  onTabDragEnd = () => {
    this.draggedSource = null;
    this.draggedSourceIndex = null;
  };

  onTabDragOver = (e: any, source: Source, hoveredTabIndex: number) => {
    const { moveTabBySourceId } = this.props;
    if (hoveredTabIndex === this.draggedSourceIndex) {
      return;
    }

    const tabDOM = ReactDOM.findDOMNode(
      this.refs[`tab_${source.id}`].getWrappedInstance()
    );

    /* $FlowIgnore: tabDOM.nodeType will always be of Node.ELEMENT_NODE since it comes from a ref;
      however; the return type of findDOMNode is null | Element | Text */
    const tabDOMRect = tabDOM.getBoundingClientRect();
    const { pageX: mouseCursorX } = e;
    if (
      /* Case: the mouse cursor moves into the left half of any target tab */
      mouseCursorX - tabDOMRect.left <
      tabDOMRect.width / 2
    ) {
      // The current tab goes to the left of the target tab
      const targetTab =
        hoveredTabIndex > this.draggedSourceIndex
          ? hoveredTabIndex - 1
          : hoveredTabIndex;
      moveTabBySourceId(this.draggedSource.id, targetTab);
      this.draggedSourceIndex = targetTab;
    } else if (
      /* Case: the mouse cursor moves into the right half of any target tab */
      mouseCursorX - tabDOMRect.left >=
      tabDOMRect.width / 2
    ) {
      // The current tab goes to the right of the target tab
      const targetTab =
        hoveredTabIndex < this.draggedSourceIndex
          ? hoveredTabIndex + 1
          : hoveredTabIndex;
      moveTabBySourceId(this.draggedSource.id, targetTab);
      this.draggedSourceIndex = targetTab;
    }
  };

  renderTabs() {
    const { tabSources } = this.props;
    if (!tabSources) {
      return;
    }

    return (
      <div className="source-tabs" ref="sourceTabs">
        {tabSources.map((source, index) => {
          return (
            <Tab
              onDragStart={_ => this.onTabDragStart(source, index)}
              onDragOver={e => {
                this.onTabDragOver(e, source, index);
                e.preventDefault();
              }}
              onDragEnd={this.onTabDragEnd}
              key={index}
              source={source}
              ref={`tab_${source.id}`}
            />
          );
        })}
      </div>
    );
  }

  renderDropdown() {
    const { hiddenTabs } = this.state;
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

export default connect<Props, OwnProps, _, _, _, _>(mapStateToProps, {
  selectSource: actions.selectSource,
  moveTab: actions.moveTab,
  moveTabBySourceId: actions.moveTabBySourceId,
  closeTab: actions.closeTab,
  togglePaneCollapse: actions.togglePaneCollapse,
  showSource: actions.showSource,
})(Tabs);
