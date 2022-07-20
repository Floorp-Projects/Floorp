/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  PageDataSchema: "resource:///modules/pagedata/PageDataSchema.sys.mjs",
  PageDataService: "resource:///modules/pagedata/PageDataService.sys.mjs",
  Snapshots: "resource:///modules/Snapshots.sys.mjs",
});
