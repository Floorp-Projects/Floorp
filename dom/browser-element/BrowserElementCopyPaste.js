/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
global addMessageListener
global removeMessageListener
global docShell
global content
global sendAsyncMsg
*/

function debug(msg) {
  // dump("BrowserElementCopyPaste - " + msg + "\n");
}

debug("loaded");

var CopyPasteAssistent = {
  COMMAND_MAP: {
    "cut": "cmd_cut",
    "copy": "cmd_copy",
    "paste": "cmd_paste",
    "selectall": "cmd_selectAll",
  },

  init() {
    addEventListener("mozcaretstatechanged", this,
                     /* useCapture = */ true, /* wantsUntrusted = */ false);
    addMessageListener("browser-element-api:call", this);
  },

  destroy() {
    removeEventListener("mozcaretstatechanged", this,
                        /* useCapture = */ true, /* wantsUntrusted = */ false);
    removeMessageListener("browser-element-api:call", this);
  },

  handleEvent(event) {
    switch (event.type) {
      case "mozcaretstatechanged":
        this._caretStateChangedHandler(event);
        break;
    }
  },

  receiveMessage(message) {
    switch (message.name) {
      case "browser-element-api:call":
        this._browserAPIHandler(message);
        break;
    }
  },

  _browserAPIHandler(e) {
    switch (e.data.msg_name) {
      case "copypaste-do-command":
        if (this._isCommandEnabled(e.data.command)) {
          docShell.doCommand(this.COMMAND_MAP[e.data.command]);
        }
        break;
    }
  },

  _isCommandEnabled(cmd) {
    let command = this.COMMAND_MAP[cmd];
    if (!command) {
      return false;
    }

    return docShell.isCommandEnabled(command);
  },

  _caretStateChangedHandler(e) {
    e.stopPropagation();

    let boundingClientRect = e.boundingClientRect;
    this._isCommandEnabled("paste");
    let zoomFactor = content.innerWidth == 0 ? 1 : content.screen.width / content.innerWidth;

    let detail = {
      rect: {
        width: boundingClientRect ? boundingClientRect.width : 0,
        height: boundingClientRect ? boundingClientRect.height : 0,
        top: boundingClientRect ? boundingClientRect.top : 0,
        bottom: boundingClientRect ? boundingClientRect.bottom : 0,
        left: boundingClientRect ? boundingClientRect.left : 0,
        right: boundingClientRect ? boundingClientRect.right : 0,
      },
      commands: {
        canSelectAll: this._isCommandEnabled("selectall"),
        canCut: this._isCommandEnabled("cut"),
        canCopy: this._isCommandEnabled("copy"),
        canPaste: this._isCommandEnabled("paste"),
      },
      zoomFactor,
      reason: e.reason,
      collapsed: e.collapsed,
      caretVisible: e.caretVisible,
      selectionVisible: e.selectionVisible,
      selectionEditable: e.selectionEditable,
      selectedTextContent: e.selectedTextContent,
    };

    // Get correct geometry information if we have nested iframe.
    let currentWindow = e.target.defaultView;
    while (currentWindow.realFrameElement) {
      let currentRect = currentWindow.realFrameElement.getBoundingClientRect();
      detail.rect.top += currentRect.top;
      detail.rect.bottom += currentRect.top;
      detail.rect.left += currentRect.left;
      detail.rect.right += currentRect.left;
      currentWindow = currentWindow.realFrameElement.ownerGlobal;

      let targetDocShell = currentWindow.docShell;
      if (targetDocShell.isMozBrowser) {
        break;
      }
    }

    sendAsyncMsg("caretstatechanged", detail);
  },
};

CopyPasteAssistent.init();
