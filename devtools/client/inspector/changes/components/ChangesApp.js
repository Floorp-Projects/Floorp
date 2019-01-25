/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const CSSDeclaration = createFactory(require("./CSSDeclaration"));
const { getChangesTree } = require("../selectors/changes");
const { getSourceForDisplay } = require("../utils/changes-utils");
const { getStr } = require("../utils/l10n");

class ChangesApp extends PureComponent {
  static get propTypes() {
    return {
      // Nested CSS rule tree structure of CSS changes grouped by source (stylesheet)
      changesTree: PropTypes.object.isRequired,
      // Event handler for "contextmenu" event
      onContextMenu: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
  }

  renderDeclarations(remove = [], add = []) {
    const removals = remove
      // Sorting changed declarations in the order they appear in the Rules view.
      .sort((a, b) => a.index > b.index)
      .map(({property, value, index}) => {
        return CSSDeclaration({
          key: "remove-" + property + index,
          className: "level diff-remove",
          marker: getDiffMarker("diff-remove"),
          property,
          value,
        });
      });

    const additions = add
      // Sorting changed declarations in the order they appear in the Rules view.
      .sort((a, b) => a.index > b.index)
      .map(({property, value, index}) => {
        return CSSDeclaration({
          key: "add-" + property + index,
          className: "level diff-add",
          marker: getDiffMarker("diff-add"),
          property,
          value,
        });
      });

    return [removals, additions];
  }

  renderRule(ruleId, rule, level = 0) {
    const selector = rule.selector;

    let diffClass = "";
    if (rule.changeType === "rule-add") {
      diffClass = "diff-add";
    } else if (rule.changeType === "rule-remove") {
      diffClass = "diff-remove";
    }

    return dom.div(
      {
        key: ruleId,
        className: "rule devtools-monospace",
        style: {
          "--diff-level": level,
        },
      },
      dom.div(
        {
          className: `level selector ${diffClass}`,
          title: selector,
        },
        getDiffMarker(diffClass),
        selector,
        dom.span({ className: "bracket-open" }, " {")
      ),
      // Render any nested child rules if they exist.
      rule.children.map(childRule => {
        return this.renderRule(childRule.ruleId, childRule, level + 1);
      }),
      // Render any changed CSS declarations.
      this.renderDeclarations(rule.remove, rule.add),
      dom.div({ className: `level bracket-close ${diffClass}` },
        getDiffMarker(diffClass),
        "}"
      )
    );
  }

  renderDiff(changes = {}) {
    // Render groups of style sources: stylesheets and element style attributes.
    return Object.entries(changes).map(([sourceId, source]) => {
      const path = getSourceForDisplay(source);
      const { href, rules, isFramed } = source;

      return dom.div(
        {
          key: sourceId,
          className: "source",
        },
        dom.div(
          {
            className: "href",
            title: href,
          },
          dom.span({}, path),
          isFramed && this.renderFrameBadge(href)
        ),
        // Render changed rules within this source.
        Object.entries(rules).map(([ruleId, rule]) => {
          return this.renderRule(ruleId, rule);
        })
      );
    });
  }

  renderFrameBadge(href = "") {
    return dom.span(
      {
        className: "inspector-badge",
        title: href,
      },
      getStr("changes.iframeLabel")
    );
  }

  renderEmptyState() {
    return dom.div({ className: "devtools-sidepanel-no-result" },
      dom.p({}, getStr("changes.noChanges")),
      dom.p({}, getStr("changes.noChangesDescription"))
    );
  }

  render() {
    const hasChanges = Object.keys(this.props.changesTree).length > 0;
    return dom.div(
      {
        className: "theme-sidebar inspector-tabpanel",
        id: "sidebar-panel-changes",
        onContextMenu: this.props.onContextMenu,
      },
      !hasChanges && this.renderEmptyState(),
      hasChanges && this.renderDiff(this.props.changesTree)
    );
  }
}

/**
 * Get a React element with text content of either a plus or minus sign according to
 * the given CSS class name used to mark added or removed lines in the changes diff view.
 * This is used as a diff line maker that can be copied over as text with the rest of the
 * content. CSS pseudo-elements are not part of the document flow and cannot be copied.
 *
 * @param  {String} className
 *         One of "diff-add" or "diff-remove"
 * @return {Component|null}
 *         Returns null if the given className isn't recognized. React handles it.
 */
function getDiffMarker(className) {
  let marker = null;
  switch (className) {
    case "diff-add":
      marker = dom.span({ className: "diff-marker" }, "+ ");
      break;
    case "diff-remove":
      marker = dom.span({ className: "diff-marker" }, "- ");
      break;
  }
  return marker;
}

const mapStateToProps = state => {
  return {
    changesTree: getChangesTree(state.changes),
  };
};

module.exports = connect(mapStateToProps)(ChangesApp);
