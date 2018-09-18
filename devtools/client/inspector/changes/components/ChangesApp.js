/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

class ChangesApp extends PureComponent {
  static get propTypes() {
    return {
      changes: PropTypes.object.isRequired
    };
  }

  renderMutations(remove = {}, add = {}) {
    const removals = Object.entries(remove).map(([prop, value]) => {
      return dom.div(
        { className: "line diff-remove"},
        `${prop}: ${value};`
      );
    });

    const additions = Object.entries(add).map(([prop, value]) => {
      return dom.div(
        { className: "line diff-add"},
        `${prop}: ${value};`
      );
    });

    return [removals, additions];
  }

  renderSelectors(selectors = {}) {
    return Object.keys(selectors).map(sel => {
      return dom.details(
        { className: "selector", open: true },
        dom.summary(
          {
            title: sel,
          },
          sel),
        this.renderMutations(selectors[sel].remove, selectors[sel].add)
      );
    });
  }

  renderDiff(diff = {}) {
    // Render groups of style sources: stylesheets, embedded styles and inline styles
    return Object.keys(diff).map(href => {
      return dom.details(
        { className: "source", open: true },
        dom.summary(
          {
            title: href,
          },
          href),
        // Render groups of selectors
        this.renderSelectors(diff[href])
      );
    });
  }

  render() {
    return dom.div(
      {
        className: "theme-sidebar inspector-tabpanel",
        id: "sidebar-panel-changes"
      },
      this.renderDiff(this.props.changes.diff)
    );
  }
}

module.exports = connect(state => state)(ChangesApp);
