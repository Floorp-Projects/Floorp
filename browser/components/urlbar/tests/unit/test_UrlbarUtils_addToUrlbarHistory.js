/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * These tests unit test the functionality of the functions in UrlbarUtils.
 * Some functions are bigger, and split out into sepearate test_UrlbarUtils_* files.
 */

"use strict";

const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
const { PlacesUIUtils } = ChromeUtils.importESModule(
  "resource:///modules/PlacesUIUtils.sys.mjs"
);

let sandbox;

add_task(function setup() {
  sandbox = sinon.createSandbox();
});

add_task(function test_addToUrlbarHistory() {
  sandbox.stub(PlacesUIUtils, "markPageAsTyped");
  sandbox.stub(PrivateBrowsingUtils, "isWindowPrivate").returns(false);

  UrlbarUtils.addToUrlbarHistory("http://example.com");
  Assert.ok(
    PlacesUIUtils.markPageAsTyped.calledOnce,
    "Should have marked a simple URL as typed."
  );
  PlacesUIUtils.markPageAsTyped.resetHistory();

  UrlbarUtils.addToUrlbarHistory();
  Assert.ok(
    PlacesUIUtils.markPageAsTyped.notCalled,
    "Should not have attempted to mark a null URL as typed."
  );
  PlacesUIUtils.markPageAsTyped.resetHistory();

  UrlbarUtils.addToUrlbarHistory("http://exam ple.com");
  Assert.ok(
    PlacesUIUtils.markPageAsTyped.notCalled,
    "Should not have marked a URL containing a space as typed."
  );
  PlacesUIUtils.markPageAsTyped.resetHistory();

  UrlbarUtils.addToUrlbarHistory("http://exam\x01ple.com");
  Assert.ok(
    PlacesUIUtils.markPageAsTyped.notCalled,
    "Should not have marked a URL containing a control character as typed."
  );
  PlacesUIUtils.markPageAsTyped.resetHistory();

  PrivateBrowsingUtils.isWindowPrivate.returns(true);
  UrlbarUtils.addToUrlbarHistory("http://example.com");
  Assert.ok(
    PlacesUIUtils.markPageAsTyped.notCalled,
    "Should not have marked a URL provided by a private browsing page as typed."
  );
  PlacesUIUtils.markPageAsTyped.resetHistory();
});
