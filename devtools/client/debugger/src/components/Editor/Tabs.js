/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { PureComponent } from "react";
import { div, ul, li, span } from "react-dom-factories";
import PropTypes from "prop-types";
import { connect } from "../../utils/connect";

import {
  getSourceTabs,
  getSelectedSource,
  getSourcesForTabs,
  getIsPaused,
  getCurrentThread,
  getBlackBoxRanges,
} from "../../selectors";
import { isVisible } from "../../utils/ui";

import { getHiddenTabs } from "../../utils/tabs";
import { getFilename, isPretty, getFileURL } from "../../utils/source";
import actions from "../../actions";

import "./Tabs.css";

import Tab from "./Tab";
import { PaneToggleButton } from "../shared/Button";
import Dropdown from "../shared/Dropdown";
import AccessibleImage from "../shared/AccessibleImage";
import CommandBar from "../SecondaryPanes/CommandBar";

const { debounce } = require("devtools/shared/debounce");

function haveTabSourcesChanged(tabSources, prevTabSources) {
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

class Tabs extends PureComponent {
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

  static get propTypes() {
    return {
      endPanelCollapsed: PropTypes.bool.isRequired,
      horizontal: PropTypes.bool.isRequired,
      isPaused: PropTypes.bool.isRequired,
      moveTab: PropTypes.func.isRequired,
      moveTabBySourceId: PropTypes.func.isRequired,
      selectSource: PropTypes.func.isRequired,
      selectedSource: PropTypes.object,
      blackBoxRanges: PropTypes.object.isRequired,
      startPanelCollapsed: PropTypes.bool.isRequired,
      tabSources: PropTypes.array.isRequired,
      tabs: PropTypes.array.isRequired,
      togglePaneCollapse: PropTypes.func.isRequired,
    };
  }

  componentDidUpdate(prevProps) {
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
      moveTab(selectedSource.url, 0);
      return;
    }

    this.setState({ hiddenTabs });
  };

  toggleSourcesDropdown() {
    this.setState(prevState => ({
      dropdownShown: !prevState.dropdownShown,
    }));
  }

  getIconClass(source) {
    if (isPretty(source)) {
      return "prettyPrint";
    }
    if (this.props.blackBoxRanges[source.url]) {
      return "blackBox";
    }
    return "file";
  }

  renderDropdownSource = source => {
    const { selectSource } = this.props;
    const filename = getFilename(source);

    const onClick = () => selectSource(source);
    return li(
      {
        key: source.id,
        onClick: onClick,
        title: getFileURL(source, false),
      },
      React.createElement(AccessibleImage, {
        className: `dropdown-icon ${this.getIconClass(source)}`,
      }),
      span(
        {
          className: "dropdown-label",
        },
        filename
      )
    );
  };

  // Note that these three listener will be called from Tab component
  // so that e.target will be Tab's DOM (and not Tabs one).
  onTabDragStart = e => {
    this.draggedSourceId = e.target.dataset.sourceId;
    this.draggedSourceIndex = e.target.dataset.index;
  };

  onTabDragEnd = () => {
    this.draggedSourceId = null;
    this.draggedSourceIndex = -1;
  };

  onTabDragOver = e => {
    e.preventDefault();

    const hoveredTabIndex = e.target.dataset.index;
    const { moveTabBySourceId } = this.props;

    if (hoveredTabIndex === this.draggedSourceIndex) {
      return;
    }

    const tabDOMRect = e.target.getBoundingClientRect();
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
      moveTabBySourceId(this.draggedSourceId, targetTab);
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
      moveTabBySourceId(this.draggedSourceId, targetTab);
      this.draggedSourceIndex = targetTab;
    }
  };

  renderTabs() {
    const { tabs } = this.props;
    if (!tabs) {
      return null;
    }
    return div(
      {
        className: "source-tabs",
        ref: "sourceTabs",
      },
      tabs.map(({ source, sourceActor }, index) => {
        return React.createElement(Tab, {
          onDragStart: this.onTabDragStart,
          onDragOver: this.onTabDragOver,
          onDragEnd: this.onTabDragEnd,
          key: source.id + sourceActor?.id,
          index,
          source,
          sourceActor,
        });
      })
    );
  }

  renderDropdown() {
    const { hiddenTabs } = this.state;
    if (!hiddenTabs || !hiddenTabs.length) {
      return null;
    }
    const panel = ul(null, hiddenTabs.map(this.renderDropdownSource));
    const icon = React.createElement(AccessibleImage, {
      className: "more-tabs",
    });
    return React.createElement(Dropdown, {
      panel,
      icon,
    });
  }

  renderCommandBar() {
    const { horizontal, endPanelCollapsed, isPaused } = this.props;
    if (!endPanelCollapsed || !isPaused) {
      return null;
    }
    return React.createElement(CommandBar, {
      horizontal,
    });
  }

  renderStartPanelToggleButton() {
    return React.createElement(PaneToggleButton, {
      position: "start",
      collapsed: this.props.startPanelCollapsed,
      handleClick: this.props.togglePaneCollapse,
    });
  }

  renderEndPanelToggleButton() {
    const { horizontal, endPanelCollapsed, togglePaneCollapse } = this.props;
    if (!horizontal) {
      return null;
    }
    return React.createElement(PaneToggleButton, {
      position: "end",
      collapsed: endPanelCollapsed,
      handleClick: togglePaneCollapse,
      horizontal,
    });
  }

  render() {
    return div(
      {
        className: "source-header",
      },
      this.renderStartPanelToggleButton(),
      this.renderTabs(),
      this.renderDropdown(),
      this.renderEndPanelToggleButton(),
      this.renderCommandBar()
    );
  }
}

const mapStateToProps = state => {
  return {
    selectedSource: getSelectedSource(state),
    tabSources: getSourcesForTabs(state),
    tabs: getSourceTabs(state),
    blackBoxRanges: getBlackBoxRanges(state),
    isPaused: getIsPaused(state, getCurrentThread(state)),
  };
};

export default connect(mapStateToProps, {
  selectSource: actions.selectSource,
  moveTab: actions.moveTab,
  moveTabBySourceId: actions.moveTabBySourceId,
  closeTab: actions.closeTab,
  togglePaneCollapse: actions.togglePaneCollapse,
  showSource: actions.showSource,
})(Tabs);
