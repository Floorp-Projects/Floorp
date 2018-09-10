/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { l10n } = require("devtools/client/webconsole/utils/messages");
const { PluralForm } = require("devtools/shared/plural-form");
const { KeyCodes } = require("devtools/client/shared/keycodes");

const actions = require("devtools/client/webconsole/actions/index");
const {
  getReverseSearchTotalResults,
  getReverseSearchResultPosition,
  getReverseSearchResult,
} = require("devtools/client/webconsole/selectors/history");

const Services = require("Services");
const isMacOS = Services.appinfo.OS === "Darwin";

class ReverseSearchInput extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      hud: PropTypes.object.isRequired,
      reverseSearchResult: PropTypes.string,
      reverseSearchTotalResults: PropTypes.Array,
      reverseSearchResultPosition: PropTypes.int,
      visible: PropTypes.bool,
    };
  }

  constructor(props) {
    super(props);

    this.onInputKeyDown = this.onInputKeyDown.bind(this);
  }

  componentDidUpdate(prevProps) {
    const {jsterm} = this.props.hud;
    if (
      prevProps.reverseSearchResult !== this.props.reverseSearchResult
      && this.props.visible
      && this.props.reverseSearchTotalResults > 0
    ) {
      jsterm.setInputValue(this.props.reverseSearchResult);
    }

    if (prevProps.visible === true && this.props.visible === false) {
      jsterm.focus();
    }
  }

  onInputKeyDown(event) {
    const {
      keyCode,
      key,
      ctrlKey,
      shiftKey,
    } = event;

    const {
      dispatch,
      hud,
      reverseSearchTotalResults,
    } = this.props;

    // On Enter, we trigger an execute.
    if (keyCode === KeyCodes.DOM_VK_RETURN) {
      event.stopPropagation();
      dispatch(actions.reverseSearchInputToggle());
      hud.jsterm.execute();
      return;
    }

    // On Escape (and Ctrl + c on OSX), we close the reverse search input.
    if (
      keyCode === KeyCodes.DOM_VK_ESCAPE || (
        isMacOS && ctrlKey === true && key.toLowerCase() === "c"
      )
    ) {
      event.stopPropagation();
      dispatch(actions.reverseSearchInputToggle());
      return;
    }

    const canNavigate = Number.isInteger(reverseSearchTotalResults)
      && reverseSearchTotalResults > 1;

    if (
      (!isMacOS && key === "F9" && shiftKey === false) ||
      (isMacOS && ctrlKey === true && key.toLowerCase() === "r")
    ) {
      event.stopPropagation();
      event.preventDefault();
      if (canNavigate) {
        dispatch(actions.showReverseSearchBack());
      }
      return;
    }

    if (
      (!isMacOS && key === "F9" && shiftKey === true) ||
      (isMacOS && ctrlKey === true && key.toLowerCase() === "s")
    ) {
      event.stopPropagation();
      event.preventDefault();
      if (canNavigate) {
        dispatch(actions.showReverseSearchNext());
      }
    }
  }

  renderSearchInformation() {
    const {
      reverseSearchTotalResults,
      reverseSearchResultPosition,
    } = this.props;

    if (!Number.isInteger(reverseSearchTotalResults)) {
      return null;
    }

    let text;
    if (reverseSearchTotalResults === 0) {
      text = l10n.getStr("webconsole.reverseSearch.noResult");
    } else {
      const resultsString = l10n.getStr("webconsole.reverseSearch.results");
      text = PluralForm.get(reverseSearchTotalResults, resultsString)
        .replace("#1", reverseSearchResultPosition)
        .replace("#2", reverseSearchTotalResults);
    }

    return dom.div({className: "reverse-search-info"}, text);
  }

  renderNavigationButtons() {
    const {
      dispatch,
      reverseSearchTotalResults,
    } = this.props;

    if (!Number.isInteger(reverseSearchTotalResults) || reverseSearchTotalResults <= 1) {
      return null;
    }

    return [
      dom.button({
        className: "devtools-button search-result-button-prev",
        title: l10n.getFormatStr("webconsole.reverseSearch.result.previousButton.tooltip",
          [isMacOS ? "Ctrl + R" : "F9"]),
        onClick: () => {
          dispatch(actions.showReverseSearchBack());
          this.inputNode.focus();
        }
      }),
      dom.button({
        className: "devtools-button search-result-button-next",
        title: l10n.getFormatStr("webconsole.reverseSearch.result.nextButton.tooltip",
          [isMacOS ? "Ctrl + S" : "Shift + F9"]),
        onClick: () => {
          dispatch(actions.showReverseSearchNext());
          this.inputNode.focus();
        }
      })
    ];
  }

  render() {
    const {
      dispatch,
      visible,
      reverseSearchTotalResults,
    } = this.props;

    if (!visible) {
      return null;
    }

    const classNames = ["reverse-search"];
    if (reverseSearchTotalResults === 0) {
      classNames.push("no-result");
    }

    return dom.div({className: classNames.join(" ")},
      dom.input({
        ref: node => {
          this.inputNode = node;
        },
        autoFocus: true,
        placeHolder: l10n.getStr("webconsole.reverseSearch.input.placeHolder"),
        className: "reverse-search-input devtools-monospace",
        onKeyDown: this.onInputKeyDown,
        onInput: ({target}) => dispatch(actions.reverseSearchInputChange(target.value))
      }),
      this.renderSearchInformation(),
      this.renderNavigationButtons(),
      dom.button({
        className: "devtools-button reverse-search-close-button",
        title: l10n.getFormatStr("webconsole.reverseSearch.closeButton.tooltip",
          ["Esc" + (isMacOS ? " | Ctrl + C" : "")]),
        onClick: () => {
          dispatch(actions.reverseSearchInputToggle());
        }
      })
    );
  }
}

const mapStateToProps = state => ({
  visible: state.ui.reverseSearchInputVisible,
  reverseSearchTotalResults: getReverseSearchTotalResults(state),
  reverseSearchResultPosition: getReverseSearchResultPosition(state),
  reverseSearchResult: getReverseSearchResult(state),
});

const mapDispatchToProps = dispatch => ({dispatch});

module.exports = connect(mapStateToProps, mapDispatchToProps)(ReverseSearchInput);
