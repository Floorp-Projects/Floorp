/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;

function run_test() {
  var sb = new Cu.Sandbox('http://www.example.com', { wantComponents: true } );
  Assert.ok(!Cu.evalInSandbox('"Components" in this', sb));
}
