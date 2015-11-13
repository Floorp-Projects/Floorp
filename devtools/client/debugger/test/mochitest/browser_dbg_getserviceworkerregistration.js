var TAB_URL = EXAMPLE_URL + "getserviceworkerregistration/doc_getserviceworkerregistration-tab.html";
var WORKER_URL = "../code_getserviceworkerregistration-worker.js";
var SCOPE1_URL = EXAMPLE_URL;
var SCOPE2_URL = EXAMPLE_URL + "getserviceworkerregistration/";

function test() {
  SpecialPowers.pushPrefEnv({'set': [
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ]}, function () {
    Task.spawn(function* () {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();

      let client = new DebuggerClient(DebuggerServer.connectPipe());
      yield connect(client);

      let tab = yield addTab(TAB_URL);
      let { tabs } = yield listTabs(client);
      let [, tabClient] = yield attachTab(client, findTab(tabs, TAB_URL));

      info("Check that the getting service worker registration is initially " +
           "empty.");
      let { registration } = yield getServiceWorkerRegistration(tabClient);
      is(registration, null);

      info("Register a service worker in the same scope as the page, and " +
           "check that it becomes the current service worker registration.");
      executeSoon(() => {
        evalInTab(tab, "promise1 = navigator.serviceWorker.register('" +
                        WORKER_URL + "', { scope: '" + SCOPE1_URL + "' });");
      });
      yield waitForServiceWorkerRegistrationChanged(tabClient);
      ({ registration } = yield getServiceWorkerRegistration(tabClient));
      is(registration.scope, SCOPE1_URL);

      info("Register a second service worker with a more specific scope, and " +
           "check that it becomes the current service worker registration.");
      executeSoon(() => {
        evalInTab(tab, "promise2 = promise1.then(function () { " +
                       "return navigator.serviceWorker.register('" +
                       WORKER_URL + "', { scope: '" + SCOPE2_URL + "' }); });");
      });
      yield waitForServiceWorkerRegistrationChanged(tabClient);
      ({ registration } = yield getServiceWorkerRegistration(tabClient));
      is(registration.scope, SCOPE2_URL);

      info("Unregister the second service worker, and check that the " +
           "first service worker becomes the current service worker " +
           "registration again.");
      executeSoon(() => {
        evalInTab(tab, "promise2.then(function (registration) { " +
                       "registration.unregister(); });")
      });
      yield waitForServiceWorkerRegistrationChanged(tabClient);
      ({ registration } = yield getServiceWorkerRegistration(tabClient));
      is(registration.scope, SCOPE1_URL);

      info("Unregister the first service worker, and check that the current " +
           "service worker registration becomes empty again.");
      executeSoon(() => {
        evalInTab(tab, "promise1.then(function (registration) { " +
                       "registration.unregister(); });");
      });
      yield waitForServiceWorkerRegistrationChanged(tabClient);
      ({ registration } = yield getServiceWorkerRegistration(tabClient));
      is(registration, null);

      yield close(client);
      finish();
    });
  });
}
