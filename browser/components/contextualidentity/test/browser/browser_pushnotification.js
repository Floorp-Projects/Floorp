let { classes: Cc, interfaces: Ci } = Components;

const URI = "http://example.com/browser/browser/components/contextualidentity/test/browser/";

add_task(function* setup() {
  return SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true],
    ["dom.push.enabled", true],
    ["dom.push.connection.enabled", true],
    ["dom.push.maxRecentMessageIDsPerSubscription", 0],
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ]});
});

add_task(function* test() {
  info("Creation of a container tab...");
  let tab = gBrowser.addTab(URI + "empty_file.html", {userContextId: 1});
  let browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);

  info("Let's creating a second tab...");
  let tab2 = gBrowser.addTab(URI + "empty_file.html", {userContextId: 2});
  let browser2 = gBrowser.getBrowserForTab(tab2);
  yield BrowserTestUtils.browserLoaded(browser2);

  info("Registration of the ServiceWorker...");
  let data = yield ContentTask.spawn(browser, URI, function(url) {
    return new Promise((resolve, reject) => {
      content.navigator.serviceWorker.register(url + "pushworker.js", {scope: url})
      .then(
        (registration) => {
          resolve({ scope: registration.scope,
                    oa: content.document.nodePrincipal.originAttributes}); },
        () => { reject(); });
    });
  });

  info("Closing the tab...");
  yield BrowserTestUtils.removeTab(tab);

  info("Creating originAttributes suffix...");
  let originAttributes = self.ChromeUtils.originAttributesToSuffix(data.oa);

  info("Sending push event...");
  let swm = Cc["@mozilla.org/serviceworkers/manager;1"]
              .getService(Ci.nsIServiceWorkerManager);
  swm.sendPushEvent(originAttributes, data.scope);

  info("Let's wait for an answer on the second tab...");
  yield ContentTask.spawn(browser2, URI, function(url) {
    return new Promise((resolve, reject) => {
      function testResponse() {
        let xhr = new content.window.XMLHttpRequest();
        xhr.open("GET", url + "pushserver.sjs");
        xhr.send();
        xhr.onload = function() {
          if (xhr.response == 42) {
            ok("The SW answered correctly.");
            resolve();
          } else {
            setTimeout(testResponse, 1000);
          }
        }
      }

      testResponse();
    });
  });

  info("Creation of a container tab...");
  tab = gBrowser.addTab(URI + "empty_file.html", {userContextId: 1});
  browser = gBrowser.getBrowserForTab(tab);
  yield BrowserTestUtils.browserLoaded(browser);

  info("Unregistration of the ServiceWorker...");
  yield ContentTask.spawn(browser, URI, function(url) {
    return new Promise((resolve, reject) => {
      content.navigator.serviceWorker.getRegistration(url)
      .then((reg) => { return reg.unregister(url); })
      .then(resolve);
    });
  });

  info("Closing the first tab...");
  yield BrowserTestUtils.removeTab(tab);

  info("Closing the second tab...");
  yield BrowserTestUtils.removeTab(tab2);
});
