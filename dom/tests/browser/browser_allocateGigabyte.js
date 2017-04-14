/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/dom/tests/browser/test_largeAllocation.html";

function expectProcessCreated() {
  let os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  return new Promise(resolve => {
    let topic = "ipc:content-created";
    function observer() {
      os.removeObserver(observer, topic);
      ok(true, "Expect process created");
      resolve();
    }
    os.addObserver(observer, topic, /* weak = */ false);
  });
}

add_task(function*() {
  yield SpecialPowers.pushPrefEnv({
    set: [
      ["dom.largeAllocationHeader.enabled", true],
    ]
  });

  // A toplevel tab should be able to navigate cross process!
  yield BrowserTestUtils.withNewTab("about:blank", function*(aBrowser) {
    let epc = expectProcessCreated();
    yield ContentTask.spawn(aBrowser, TEST_URI, TEST_URI => {
      content.document.location = TEST_URI;
    });

    // Wait for the new process to be created by the Large-Allocation header
    yield epc;

    // Allocate a gigabyte of memory in the content process
    yield ContentTask.spawn(aBrowser, null, () => {
      let arrayBuffer = new ArrayBuffer(1024*1024*1024);
      ok(arrayBuffer, "Successfully allocated a gigabyte of memory in content process");
    });
  });
});
