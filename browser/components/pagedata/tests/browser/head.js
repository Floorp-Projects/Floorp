/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  PageDataSchema: "resource:///modules/pagedata/PageDataSchema.jsm",
  PageDataService: "resource:///modules/pagedata/PageDataService.jsm",
  Snapshots: "resource:///modules/Snapshots.jsm",
});
