/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function test() {
  runTests();
}

function cleanup() {
  let notificationBox = Browser.getNotificationBox();
  notificationBox && notificationBox.removeAllNotifications(true);
}

const XHTML_NS = "http://www.w3.org/1999/xhtml";

function createTestNotification(aLabel, aID) {
    let notificationBox = Browser.getNotificationBox();
    let notn = notificationBox.appendNotification(aLabel || "some label", aID || "test-notification",
                                                  "", notificationBox.PRIORITY_INFO_LOW, []);
    return notn;
}

gTests.push({
  desc: "Verify notification bindings are correct",
  run: function () {
    let notificationBox = Browser.getNotificationBox();
    let notn = createTestNotification();

    let binding = notn && getComputedStyle(notn).MozBinding;
    is(binding,
       "url(\"chrome://browser/content/bindings/notification.xml#notification\")",
       "notification has expected binding");

    is(getComputedStyle(notificationBox).MozBinding,
       "url(\"chrome://browser/content/bindings/notification.xml#notificationbox\")",
       "notificationbox has expected binding");
  },
  tearDown: cleanup
});

gTests.push({
  desc: "Check label property handling",
  run: function () {
    let notn = createTestNotification("the label");
    is(notn.label, "the label");

    let doc = notn.ownerDocument;
    let fragment = doc.createDocumentFragment();
    try {
      let boldLabel = doc.createElementNS(XHTML_NS, "b");
      boldLabel.innerHTML = 'The <span class="foo">label</span>';
      fragment.appendChild(boldLabel);
      notn.label = fragment;
    } catch (ex) {
      ok(!ex, "Exception creating notification label with markup: "+ex.message);
    }

    // expect to get a documentFragment back when the label has markup
    let labelNode = notn.label;
    is(labelNode.nodeType,
       Components.interfaces.nsIDOMNode.DOCUMENT_FRAGMENT_NODE,
       "notification label getter returns documentFragment nodeType "+Components.interfaces.nsIDOMNode.DOCUMENT_FRAGMENT_NODE+", when value contains markup");
    ok(labelNode !== fragment,
       "label fragment is a newly created fragment, not the one we assigned in the setter");
    ok(labelNode.querySelector("b > .foo"),
       "label fragment has the expected elements in it");
  },
  tearDown: cleanup
});

gTests.push({
  desc: "Check adoptNotification does what we expect",
  setUp: function() {
    yield addTab("about:start");
    yield addTab("about:mozilla");
  },
  run: function () {
    let browser = getBrowser();
    let notn = createTestNotification("label", "adopt-notification");
    let firstBox = Browser.getNotificationBox();
    let nextTab = Browser.getNextTab(Browser.getTabForBrowser(browser));
    let nextBox = Browser.getNotificationBox(nextTab.browser);

    ok(firstBox.getNotificationWithValue("adopt-notification"), "notificationbox has our notification intially");
    nextBox.adoptNotification(notn);

    ok(!firstBox.getNotificationWithValue("adopt-notification"), "after adoptNotification, original notificationbox no longer has our notification");
    ok(nextBox.getNotificationWithValue("adopt-notification"), "next notificationbox has our notification");
  },
  // leave browser in clean state for next tests
  tearDown: cleanUpOpenedTabs
});

