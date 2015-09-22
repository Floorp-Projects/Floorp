/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if timing intervals are divided againts seconds when appropriate.
 */

function test() {
  initNetMonitor(CUSTOM_GET_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { $all, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    waitForNetworkEvents(aMonitor, 2).then(() => {
      let millisecondDivs = $all(".requests-menu-timings-division[division-scale=millisecond]");
      let secondDivs = $all(".requests-menu-timings-division[division-scale=second]");
      let minuteDivs = $all(".requests-menu-timings-division[division-scale=minute]");

      info("Number of millisecond divisions: " + millisecondDivs.length);
      info("Number of second divisions: " + secondDivs.length);
      info("Number of minute divisions: " + minuteDivs.length);

      for (let div of millisecondDivs) {
        info("Millisecond division: " + div.getAttribute("value"));
      }
      for (let div of secondDivs) {
        info("Second division: " + div.getAttribute("value"));
      }
      for (let div of minuteDivs) {
        info("Minute division: " + div.getAttribute("value"));
      }

      is(RequestsMenu.itemCount, 2,
        "There should be only two requests made.");

      let firstRequest = RequestsMenu.getItemAtIndex(0);
      let lastRequest = RequestsMenu.getItemAtIndex(1);

      info("First request happened at: " +
        firstRequest.attachment.responseHeaders.headers.find(e => e.name == "Date").value);
      info("Last request happened at: " +
        lastRequest.attachment.responseHeaders.headers.find(e => e.name == "Date").value);

      ok(secondDivs.length,
        "There should be at least one division on the seconds time scale.");
      ok(secondDivs[0].getAttribute("value").match(/\d+\.\d{2}\s\w+/),
        "The division on the seconds time scale looks legit.");

      teardown(aMonitor).then(finish);
    });

      // Timeout needed for having enough divisions on the time scale.
    aDebuggee.performRequests(2, null, 3000);
  });
}
