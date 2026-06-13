/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { splitViewService } from "./service.js";

export { splitViewService } from "./service.js";
export {
  isSplitViewEnabled,
  setSplitViewEnabled,
  splitViewEnabled,
} from "./data/enabled.js";

@noraComponent(import.meta.hot)
export default class SplitView extends NoraComponentBase {
  init(): void {
    this.logger.debug("Initializing split view service");
    splitViewService.init();
  }
}
