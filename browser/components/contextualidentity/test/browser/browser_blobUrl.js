"use strict";

// Here we want to test that blob URLs are not available cross containers.

const BASE_URI = "http://mochi.test:8888/browser/browser/components/"
  + "contextualidentity/test/browser/empty_file.html";

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true]
  ]});
});

add_task(function* test() {
  info("Creating a tab with UCI = 1...");
  let tab1 = gBrowser.addTab(BASE_URI, {userContextId: 1});
  is(tab1.getAttribute('usercontextid'), 1, "New tab has UCI equal 1");

  let browser1 = gBrowser.getBrowserForTab(tab1);
  yield BrowserTestUtils.browserLoaded(browser1);

  let blobURL;

  info("Creating a blob URL...");
  yield ContentTask.spawn(browser1, null, function() {
    return Promise.resolve(content.window.URL.createObjectURL(new content.window.Blob([123])));
  }).then(newURL => { blobURL = newURL });

  info("Blob URL: " + blobURL);

  info("Creating a tab with UCI = 2...");
  let tab2 = gBrowser.addTab(BASE_URI, {userContextId: 2});
  is(tab2.getAttribute('usercontextid'), 2, "New tab has UCI equal 2");

  let browser2 = gBrowser.getBrowserForTab(tab2);
  yield BrowserTestUtils.browserLoaded(browser2);

  yield ContentTask.spawn(browser2, blobURL, function(url) {
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
  let tab3 = gBrowser.addTab(BASE_URI, {userContextId: 1});
  is(tab3.getAttribute('usercontextid'), 1, "New tab has UCI equal 1");

  let browser3 = gBrowser.getBrowserForTab(tab3);
  yield BrowserTestUtils.browserLoaded(browser3);

  yield ContentTask.spawn(browser3, blobURL, function(url) {
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

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
  yield BrowserTestUtils.removeTab(tab3);
});
