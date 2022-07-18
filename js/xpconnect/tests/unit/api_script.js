"use strict";

// This is a test script similar to those used by ExtensionAPIs.
// https://searchfox.org/mozilla-central/source/toolkit/components/extensions/parent

let module3, module4;

// This should work across ESR 102 and Firefox 103+.
if (ChromeUtils.importESModule) {
  module3 = ChromeUtils.importESModule("resource://test/esmified-3.sys.mjs");
  module4 = ChromeUtils.importESModule("resource://test/esmified-4.sys.mjs");
} else {
  module3 = ChromeUtils.import("resource://test/esmified-3.jsm");
  module4 = ChromeUtils.import("resource://test/esmified-4.jsm");
}

injected3.obj.value += 3;
module3.obj.value += 3;
module4.obj.value += 4;

this.testResults = {
  injected3: injected3.obj.value,
  module3: module3.obj.value,
  sameInstance3: injected3 === module3,
  module4: module4.obj.value,
};
