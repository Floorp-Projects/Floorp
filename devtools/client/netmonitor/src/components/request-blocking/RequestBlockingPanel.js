/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const {
  button,
  div,
  form,
  input,
  label,
  li,
  span,
  ul,
} = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const Actions = require("../../actions/index");
const { L10N } = require("../../utils/l10n");
const { PANELS } = require("../../constants");

const ENABLE_BLOCKING_LABEL = L10N.getStr(
  "netmonitor.actionbar.enableBlocking"
);
const ADD_BUTTON_TOOLTIP = L10N.getStr("netmonitor.actionbar.addBlockedUrl");
const ADD_URL_PLACEHOLDER = L10N.getStr(
  "netmonitor.actionbar.blockSearchPlaceholder"
);
const REMOVE_URL_TOOLTIP = L10N.getStr("netmonitor.actionbar.removeBlockedUrl");

class RequestBlockingPanel extends Component {
  static get propTypes() {
    return {
      blockedUrls: PropTypes.array.isRequired,
      addBlockedUrl: PropTypes.func.isRequired,
      isDisplaying: PropTypes.bool.isRequired,
      removeBlockedUrl: PropTypes.func.isRequired,
      toggleBlockingEnabled: PropTypes.func.isRequired,
      toggleBlockedUrl: PropTypes.func.isRequired,
      updateBlockedUrl: PropTypes.func.isRequired,
      blockingEnabled: PropTypes.bool.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      editingUrl: null,
    };
  }

  componentDidMount() {
    this.refs.addInput.focus();
  }

  componentDidUpdate(prevProps) {
    if (this.state.editingUrl) {
      this.refs.editInput.focus();
      this.refs.editInput.select();
    } else if (this.props.isDisplaying && !prevProps.isDisplaying) {
      this.refs.addInput.focus();
    }
  }

  renderEnableBar() {
    return div(
      { className: "request-blocking-enable-bar" },
      div(
        { className: "request-blocking-enable-form" },
        label(
          { className: "devtools-checkbox-label" },
          input({
            type: "checkbox",
            className: "devtools-checkbox",
            checked: this.props.blockingEnabled,
            ref: "enabledCheckbox",
            onChange: () =>
              this.props.toggleBlockingEnabled(
                this.refs.enabledCheckbox.checked
              ),
          }),
          span({ className: "request-blocking-label" }, ENABLE_BLOCKING_LABEL)
        )
      ),
      button({
        className: "devtools-button",
        title: ADD_BUTTON_TOOLTIP,
        onClick: () => this.refs.addInput.focus(),
      })
    );
  }

  renderItemContent({ url, enabled }) {
    const { toggleBlockedUrl, removeBlockedUrl } = this.props;

    return li(
      { key: url },
      label(
        {
          className: "devtools-checkbox-label",
          onDoubleClick: () => this.setState({ editingUrl: url }),
        },
        input({
          type: "checkbox",
          className: "devtools-checkbox",
          checked: enabled,
          onChange: () => toggleBlockedUrl(url),
        }),
        span(
          {
            className: "request-blocking-label",
            title: url,
          },
          url
        )
      ),
      button({
        className: "request-blocking-remove-button",
        title: REMOVE_URL_TOOLTIP,
        "aria-label": REMOVE_URL_TOOLTIP,
        onClick: () => removeBlockedUrl(url),
      })
    );
  }

  renderEditForm(url) {
    const { updateBlockedUrl, removeBlockedUrl } = this.props;
    return li(
      { key: url, className: "request-blocking-edit-item" },
      form(
        {
          onSubmit: e => {
            const { editInput } = this.refs;
            const newValue = editInput.value;
            e.preventDefault();

            if (url != newValue) {
              if (editInput.value.trim() === "") {
                removeBlockedUrl(url, newValue);
              } else {
                updateBlockedUrl(url, newValue);
              }
            }
            this.setState({ editingUrl: null });
          },
        },
        input({
          type: "text",
          defaultValue: url,
          ref: "editInput",
          className: "devtools-searchinput",
          placeholder: ADD_URL_PLACEHOLDER,
          onBlur: () => this.setState({ editingUrl: null }),
          onKeyDown: e => {
            if (e.key === "Escape") {
              e.stopPropagation();
              e.preventDefault();
              this.setState({ editingUrl: null });
            }
          },
        }),

        input({ type: "submit", style: { display: "none" } })
      )
    );
  }

  renderBlockedList() {
    const { blockedUrls, blockingEnabled } = this.props;

    if (blockedUrls.length === 0) {
      return null;
    }

    const listItems = blockedUrls.map(item =>
      this.state.editingUrl === item.url
        ? this.renderEditForm(item.url)
        : this.renderItemContent(item)
    );

    return ul(
      {
        className: `request-blocking-list ${blockingEnabled ? "" : "disabled"}`,
      },
      ...listItems
    );
  }

  renderAddForm() {
    const { addBlockedUrl } = this.props;
    return div(
      { className: "request-blocking-add-form" },
      form(
        {
          onSubmit: e => {
            const { addInput } = this.refs;
            e.preventDefault();
            addBlockedUrl(addInput.value);
            addInput.value = "";
            addInput.focus();
          },
        },
        input({
          type: "text",
          ref: "addInput",
          className: "devtools-searchinput",
          placeholder: ADD_URL_PLACEHOLDER,
          onKeyDown: e => {
            if (e.key === "Escape") {
              e.stopPropagation();
              e.preventDefault();

              const { addInput } = this.refs;
              addInput.value = "";
              addInput.focus();
            }
          },
        }),
        input({ type: "submit", style: { display: "none" } })
      )
    );
  }

  render() {
    return div(
      { className: "request-blocking-panel" },
      this.renderEnableBar(),
      div(
        { className: "request-blocking-contents" },
        this.renderBlockedList(),
        this.renderAddForm()
      )
    );
  }
}

module.exports = connect(
  state => ({
    blockedUrls: state.requestBlocking.blockedUrls,
    blockingEnabled: state.requestBlocking.blockingEnabled,
    isDisplaying: state.ui.selectedActionBarTabId === PANELS.BLOCKING,
  }),
  dispatch => ({
    toggleBlockingEnabled: checked =>
      dispatch(Actions.toggleBlockingEnabled(checked)),
    addBlockedUrl: url => dispatch(Actions.addBlockedUrl(url)),
    removeBlockedUrl: url => dispatch(Actions.removeBlockedUrl(url)),
    toggleBlockedUrl: url => dispatch(Actions.toggleBlockedUrl(url)),
    updateBlockedUrl: (oldUrl, newUrl) =>
      dispatch(Actions.updateBlockedUrl(oldUrl, newUrl)),
  })
)(RequestBlockingPanel);
