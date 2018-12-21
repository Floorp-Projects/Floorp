/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import { connect } from "react-redux";
import { Tab, Tabs, TabList, TabPanels } from "react-aria-components/src/tabs";
import { formatKeyShortcut } from "../../utils/text";
import actions from "../../actions";
import {
  getSources,
  getActiveSearch,
  getSelectedPrimaryPaneTab
} from "../../selectors";
import { features, prefs } from "../../utils/prefs";
import "./Sources.css";
import classnames from "classnames";

import Outline from "./Outline";
import SourcesTree from "./SourcesTree";

import type { SourcesMap } from "../../reducers/types";
import type { SelectedPrimaryPaneTabType } from "../../reducers/ui";

type State = {
  alphabetizeOutline: boolean
};

type Props = {
  selectedTab: string,
  sources: SourcesMap,
  horizontal: boolean,
  sourceSearchOn: boolean,
  setPrimaryPaneTab: typeof actions.setPrimaryPaneTab,
  setActiveSearch: typeof actions.setActiveSearch,
  closeActiveSearch: typeof actions.closeActiveSearch
};

class PrimaryPanes extends Component<Props, State> {
  constructor(props: Props) {
    super(props);

    this.state = {
      alphabetizeOutline: prefs.alphabetizeOutline
    };
  }

  showPane = (selectedPane: SelectedPrimaryPaneTabType) => {
    this.props.setPrimaryPaneTab(selectedPane);
  };

  onAlphabetizeClick = () => {
    const alphabetizeOutline = !prefs.alphabetizeOutline;
    prefs.alphabetizeOutline = alphabetizeOutline;
    this.setState({ alphabetizeOutline });
  };

  onActivateTab = (index: number) => {
    if (index === 0) {
      this.showPane("sources");
    } else {
      this.showPane("outline");
    }
  };

  renderOutlineTabs() {
    if (!features.outline) {
      return;
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
      </Tab>
    ];
  }

  render() {
    const { selectedTab } = this.props;
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
        <TabPanels className="source-outline-panel" hasFocusableContent>
          <SourcesTree />
          <Outline
            alphabetizeOutline={this.state.alphabetizeOutline}
            onAlphabetizeClick={this.onAlphabetizeClick}
          />
        </TabPanels>
      </Tabs>
    );
  }
}

const mapStateToProps = state => ({
  selectedTab: getSelectedPrimaryPaneTab(state),
  sources: getSources(state),
  sourceSearchOn: getActiveSearch(state) === "source"
});

export default connect(
  mapStateToProps,
  {
    setPrimaryPaneTab: actions.setPrimaryPaneTab,
    setActiveSearch: actions.setActiveSearch,
    closeActiveSearch: actions.closeActiveSearch
  }
)(PrimaryPanes);
