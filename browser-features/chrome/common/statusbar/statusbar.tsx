// SPDX-License-Identifier: MPL-2.0

import { manager } from "./index.ts";
import statusbarStyle from "./statusbar.css?inline";

export function StatusBarElem() {
  return (
    <>
      <xul:toolbar
        id="nora-statusbar"
        toolbarname="Status bar"
        customizable="true"
        style="border-top: 1px solid var(--chrome-content-separator-color)"
        class={`nora-statusbar browser-toolbar ${
          manager.showStatusBar() ? "" : "collapsed"
        }`}
        mode="icons"
        context="toolbar-context-menu"
        accesskey="A"
      >
        <xul:hbox
          id="status-text"
          align="center"
          flex="1"
          class="statusbar-padding"
        />
      </xul:toolbar>
      <style class="nora-statusbar" jsx>
        {statusbarStyle}
      </style>
    </>
  );
}
