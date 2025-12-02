/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { ExternalBrowserTabContextMenu } from "./tab-context-menu.tsx";
import { ExternalBrowserLinkContextMenu } from "./link-context-menu.tsx";

@noraComponent(import.meta.hot)
export default class ExternalBrowser extends NoraComponentBase {
  init() {
    this.logger.info("Initializing ExternalBrowser component");

    // Initialize tab context menu (right-click on tab)
    new ExternalBrowserTabContextMenu();

    // Initialize link context menu (right-click on link)
    new ExternalBrowserLinkContextMenu();

    this.logger.info("ExternalBrowser component initialized");
  }
}
