/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { manager } from "./";
import statusbarUtilsStyle from "./statusbar-utils.css?inline";

export function StatusBarElem() {
  return (
    <>
      <xul:toolbar
        id="nora-statusbar"
        toolbarname="Status bar"
        customizable="true"
        class={`browser-toolbar`}
        style={{
          display: manager.showStatusBar() ? "flex" : "none",
          alignItems: "center",
          width: "100%",
          borderTop: "1px solid var(--chrome-content-separator-color)",
          background: "var(--panel-sidebar-background-color)",
        }}
        mode="icons"
        context="toolbar-context-menu"
        accesskey="A"
      >
        <xul:hbox
          id="status-text"
          align="center"
          flex="1"
          class="status-text"
        />
      </xul:toolbar>
      <style class="nora-statusbar-utils" jsx>
        {statusbarUtilsStyle}
      </style>
    </>
  );
}
