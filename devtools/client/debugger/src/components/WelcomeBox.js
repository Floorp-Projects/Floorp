/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";

import { connect } from "../utils/connect";

import actions from "../actions";
import { getPaneCollapse } from "../selectors";
import { formatKeyShortcut } from "../utils/text";

import type { ActiveSearchType } from "../reducers/ui";

import "./WelcomeBox.css";

type Props = {
  horizontal: boolean,
  endPanelCollapsed: boolean,
  togglePaneCollapse: Function,
  setActiveSearch: (?ActiveSearchType) => any,
  openQuickOpen: typeof actions.openQuickOpen,
  toggleShortcutsModal: () => void,
};

export class WelcomeBox extends Component<Props> {
  render() {
    const searchSourcesShortcut = formatKeyShortcut(
      L10N.getStr("sources.search.key2")
    );

    const searchProjectShortcut = formatKeyShortcut(
      L10N.getStr("projectTextSearch.key")
    );

    const allShortcutsShortcut = formatKeyShortcut(
      L10N.getStr("allShortcut.key")
    );

    const allShortcutsLabel = L10N.getStr("welcome.allShortcuts");
    const searchSourcesLabel = L10N.getStr("welcome.search2").substring(2);
    const searchProjectLabel = L10N.getStr("welcome.findInFiles2").substring(2);
    const { setActiveSearch, openQuickOpen, toggleShortcutsModal } = this.props;

    return (
      <div className="welcomebox">
        <div className="alignlabel">
          <div className="shortcutFunction">
            <p
              className="welcomebox__searchSources"
              role="button"
              tabIndex="0"
              onClick={() => openQuickOpen()}
            >
              <span className="shortcutKey">{searchSourcesShortcut}</span>
              <span className="shortcutLabel">{searchSourcesLabel}</span>
            </p>
            <p
              className="welcomebox__searchProject"
              role="button"
              tabIndex="0"
              onClick={setActiveSearch.bind(null, "project")}
            >
              <span className="shortcutKey">{searchProjectShortcut}</span>
              <span className="shortcutLabel">{searchProjectLabel}</span>
            </p>
            <p
              className="welcomebox__allShortcuts"
              role="button"
              tabIndex="0"
              onClick={() => toggleShortcutsModal()}
            >
              <span className="shortcutKey">{allShortcutsShortcut}</span>
              <span className="shortcutLabel">{allShortcutsLabel}</span>
            </p>
          </div>
        </div>
      </div>
    );
  }
}

const mapStateToProps = state => ({
  endPanelCollapsed: getPaneCollapse(state, "end"),
});

export default connect(
  mapStateToProps,
  {
    togglePaneCollapse: actions.togglePaneCollapse,
    setActiveSearch: actions.setActiveSearch,
    openQuickOpen: actions.openQuickOpen,
  }
)(WelcomeBox);
