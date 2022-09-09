/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const {
  getReverseSearchTotalResults,
  getReverseSearchResultPosition,
  getReverseSearchResult,
} = require("devtools/client/webconsole/selectors/history");

loader.lazyRequireGetter(
  this,
  "PropTypes",
  "devtools/client/shared/vendor/react-prop-types"
);
loader.lazyRequireGetter(
  this,
  "actions",
  "devtools/client/webconsole/actions/index"
);
loader.lazyRequireGetter(
  this,
  "l10n",
  "devtools/client/webconsole/utils/messages",
  true
);
loader.lazyRequireGetter(
  this,
  "PluralForm",
  "devtools/shared/plural-form",
  true
);
loader.lazyRequireGetter(
  this,
  "KeyCodes",
  "devtools/client/shared/keycodes",
  true
);

const isMacOS = Services.appinfo.OS === "Darwin";

class ReverseSearchInput extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      setInputValue: PropTypes.func.isRequired,
      focusInput: PropTypes.func.isRequired,
      reverseSearchResult: PropTypes.string,
      reverseSearchTotalResults: PropTypes.number,
      reverseSearchResultPosition: PropTypes.number,
      visible: PropTypes.bool,
      initialValue: PropTypes.string,
    };
  }

  constructor(props) {
    super(props);

    this.onInputKeyDown = this.onInputKeyDown.bind(this);
  }

  componentDidUpdate(prevProps) {
    const { setInputValue, focusInput } = this.props;
    if (
      prevProps.reverseSearchResult !== this.props.reverseSearchResult &&
      this.props.visible &&
      this.props.reverseSearchTotalResults > 0
    ) {
      setInputValue(this.props.reverseSearchResult);
    }

    if (prevProps.visible === true && this.props.visible === false) {
      focusInput();
    }

    if (
      prevProps.visible === false &&
      this.props.visible === true &&
      this.props.initialValue
    ) {
      this.inputNode.value = this.props.initialValue;
    }
  }

  onEnterKeyboardShortcut(event) {
    const { dispatch } = this.props;
    event.stopPropagation();
    dispatch(actions.reverseSearchInputToggle());
    dispatch(actions.evaluateExpression(undefined, "reverse-search"));
  }

  onEscapeKeyboardShortcut(event) {
    const { dispatch } = this.props;
    event.stopPropagation();
    dispatch(actions.reverseSearchInputToggle());
  }

  onBackwardNavigationKeyBoardShortcut(event, canNavigate) {
    const { dispatch } = this.props;
    event.stopPropagation();
    event.preventDefault();
    if (canNavigate) {
      dispatch(actions.showReverseSearchBack({ access: "keyboard" }));
    }
  }

  onForwardNavigationKeyBoardShortcut(event, canNavigate) {
    const { dispatch } = this.props;
    event.stopPropagation();
    event.preventDefault();
    if (canNavigate) {
      dispatch(actions.showReverseSearchNext({ access: "keyboard" }));
    }
  }

  onInputKeyDown(event) {
    const { keyCode, key, ctrlKey, shiftKey } = event;
    const { reverseSearchTotalResults } = this.props;

    // On Enter, we trigger an execute.
    if (keyCode === KeyCodes.DOM_VK_RETURN) {
      return this.onEnterKeyboardShortcut(event);
    }

    const lowerCaseKey = key.toLowerCase();

    // On Escape (and Ctrl + c on OSX), we close the reverse search input.
    if (
      keyCode === KeyCodes.DOM_VK_ESCAPE ||
      (isMacOS && ctrlKey && lowerCaseKey === "c")
    ) {
      return this.onEscapeKeyboardShortcut(event);
    }

    const canNavigate =
      Number.isInteger(reverseSearchTotalResults) &&
      reverseSearchTotalResults > 1;

    if (
      (!isMacOS && key === "F9" && !shiftKey) ||
      (isMacOS && ctrlKey && lowerCaseKey === "r")
    ) {
      return this.onBackwardNavigationKeyBoardShortcut(event, canNavigate);
    }

    if (
      (!isMacOS && key === "F9" && shiftKey) ||
      (isMacOS && ctrlKey && lowerCaseKey === "s")
    ) {
      return this.onForwardNavigationKeyBoardShortcut(event, canNavigate);
    }

    return null;
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

    return dom.div({ className: "reverse-search-info" }, text);
  }

  renderNavigationButtons() {
    const { dispatch, reverseSearchTotalResults } = this.props;

    if (
      !Number.isInteger(reverseSearchTotalResults) ||
      reverseSearchTotalResults <= 1
    ) {
      return null;
    }

    return [
      dom.button({
        key: "search-result-button-prev",
        className: "devtools-button search-result-button-prev",
        title: l10n.getFormatStr(
          "webconsole.reverseSearch.result.previousButton.tooltip",
          [isMacOS ? "Ctrl + R" : "F9"]
        ),
        onClick: () => {
          dispatch(actions.showReverseSearchBack({ access: "click" }));
          this.inputNode.focus();
        },
      }),
      dom.button({
        key: "search-result-button-next",
        className: "devtools-button search-result-button-next",
        title: l10n.getFormatStr(
          "webconsole.reverseSearch.result.nextButton.tooltip",
          [isMacOS ? "Ctrl + S" : "Shift + F9"]
        ),
        onClick: () => {
          dispatch(actions.showReverseSearchNext({ access: "click" }));
          this.inputNode.focus();
        },
      }),
    ];
  }

  render() {
    const { dispatch, visible, reverseSearchTotalResults } = this.props;

    if (!visible) {
      return null;
    }

    const classNames = ["reverse-search"];

    if (reverseSearchTotalResults === 0) {
      classNames.push("no-result");
    }

    return dom.div(
      { className: classNames.join(" ") },
      dom.input({
        ref: node => {
          this.inputNode = node;
        },
        autoFocus: true,
        placeholder: l10n.getStr("webconsole.reverseSearch.input.placeHolder"),
        className: "reverse-search-input devtools-monospace",
        onKeyDown: this.onInputKeyDown,
        onInput: ({ target }) =>
          dispatch(actions.reverseSearchInputChange(target.value)),
      }),
      dom.div(
        {
          className: "reverse-search-actions",
        },
        this.renderSearchInformation(),
        this.renderNavigationButtons(),
        dom.button({
          className: "devtools-button reverse-search-close-button",
          title: l10n.getFormatStr(
            "webconsole.reverseSearch.closeButton.tooltip",
            ["Esc" + (isMacOS ? " | Ctrl + C" : "")]
          ),
          onClick: () => {
            dispatch(actions.reverseSearchInputToggle());
          },
        })
      )
    );
  }
}

const mapStateToProps = state => ({
  visible: state.ui.reverseSearchInputVisible,
  reverseSearchTotalResults: getReverseSearchTotalResults(state),
  reverseSearchResultPosition: getReverseSearchResultPosition(state),
  reverseSearchResult: getReverseSearchResult(state),
});

const mapDispatchToProps = dispatch => ({ dispatch });

module.exports = connect(
  mapStateToProps,
  mapDispatchToProps
)(ReverseSearchInput);
