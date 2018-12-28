/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import { connect } from "../../utils/connect";
import { Tab, Tabs, TabList, TabPanels } from "react-aria-components/src/tabs";
import { formatKeyShortcut } from "../../utils/text";
import actions from "../../actions";
import {
  getRelativeSources,
  getActiveSearch,
  getSelectedPrimaryPaneTab,
  getWorkerDisplayName
} from "../../selectors";
import { features, prefs } from "../../utils/prefs";
import "./Sources.css";
import classnames from "classnames";

import Outline from "./Outline";
import SourcesTree from "./SourcesTree";

import type { SourcesMapByThread } from "../../reducers/types";
import type { SelectedPrimaryPaneTabType } from "../../selectors";

type State = {
  alphabetizeOutline: boolean
};

type Props = {
  selectedTab: SelectedPrimaryPaneTabType,
  sources: SourcesMapByThread,
  horizontal: boolean,
  sourceSearchOn: boolean,
  setPrimaryPaneTab: typeof actions.setPrimaryPaneTab,
  setActiveSearch: typeof actions.setActiveSearch,
  closeActiveSearch: typeof actions.closeActiveSearch,
  getWorkerDisplayName: string => string
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

  renderThreadSources() {
    const threads = Object.getOwnPropertyNames(this.props.sources);
    threads.sort(
      (a, b) =>
        this.props.getWorkerDisplayName(a) > this.props.getWorkerDisplayName(b)
          ? 1
          : -1
    );
    return threads.map(thread => <SourcesTree thread={thread} key={thread} />);
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
          <div>{this.renderThreadSources()}</div>
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
  sources: getRelativeSources(state),
  sourceSearchOn: getActiveSearch(state) === "source",
  getWorkerDisplayName: thread => getWorkerDisplayName(state, thread)
});

const connector = connect(
  mapStateToProps,
  {
    setPrimaryPaneTab: actions.setPrimaryPaneTab,
    setActiveSearch: actions.setActiveSearch,
    closeActiveSearch: actions.closeActiveSearch
  }
);

export default connector(PrimaryPanes);
