/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const React = require("react");
const { Component } = React;
const PropTypes = require("prop-types");
const dom = require("react-dom-factories");
require("./QuickLinks.css");
const samples = require("../samples.js");

class QuickLinks extends Component {
  static get propTypes() {
    return {
      evaluate: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.evaluateExpressions = this.evaluateExpressions.bind(this);
    this.renderLinks = this.renderLinks.bind(this);
  }

  evaluateExpressions(expressions) {
    expressions.forEach(expression => this.props.evaluate(expression));
  }

  renderLinks() {
    return Object.keys(samples).map(label => {
      const expressions = samples[label];
      const length = expressions.length;
      return dom.button(
        {
          key: label,
          title:
            label === "yolo"
              ? "Add all sample expressions"
              : `Add ${length} ${label} sample expression${
                  length > 1 ? "s" : ""
                }`,
          onClick: () => this.evaluateExpressions(expressions),
        },
        label
      );
    });
  }

  render() {
    return dom.div({ className: "quick-links" }, this.renderLinks());
  }
}

module.exports = QuickLinks;
