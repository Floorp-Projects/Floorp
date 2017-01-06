/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals XPCOMUtils, UAOverrider, IOService */

"use strict";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UAOverrider", "chrome://webcompat/content/lib/ua_overrider.js");
XPCOMUtils.defineLazyServiceGetter(this, "IOService", "@mozilla.org/network/io-service;1", "nsIIOService");

function getnsIURI(uri) {
  return IOService.newURI(uri, "utf-8", null);
}

add_task(function test() {
  let overrider = new UAOverrider([
    {
      baseDomain: "example.org",
      uaTransformer: () => "Test UA"
    }
  ]);

  let finalUA = overrider.getUAForURI(getnsIURI("http://www.example.org/foobar/"));
  is(finalUA, "Test UA", "Overrides the UA without a matcher function");
});

add_task(function test() {
  let overrider = new UAOverrider([
    {
      baseDomain: "example.org",
      uriMatcher: () => false,
      uaTransformer: () => "Test UA"
    }
  ]);

  let finalUA = overrider.getUAForURI(getnsIURI("http://www.example.org/foobar/"));
  isnot(finalUA, "Test UA", "Does not override the UA with the matcher returning false");
});
