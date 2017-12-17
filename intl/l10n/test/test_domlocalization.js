/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { DOMLocalization } =
  Components.utils.import("resource://gre/modules/DOMLocalization.jsm", {});

add_task(function test_methods_presence() {
  equal(typeof DOMLocalization.prototype.getAttributes, "function");
  equal(typeof DOMLocalization.prototype.setAttributes, "function");
  equal(typeof DOMLocalization.prototype.translateElements, "function");
  equal(typeof DOMLocalization.prototype.connectRoot, "function");
  equal(typeof DOMLocalization.prototype.disconnectRoot, "function");
  equal(typeof DOMLocalization.prototype.translateRoots, "function");
});
