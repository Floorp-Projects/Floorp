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
      // Event handler for "copy" event
      onCopy: PropTypes.func.isRequired,
      // Event handler for click on "Copy All Changes" button
      onCopyAllChanges: PropTypes.func.isRequired,
      // Event handler for click on "Copy Rule" button
      onCopyRule: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
  }

  renderCopyAllChangesButton() {
    const button = dom.button(
      {
        className: "changes__copy-all-changes-button",
        onClick: e => {
          e.stopPropagation();
          this.props.onCopyAllChanges();
        },
        title: getStr("changes.contextmenu.copyAllChangesDescription"),
      },
      getStr("changes.contextmenu.copyAllChanges")
    );

    return dom.div({ className: "changes__toolbar" }, button);
  }

  renderCopyButton(ruleId) {
    return dom.button(
      {
        className: "changes__copy-rule-button",
        onClick: e => {
          e.stopPropagation();
          this.props.onCopyRule(ruleId);
        },
        title: getStr("changes.contextmenu.copyRuleDescription"),
      },
      getStr("changes.contextmenu.copyRule")
    );
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
    const diffClass = rule.isNew ? "diff-add" : "";
    return dom.div(
      {
        key: ruleId,
        className: "changes__rule devtools-monospace",
        "data-rule-id": ruleId,
        style: {
          "--diff-level": level,
        },
      },
      this.renderSelectors(rule.selectors, rule.isNew),
      this.renderCopyButton(ruleId),
      // Render any nested child rules if they exist.
      rule.children.map(childRule => {
        return this.renderRule(childRule.ruleId, childRule, level + 1);
      }),
      // Render any changed CSS declarations.
      this.renderDeclarations(rule.remove, rule.add),
      // Render the closing bracket with a diff marker if necessary.
      dom.div({ className: `level ${diffClass}` }, getDiffMarker(diffClass), "}")
    );
  }

  /**
   * Return an array of React elements for the rule's selector.
   *
   * @param  {Array} selectors
   *         List of strings as versions of this rule's selector over time.
   * @param  {Boolean} isNewRule
   *         Whether the rule was created at runtime.
   * @return {Array}
   */
  renderSelectors(selectors, isNewRule) {
    const selectorDiffClassMap = new Map();

    // The selectors array has just one item if it hasn't changed. Render it as-is.
    // If the rule was created at runtime, mark the single selector as added.
    // If it has two or more items, the first item was the original selector (mark as
    // removed) and the last item is the current selector (mark as added).
    if (selectors.length === 1) {
      selectorDiffClassMap.set(selectors[0], isNewRule ? "diff-add" : "");
    } else if (selectors.length >= 2) {
      selectorDiffClassMap.set(selectors[0], "diff-remove");
      selectorDiffClassMap.set(selectors[selectors.length - 1], "diff-add");
    }

    const elements = [];

    for (const [selector, diffClass] of selectorDiffClassMap) {
      elements.push(dom.div(
        {
          key: selector,
          className: `level changes__selector ${diffClass}`,
          title: selector,
        },
        getDiffMarker(diffClass),
        selector,
        dom.span({}, " {")
      ));
    }

    return elements;
  }

  renderDiff(changes = {}) {
    // Render groups of style sources: stylesheets and element style attributes.
    return Object.entries(changes).map(([sourceId, source]) => {
      const path = getSourceForDisplay(source);
      const { href, rules, isFramed } = source;

      return dom.div(
        {
          key: sourceId,
          "data-source-id": sourceId,
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
        onCopy: this.props.onCopy,
      },
      !hasChanges && this.renderEmptyState(),
      hasChanges && this.renderCopyAllChangesButton(),
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
