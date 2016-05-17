/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the empty-requests reload button works.
 */

 add_task(function* () {
   let [tab, debuggee, monitor] = yield initNetMonitor(SINGLE_GET_URL);
   info("Starting test... ");

   let { document, EVENTS, NetworkEventsHandler } = monitor.panelWin;
   let button = document.querySelector("#requests-menu-reload-notice-button");
   button.click();

   let deferred = promise.defer();
   let markers = [];

   monitor.panelWin.on(EVENTS.TIMELINE_EVENT, (_, marker) => {
     markers.push(marker);
   });

   yield waitForNetworkEvents(monitor, 2);
   yield waitUntil(() => markers.length == 2);

   ok(true, "Reloading finished");

   is(markers[0].name, "document::DOMContentLoaded",
    "The first received marker is correct.");
   is(markers[1].name, "document::Load",
    "The second received marker is correct.");

   teardown(monitor).then(finish);
 });

 function waitUntil(predicate, interval = 10) {
   if (predicate()) {
     return Promise.resolve(true);
   }
   return new Promise(resolve => {
     setTimeout(function () {
       waitUntil(predicate).then(() => resolve(true));
     }, interval);
   });
 }
