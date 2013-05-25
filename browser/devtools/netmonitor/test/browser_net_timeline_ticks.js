/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if timeline correctly displays interval divisions.
 */

function test() {
  initNetMonitor(SIMPLE_URL).then(([aTab, aDebuggee, aMonitor]) => {
    info("Starting test... ");

    let { document, L10N, NetMonitorView } = aMonitor.panelWin;
    let { RequestsMenu } = NetMonitorView;

    RequestsMenu.lazyUpdate = false;

    ok(document.querySelector("#requests-menu-waterfall-label"),
      "An timeline label should be displayed when the frontend is opened.");
    ok(document.querySelectorAll(".requests-menu-timings-division").length == 0,
      "No tick labels should be displayed when the frontend is opened.");

    ok(!RequestsMenu._canvas,
      "No canvas should be created when the frontend is opened.");
    ok(!RequestsMenu._ctx,
      "No 2d context should be created when the frontend is opened.");

    waitForNetworkEvents(aMonitor, 1).then(() => {
      ok(!document.querySelector("#requests-menu-waterfall-label"),
        "The timeline label should be hidden after the first request.");
      ok(document.querySelectorAll(".requests-menu-timings-division").length >= 3,
        "There should be at least 3 tick labels in the network requests header.");

      is(document.querySelectorAll(".requests-menu-timings-division")[0]
        .getAttribute("value"), L10N.getFormatStr("networkMenu.divisionMS", 0),
        "The first tick label has an incorrect value");
      is(document.querySelectorAll(".requests-menu-timings-division")[1]
        .getAttribute("value"), L10N.getFormatStr("networkMenu.divisionMS", 80),
        "The second tick label has an incorrect value");
      is(document.querySelectorAll(".requests-menu-timings-division")[2]
        .getAttribute("value"), L10N.getFormatStr("networkMenu.divisionMS", 160),
        "The third tick label has an incorrect value");

      is(document.querySelectorAll(".requests-menu-timings-division")[0]
        .style.transform, "translateX(0px)",
        "The first tick label has an incorrect translation");
      is(document.querySelectorAll(".requests-menu-timings-division")[1]
        .style.transform, "translateX(80px)",
        "The second tick label has an incorrect translation");
      is(document.querySelectorAll(".requests-menu-timings-division")[2]
        .style.transform, "translateX(160px)",
        "The third tick label has an incorrect translation");

      ok(RequestsMenu._canvas,
        "A canvas should be created after the first request.");
      ok(RequestsMenu._ctx,
        "A 2d context should be created after the first request.");

      let imageData = RequestsMenu._ctx.getImageData(0, 0, 161, 1);
      ok(imageData, "The image data should have been created.");

      let data = imageData.data;
      ok(data, "The image data should contain a pixel array.");

      ok( hasPixelAt(0), "The tick at 0 is should not be empty.");
      ok(!hasPixelAt(1), "The tick at 1 is should be empty.");
      ok(!hasPixelAt(19), "The tick at 19 is should be empty.");
      ok( hasPixelAt(20), "The tick at 20 is should not be empty.");
      ok(!hasPixelAt(21), "The tick at 21 is should be empty.");
      ok(!hasPixelAt(39), "The tick at 39 is should be empty.");
      ok( hasPixelAt(40), "The tick at 40 is should not be empty.");
      ok(!hasPixelAt(41), "The tick at 41 is should be empty.");
      ok(!hasPixelAt(59), "The tick at 59 is should be empty.");
      ok( hasPixelAt(60), "The tick at 60 is should not be empty.");
      ok(!hasPixelAt(61), "The tick at 61 is should be empty.");
      ok(!hasPixelAt(79), "The tick at 79 is should be empty.");
      ok( hasPixelAt(80), "The tick at 80 is should not be empty.");
      ok(!hasPixelAt(81), "The tick at 81 is should be empty.");
      ok(!hasPixelAt(159), "The tick at 159 is should be empty.");
      ok( hasPixelAt(160), "The tick at 160 is should not be empty.");
      ok(!hasPixelAt(161), "The tick at 161 is should be empty.");

      ok(isPixelBrighterAtThan(0, 20),
        "The tick at 0 should be brighter than the one at 20");
      ok(isPixelBrighterAtThan(40, 20),
        "The tick at 40 should be brighter than the one at 20");
      ok(isPixelBrighterAtThan(40, 60),
        "The tick at 40 should be brighter than the one at 60");
      ok(isPixelBrighterAtThan(80, 60),
        "The tick at 80 should be brighter than the one at 60");

      ok(isPixelBrighterAtThan(80, 100),
        "The tick at 80 should be brighter than the one at 100");
      ok(isPixelBrighterAtThan(120, 100),
        "The tick at 120 should be brighter than the one at 100");
      ok(isPixelBrighterAtThan(120, 140),
        "The tick at 120 should be brighter than the one at 140");
      ok(isPixelBrighterAtThan(160, 140),
        "The tick at 160 should be brighter than the one at 140");

      ok(isPixelEquallyBright(20, 60),
        "The tick at 20 should be equally bright to the one at 60");
      ok(isPixelEquallyBright(100, 140),
        "The tick at 100 should be equally bright to the one at 140");

      ok(isPixelEquallyBright(40, 120),
        "The tick at 40 should be equally bright to the one at 120");

      ok(isPixelEquallyBright(0, 80),
        "The tick at 80 should be equally bright to the one at 160");
      ok(isPixelEquallyBright(80, 160),
        "The tick at 80 should be equally bright to the one at 160");

      function hasPixelAt(x) {
        let i = (x | 0) * 4;
        return data[i] && data[i + 1] && data[i + 2] && data[i + 3];
      }

      function isPixelBrighterAtThan(x1, x2) {
        let i = (x1 | 0) * 4;
        let j = (x2 | 0) * 4;
        return data[i + 3] > data [j + 3];
      }

      function isPixelEquallyBright(x1, x2) {
        let i = (x1 | 0) * 4;
        let j = (x2 | 0) * 4;
        return data[i + 3] == data [j + 3];
      }

      teardown(aMonitor).then(finish);
    });

    aDebuggee.location.reload();
  });
}
