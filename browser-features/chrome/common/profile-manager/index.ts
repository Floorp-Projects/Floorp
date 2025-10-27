/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { ProfileManagerService } from "./service.tsx";

/**
 * Chrome CSS component for user styles management
 */
@noraComponent(import.meta.hot)
export default class ProfileManager extends NoraComponentBase {
  static ctx: ProfileManagerService | null = null;

  init(): void {
    const profileManagerService = ProfileManagerService.getInstance();
    profileManagerService.init();
    ProfileManager.ctx = profileManagerService;
  }
}
