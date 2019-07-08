/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { l10n } = require("devtools/client/webconsole/utils/messages");
const Services = require("Services");
const isMacOS = Services.appinfo.OS === "Darwin";

class EditorToolbar extends Component {
  static get propTypes() {
    return {
      webConsoleUI: PropTypes.object.isRequired,
      editorMode: PropTypes.bool,
    };
  }

  render() {
    const { editorMode, webConsoleUI } = this.props;

    if (!editorMode) {
      return null;
    }

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
            [isMacOS ? "Cmd + Enter" : "Ctrl + Enter"]
          ),
          onClick: () => webConsoleUI.jsterm.execute(),
        },
        l10n.getStr("webconsole.editor.toolbar.executeButton.label")
      )
    );
  }
}

module.exports = EditorToolbar;
