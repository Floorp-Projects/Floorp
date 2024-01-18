/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import actions from "../../actions/index";
import { getSelectedPrimaryPaneTab } from "../../selectors/index";
import { prefs } from "../../utils/prefs";
import { connect } from "devtools/client/shared/vendor/react-redux";
import { primaryPaneTabs } from "../../constants";

import Outline from "./Outline";
import SourcesTree from "./SourcesTree";
import ProjectSearch from "./ProjectSearch";

const {
  TabPanel,
  Tabs,
} = require("resource://devtools/client/shared/components/tabs/Tabs.js");

const tabs = [
  primaryPaneTabs.SOURCES,
  primaryPaneTabs.OUTLINE,
  primaryPaneTabs.PROJECT_SEARCH,
];

class PrimaryPanes extends Component {
  constructor(props) {
    super(props);

    this.state = {
      alphabetizeOutline: prefs.alphabetizeOutline,
    };
  }

  static get propTypes() {
    return {
      projectRootName: PropTypes.string.isRequired,
      selectedTab: PropTypes.oneOf(tabs).isRequired,
      setPrimaryPaneTab: PropTypes.func.isRequired,
      setActiveSearch: PropTypes.func.isRequired,
      closeActiveSearch: PropTypes.func.isRequired,
    };
  }

  onAlphabetizeClick = () => {
    const alphabetizeOutline = !prefs.alphabetizeOutline;
    prefs.alphabetizeOutline = alphabetizeOutline;
    this.setState({ alphabetizeOutline });
  };

  onActivateTab = index => {
    const tab = tabs.at(index);
    this.props.setPrimaryPaneTab(tab);
    if (tab == primaryPaneTabs.PROJECT_SEARCH) {
      this.props.setActiveSearch(tab);
    } else {
      this.props.closeActiveSearch();
    }
  };

  render() {
    const { selectedTab } = this.props;
    return React.createElement(
      "aside",
      {
        className: "tab-panel sources-panel",
      },
      React.createElement(
        Tabs,
        {
          activeTab: tabs.indexOf(selectedTab),
          onAfterChange: this.onActivateTab,
        },
        React.createElement(
          TabPanel,
          {
            id: "sources-tab",
            key: `sources-tab${
              selectedTab === primaryPaneTabs.SOURCES ? "-selected" : ""
            }`,
            className: "tab sources-tab",
            title: L10N.getStr("sources.header"),
          },
          React.createElement(SourcesTree, null)
        ),
        React.createElement(
          TabPanel,
          {
            id: "outline-tab",
            key: `outline-tab${
              selectedTab === primaryPaneTabs.OUTLINE ? "-selected" : ""
            }`,
            className: "tab outline-tab",
            title: L10N.getStr("outline.header"),
          },
          React.createElement(Outline, {
            alphabetizeOutline: this.state.alphabetizeOutline,
            onAlphabetizeClick: this.onAlphabetizeClick,
          })
        ),
        React.createElement(
          TabPanel,
          {
            id: "search-tab",
            key: `search-tab${
              selectedTab === primaryPaneTabs.PROJECT_SEARCH ? "-selected" : ""
            }`,
            className: "tab search-tab",
            title: L10N.getStr("search.header"),
          },
          React.createElement(ProjectSearch, null)
        )
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    selectedTab: getSelectedPrimaryPaneTab(state),
  };
};

const connector = connect(mapStateToProps, {
  setPrimaryPaneTab: actions.setPrimaryPaneTab,
  setActiveSearch: actions.setActiveSearch,
  closeActiveSearch: actions.closeActiveSearch,
});

export default connector(PrimaryPanes);
