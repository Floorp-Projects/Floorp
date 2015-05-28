/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

Components.utils.import("resource:///modules/NewTabURL.jsm");

function run_test() {
  Assert.equal(NewTabURL.get(), "about:newtab", "Default newtab URL should be about:newtab");
  let url = "http://example.com/";
  NewTabURL.override(url);
  Assert.ok(NewTabURL.overridden, "Newtab URL should be overridden");
  Assert.equal(NewTabURL.get(), url, "Newtab URL should be the custom URL");
  NewTabURL.reset();
  Assert.ok(!NewTabURL.overridden, "Newtab URL should not be overridden");
  Assert.equal(NewTabURL.get(), "about:newtab", "Newtab URL should be the about:newtab");
}
