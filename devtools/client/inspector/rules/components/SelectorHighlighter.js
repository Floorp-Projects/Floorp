/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const { getStr } = require("devtools/client/inspector/rules/utils/l10n");
const Types = require("devtools/client/inspector/rules/types");

class SelectorHighlighter extends PureComponent {
  static get propTypes() {
    return {
      highlightedSelector: PropTypes.string.isRequired,
      onToggleSelectorHighlighter: PropTypes.func.isRequired,
      selector: PropTypes.shape(Types.selector).isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      // A unique selector to the current Rule. This is checked against the value
      // of any existing highlighted selector in order mark the toggled state of
      // the component.
      uniqueSelector: "",
    };

    this.onToggleHighlighterClick = this.onToggleHighlighterClick.bind(this);
  }

  componentDidMount() {
    // Only fetch the unique selector if a selector is highlighted.
    if (this.props.highlightedSelector) {
      this.updateState();
    }
  }

  async onToggleHighlighterClick(event) {
    event.stopPropagation();

    const { onToggleSelectorHighlighter, selector } = this.props;

    const uniqueSelector = await selector.getUniqueSelector();
    this.setState({ uniqueSelector });
    onToggleSelectorHighlighter(uniqueSelector);
  }

  async updateState() {
    const uniqueSelector = await this.props.selector.getUniqueSelector();
    this.setState({ uniqueSelector });
  }

  render() {
    const { highlightedSelector } = this.props;
    const { uniqueSelector } = this.state;

    return dom.span({
      className:
        "ruleview-selectorhighlighter js-toggle-selector-highlighter" +
        (highlightedSelector && highlightedSelector === uniqueSelector
          ? " highlighted"
          : ""),
      onClick: this.onToggleHighlighterClick,
      title: getStr("rule.selectorHighlighter.tooltip"),
    });
  }
}

const mapStateToProps = state => {
  return {
    highlightedSelector: state.rules.highlightedSelector,
  };
};

module.exports = connect(mapStateToProps)(SelectorHighlighter);
