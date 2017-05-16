"use strict";

// Here we want to test that blob URLs are not available cross containers.

const BASE_URI = "http://mochi.test:8888/browser/browser/components/"
  + "contextualidentity/test/browser/empty_file.html";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true]
  ]});
});

add_task(async function test() {
  info("Creating a tab with UCI = 1...");
  let tab1 = BrowserTestUtils.addTab(gBrowser, BASE_URI, {userContextId: 1});
  is(tab1.getAttribute("usercontextid"), 1, "New tab has UCI equal 1");

  let browser1 = gBrowser.getBrowserForTab(tab1);
  await BrowserTestUtils.browserLoaded(browser1);

  let blobURL;

  info("Creating a blob URL...");
  await ContentTask.spawn(browser1, null, function() {
    return Promise.resolve(content.window.URL.createObjectURL(new content.window.Blob([123])));
  }).then(newURL => { blobURL = newURL });

  info("Blob URL: " + blobURL);

  info("Creating a tab with UCI = 2...");
  let tab2 = BrowserTestUtils.addTab(gBrowser, BASE_URI, {userContextId: 2});
  is(tab2.getAttribute("usercontextid"), 2, "New tab has UCI equal 2");

  let browser2 = gBrowser.getBrowserForTab(tab2);
  await BrowserTestUtils.browserLoaded(browser2);

  await ContentTask.spawn(browser2, blobURL, function(url) {
    return new Promise(resolve => {
      var xhr = new content.window.XMLHttpRequest();
      xhr.onerror = function() { resolve("SendErrored"); }
      xhr.onload = function() { resolve("SendLoaded"); }
      xhr.open("GET", url);
      xhr.send();
    });
  }).then(status => {
    is(status, "SendErrored", "Using a blob URI from one user context id in another should not work");
  });

  info("Creating a tab with UCI = 1...");
  let tab3 = BrowserTestUtils.addTab(gBrowser, BASE_URI, {userContextId: 1});
  is(tab3.getAttribute("usercontextid"), 1, "New tab has UCI equal 1");

  let browser3 = gBrowser.getBrowserForTab(tab3);
  await BrowserTestUtils.browserLoaded(browser3);

  await ContentTask.spawn(browser3, blobURL, function(url) {
    return new Promise(resolve => {
      var xhr = new content.window.XMLHttpRequest();
      xhr.open("GET", url);
      try {
        xhr.send();
        resolve("SendSucceeded");
      } catch (e) {
        resolve("SendThrew");
      }
    });
  }).then(status => {
    is(status, "SendSucceeded", "Using a blob URI within a single user context id should work");
  });

  await BrowserTestUtils.removeTab(tab1);
  await BrowserTestUtils.removeTab(tab2);
  await BrowserTestUtils.removeTab(tab3);
});
