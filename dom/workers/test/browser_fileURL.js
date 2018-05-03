/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const WORKER_BODY = "postMessage(42);\n";

// file:// tests.
add_task(async function() {
  info("Creating the tmp directory.");
  let parent = Cc["@mozilla.org/file/directory_service;1"]
                 .getService(Ci.nsIDirectoryService)
                 .QueryInterface(Ci.nsIProperties)
                 .get('TmpD', Ci.nsIFile);
  parent.append('worker-dir-test');
  parent.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o700);

  let dir_a = parent.clone();
  dir_a.append('a');
  dir_a.create(Ci.nsIFile.DIRECTORY_TYPE, 0o700);

  let page_a = dir_a.clone();
  page_a.append('empty.html');
  page_a.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  let url_a = Services.io.newFileURI(page_a)

  let worker = dir_a.clone();
  worker.append('worker.js');

  let stream = Cc["@mozilla.org/network/file-output-stream;1"]
                    .createInstance(Ci.nsIFileOutputStream);
  stream.init(worker, 0x02 | 0x08 | 0x20, // write, create, truncate
               0o666, 0);
  stream.write(WORKER_BODY, WORKER_BODY.length);
  stream.close();

  let url_worker = Services.io.newFileURI(worker);

  let dir_b = parent.clone();
  dir_b.append('b');
  dir_b.create(Ci.nsIFile.DIRECTORY_TYPE, 0o700);

  let page_b = dir_b.clone();
  page_b.append('empty.html');
  page_b.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  let url_b = Services.io.newFileURI(page_b)

  let tab = BrowserTestUtils.addTab(gBrowser, url_a.spec);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, url_worker.spec, function(spec) {
    return new content.Promise((resolve, reject) => {
      let w = new content.window.Worker(spec);
      w.onerror = _ => { reject(); }
      w.onmessage = _ => { resolve() };
    });
  });
  ok(true, "The worker is loaded when the script is on the same directory.");

  BrowserTestUtils.removeTab(tab);

  tab = BrowserTestUtils.addTab(gBrowser, url_b.spec);
  gBrowser.selectedTab = tab;

  browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, url_worker.spec, function(spec) {
    return new content.Promise((resolve, reject) => {
      let w = new content.window.Worker(spec);
      w.onerror = _ => { resolve(); }
      w.onmessage = _ => { reject() };
    });
  });
  ok(true, "The worker is not loaded when the script is on a different directory.");

  BrowserTestUtils.removeTab(tab);

  info("Removing the tmp directory.");
  parent.remove(true);
});

const EMPTY_URL = "/browser/dom/workers/test/empty.html";
const WORKER_URL = "/browser/dom/workers/test/empty_worker.js";

add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser, "http://mochi.test:8888" + EMPTY_URL);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, "http://example.org" + WORKER_URL, function(spec) {
    return new content.Promise((resolve, reject) => {
      let w = new content.window.Worker(spec);
      w.onerror = _ => { resolve(); }
      w.onmessage = _ => { reject() };
    });
  });
  ok(true, "The worker is not loaded when the script is from different origin.");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function() {
  let tab = BrowserTestUtils.addTab(gBrowser, "https://example.org" + EMPTY_URL);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await ContentTask.spawn(browser, "http://example.org" + WORKER_URL, function(spec) {
    return new content.Promise((resolve, reject) => {
      let w = new content.window.Worker(spec);
      w.onerror = _ => { resolve(); }
      w.onmessage = _ => { reject() };
    });
  });
  ok(true, "The worker is not loaded when the script is from different origin.");

  BrowserTestUtils.removeTab(tab);
});
