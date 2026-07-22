/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Panel } from "../utils/type.ts";
import { STATIC_PANEL_DATA } from "../data/static-panels.ts";

export function ChromeSiteBrowser({ id, url }: Panel) {
  const panel = STATIC_PANEL_DATA[url as keyof typeof STATIC_PANEL_DATA];
  // about: pages need a content browser as much as http ones do. Loaded
  // into a chrome-type browser they still navigate, but any about: page
  // whose UI is built by a window actor renders blank, because those
  // actors only attach to content browsing contexts. chrome:// panels
  // (bookmarks, history, notes) must stay chrome-type, so this tests the
  // scheme rather than widening the branch for everything.
  const asContentBrowser = panel.url?.startsWith("http") ||
    panel.url?.startsWith("about:");
  return (
    <xul:browser
      id={`sidebar-panel-${id}`}
      class="sidebar-panel-browser"
      flex="1"
      xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
      disablehistory="true"
      disablefullscreen="true"
      tooltip="aHTMLTooltip"
      autoscroll="false"
      disableglobalhistory="true"
      messagemanagergroup="browsers"
      autocompletepopup="PopupAutoComplete"
      {...(asContentBrowser
        ? {
          type: "content",
          remote: "true",
          maychangeremoteness: "true",
        }
        : {})}
      src={panel.url}
    />
  );
}
