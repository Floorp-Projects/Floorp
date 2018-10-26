/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const CSSDeclaration = createFactory(require("./CSSDeclaration"));

class ChangesApp extends PureComponent {
  static get propTypes() {
    return {
      // Redux state slice assigned to Track Changes feature; passed as prop by connect()
      changes: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);
    // In the Redux store, all rules exist in a collection at the same level of nesting.
    // Parent rules come before child rules. Parent/child dependencies are set
    // via parameters in each rule pointing to the corresponding rule ids.
    //
    // To render rules, we traverse the descendant rule tree and render each child rule
    // found. This means we get into situations where we can render the same rule multiple
    // times: once as a child of its parent and once standalone.
    //
    // By keeping a log of rules previously rendered we prevent needless multi-rendering.
    this.renderedRules = [];
  }

  renderDeclarations(remove = {}, add = {}) {
    const removals = Object.entries(remove).map(([property, value]) => {
      return CSSDeclaration({ className: "level diff-remove", property, value });
    });

    const additions = Object.entries(add).map(([property, value]) => {
      return CSSDeclaration({ className: "level diff-add", property, value });
    });

    return [removals, additions];
  }

  renderRule(ruleId, rule, rules) {
    const selector = rule.selector;

    if (this.renderedRules.includes(ruleId)) {
      return null;
    }

    // Mark this rule as rendered so we don't render it again.
    this.renderedRules.push(ruleId);

    return dom.div(
      {
        className: "rule",
      },
      dom.div(
        {
          className: "level selector",
          title: selector,
        },
        selector,
        dom.span({ className: "bracket-open" }, "{")
      ),
      // Render any nested child rules if they are present.
      rule.children.length > 0 && rule.children.map(childRuleId => {
        return this.renderRule(childRuleId, rules[childRuleId], rules);
      }),
      // Render any changed CSS declarations.
      this.renderDeclarations(rule.remove, rule.add),
      dom.span({ className: "level bracket-close" }, "}")
    );
  }

  renderDiff(changes = {}) {
    // Render groups of style sources: stylesheets and element style attributes.
    return Object.entries(changes).map(([sourceId, source]) => {
      const href = source.href || `inline stylesheet #${source.index}`;
      const rules = source.rules;

      return dom.details(
        {
          className: "source devtools-monospace",
          open: true,
        },
        dom.summary(
          {
            className: "href",
            title: href,
          },
          href),
        // Render changed rules within this source.
        Object.entries(rules).map(([ruleId, rule]) => {
          return this.renderRule(ruleId, rule, rules);
        })
      );
    });
  }

  render() {
    // Reset log of rendered rules.
    this.renderedRules = [];

    return dom.div(
      {
        className: "theme-sidebar inspector-tabpanel",
        id: "sidebar-panel-changes",
      },
      this.renderDiff(this.props.changes)
    );
  }
}

module.exports = connect(state => state)(ChangesApp);
