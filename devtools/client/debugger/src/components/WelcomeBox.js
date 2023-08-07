/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { Component } from "react";
import { div, p, span } from "react-dom-factories";
import PropTypes from "prop-types";

import { connect } from "../utils/connect";
import { primaryPaneTabs } from "../constants";

import actions from "../actions";
import { getPaneCollapse } from "../selectors";
import { formatKeyShortcut } from "../utils/text";

import "./WelcomeBox.css";

export class WelcomeBox extends Component {
  static get propTypes() {
    return {
      openQuickOpen: PropTypes.func.isRequired,
      setActiveSearch: PropTypes.func.isRequired,
      toggleShortcutsModal: PropTypes.func.isRequired,
      setPrimaryPaneTab: PropTypes.func.isRequired,
    };
  }

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

    return div(
      {
        className: "welcomebox",
      },
      div(
        {
          className: "alignlabel",
        },
        div(
          {
            className: "shortcutFunction",
          },
          p(
            {
              className: "welcomebox__searchSources",
              role: "button",
              tabIndex: "0",
              onClick: () => this.props.openQuickOpen(),
            },
            span(
              {
                className: "shortcutKey",
              },
              searchSourcesShortcut
            ),
            span(
              {
                className: "shortcutLabel",
              },
              searchSourcesLabel
            )
          ),
          p(
            {
              className: "welcomebox__searchProject",
              role: "button",
              tabIndex: "0",
              onClick: () => {
                this.props.setActiveSearch(primaryPaneTabs.PROJECT_SEARCH);
                this.props.setPrimaryPaneTab(primaryPaneTabs.PROJECT_SEARCH);
              },
            },
            span(
              {
                className: "shortcutKey",
              },
              searchProjectShortcut
            ),
            span(
              {
                className: "shortcutLabel",
              },
              searchProjectLabel
            )
          ),
          p(
            {
              className: "welcomebox__allShortcuts",
              role: "button",
              tabIndex: "0",
              onClick: () => this.props.toggleShortcutsModal(),
            },
            span(
              {
                className: "shortcutKey",
              },
              allShortcutsShortcut
            ),
            span(
              {
                className: "shortcutLabel",
              },
              allShortcutsLabel
            )
          )
        )
      )
    );
  }
}

const mapStateToProps = state => ({
  endPanelCollapsed: getPaneCollapse(state, "end"),
});

export default connect(mapStateToProps, {
  togglePaneCollapse: actions.togglePaneCollapse,
  setActiveSearch: actions.setActiveSearch,
  openQuickOpen: actions.openQuickOpen,
  setPrimaryPaneTab: actions.setPrimaryPaneTab,
})(WelcomeBox);
