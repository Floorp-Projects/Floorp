/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { RebootPanelMenu } from "./RebootPanelMenu";

@noraComponent(import.meta.hot)
export default class RebootPanelMenuComponent extends NoraComponentBase {
  private rebootPanelMenu: RebootPanelMenu | null = null;

  init(): void {
    this.rebootPanelMenu = new RebootPanelMenu();
  }
}
