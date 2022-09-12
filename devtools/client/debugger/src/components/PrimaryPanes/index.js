/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import classnames from "classnames";
import { Tab, Tabs, TabList, TabPanels } from "react-aria-components/src/tabs";

import actions from "../../actions";
import {
  getProjectDirectoryRootName,
  getSelectedPrimaryPaneTab,
  getContext,
} from "../../selectors";
import { features, prefs } from "../../utils/prefs";
import { connect } from "../../utils/connect";
import { formatKeyShortcut } from "../../utils/text";

import Outline from "./Outline";
import SourcesTree from "./SourcesTree";
import AccessibleImage from "../shared/AccessibleImage";

import "./Sources.css";

class PrimaryPanes extends Component {
  constructor(props) {
    super(props);

    this.state = {
      alphabetizeOutline: prefs.alphabetizeOutline,
    };
  }

  static get propTypes() {
    return {
      clearProjectDirectoryRoot: PropTypes.func.isRequired,
      cx: PropTypes.object.isRequired,
      projectRootName: PropTypes.string.isRequired,
      selectedTab: PropTypes.oneOf(["sources", "outline"]).isRequired,
      setPrimaryPaneTab: PropTypes.func.isRequired,
    };
  }

  showPane = selectedPane => {
    this.props.setPrimaryPaneTab(selectedPane);
  };

  onAlphabetizeClick = () => {
    const alphabetizeOutline = !prefs.alphabetizeOutline;
    prefs.alphabetizeOutline = alphabetizeOutline;
    this.setState({ alphabetizeOutline });
  };

  onActivateTab = index => {
    if (index === 0) {
      this.showPane("sources");
    } else {
      this.showPane("outline");
    }
  };

  renderOutlineTabs() {
    if (!features.outline) {
      return null;
    }

    const sources = formatKeyShortcut(L10N.getStr("sources.header"));
    const outline = formatKeyShortcut(L10N.getStr("outline.header"));
    const isSources = this.props.selectedTab === "sources";
    const isOutline = this.props.selectedTab === "outline";

    return [
      <Tab
        className={classnames("tab sources-tab", { active: isSources })}
        key="sources-tab"
      >
        {sources}
      </Tab>,
      <Tab
        className={classnames("tab outline-tab", { active: isOutline })}
        key="outline-tab"
      >
        {outline}
      </Tab>,
    ];
  }

  renderProjectRootHeader() {
    const { cx, projectRootName } = this.props;

    if (!projectRootName) {
      return null;
    }

    return (
      <div key="root" className="sources-clear-root-container">
        <button
          className="sources-clear-root"
          onClick={() => this.props.clearProjectDirectoryRoot(cx)}
          title={L10N.getStr("removeDirectoryRoot.label")}
        >
          <AccessibleImage className="home" />
          <AccessibleImage className="breadcrumb" />
          <span className="sources-clear-root-label">{projectRootName}</span>
        </button>
      </div>
    );
  }

  renderThreadSources() {
    return <SourcesTree />;
  }

  render() {
    const { selectedTab, projectRootName } = this.props;
    const activeIndex = selectedTab === "sources" ? 0 : 1;

    return (
      <Tabs
        activeIndex={activeIndex}
        className="sources-panel"
        onActivateTab={this.onActivateTab}
      >
        <TabList className="source-outline-tabs">
          {this.renderOutlineTabs()}
        </TabList>
        <TabPanels
          className={classnames("source-outline-panel", {
            "has-root": projectRootName,
          })}
          hasFocusableContent
        >
          <div className="threads-list">
            {this.renderProjectRootHeader()}
            {this.renderThreadSources()}
          </div>
          <Outline
            alphabetizeOutline={this.state.alphabetizeOutline}
            onAlphabetizeClick={this.onAlphabetizeClick}
          />
        </TabPanels>
      </Tabs>
    );
  }
}

const mapStateToProps = state => {
  return {
    cx: getContext(state),
    selectedTab: getSelectedPrimaryPaneTab(state),
    projectRootName: getProjectDirectoryRootName(state),
  };
};

const connector = connect(mapStateToProps, {
  setPrimaryPaneTab: actions.setPrimaryPaneTab,
  setActiveSearch: actions.setActiveSearch,
  closeActiveSearch: actions.closeActiveSearch,
  clearProjectDirectoryRoot: actions.clearProjectDirectoryRoot,
});

export default connector(PrimaryPanes);
