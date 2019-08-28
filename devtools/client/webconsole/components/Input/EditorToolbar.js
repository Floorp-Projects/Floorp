/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const actions = require("devtools/client/webconsole/actions/index");
const { l10n } = require("devtools/client/webconsole/utils/messages");
const Services = require("Services");
const isMacOS = Services.appinfo.OS === "Darwin";

// Constants used for defining the direction of JSTerm input history navigation.
const {
  HISTORY_BACK,
  HISTORY_FORWARD,
} = require("devtools/client/webconsole/constants");

class EditorToolbar extends Component {
  static get propTypes() {
    return {
      editorMode: PropTypes.bool,
      dispatch: PropTypes.func.isRequired,
      webConsoleUI: PropTypes.object.isRequired,
    };
  }

  render() {
    const { editorMode, dispatch, webConsoleUI } = this.props;

    if (!editorMode) {
      return null;
    }

    const enterStr = l10n.getStr("webconsole.enterKey");

    return dom.div(
      {
        className:
          "devtools-toolbar devtools-input-toolbar webconsole-editor-toolbar",
      },
      dom.button(
        {
          className: "devtools-button webconsole-editor-toolbar-executeButton",
          title: l10n.getFormatStr(
            "webconsole.editor.toolbar.executeButton.tooltip",
            [isMacOS ? `Cmd + ${enterStr}` : `Ctrl + ${enterStr}`]
          ),
          onClick: () => dispatch(actions.evaluateExpression()),
        },
        l10n.getStr("webconsole.editor.toolbar.executeButton.label")
      ),
      dom.button({
        className:
          "devtools-button webconsole-editor-toolbar-history-prevExpressionButton",
        title: l10n.getStr(
          "webconsole.editor.toolbar.history.prevExpressionButton.tooltip"
        ),
        onClick: () => {
          webConsoleUI.jsterm.historyPeruse(HISTORY_BACK);
        },
      }),
      dom.button({
        className:
          "devtools-button webconsole-editor-toolbar-history-nextExpressionButton",
        title: l10n.getStr(
          "webconsole.editor.toolbar.history.nextExpressionButton.tooltip"
        ),
        onClick: () => {
          webConsoleUI.jsterm.historyPeruse(HISTORY_FORWARD);
        },
      }),
      dom.button({
        className: "devtools-button webconsole-editor-toolbar-closeButton",
        title: l10n.getFormatStr(
          "webconsole.editor.toolbar.closeButton.tooltip2",
          [isMacOS ? "Cmd + B" : "Ctrl + B"]
        ),
        onClick: () => dispatch(actions.editorToggle()),
      })
    );
  }
}

module.exports = EditorToolbar;
