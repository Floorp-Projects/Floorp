/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests if the network monitor panes collapse properly.
 */

add_task(function* () {
  let [,, monitor] = yield initNetMonitor(SIMPLE_URL);
  info("Starting test... ");

  let { document, Prefs, NetMonitorView } = monitor.panelWin;
  let detailsPane = document.getElementById("details-pane");
  let detailsPaneToggleButton = document.getElementById("details-pane-toggle");

  ok(detailsPane.classList.contains("pane-collapsed") &&
     detailsPaneToggleButton.classList.contains("pane-collapsed"),
    "The details pane should initially be hidden.");

  NetMonitorView.toggleDetailsPane({ visible: true, animated: false });

  let width = ~~(detailsPane.getAttribute("width"));
  is(width, Prefs.networkDetailsWidth,
    "The details pane has an incorrect width.");
  is(detailsPane.style.marginLeft, "0px",
    "The details pane has an incorrect left margin.");
  is(detailsPane.style.marginRight, "0px",
    "The details pane has an incorrect right margin.");
  ok(!detailsPane.hasAttribute("animated"),
    "The details pane has an incorrect animated attribute.");
  ok(!detailsPane.classList.contains("pane-collapsed") &&
     !detailsPaneToggleButton.classList.contains("pane-collapsed"),
    "The details pane should at this point be visible.");

  NetMonitorView.toggleDetailsPane({ visible: false, animated: true });

  let margin = -(width + 1) + "px";
  is(width, Prefs.networkDetailsWidth,
    "The details pane has an incorrect width after collapsing.");
  is(detailsPane.style.marginLeft, margin,
    "The details pane has an incorrect left margin after collapsing.");
  is(detailsPane.style.marginRight, margin,
    "The details pane has an incorrect right margin after collapsing.");
  ok(detailsPane.hasAttribute("animated"),
    "The details pane has an incorrect attribute after an animated collapsing.");
  ok(detailsPane.classList.contains("pane-collapsed") &&
     detailsPaneToggleButton.classList.contains("pane-collapsed"),
    "The details pane should not be visible after collapsing.");

  NetMonitorView.toggleDetailsPane({ visible: true, animated: false });

  is(width, Prefs.networkDetailsWidth,
    "The details pane has an incorrect width after uncollapsing.");
  is(detailsPane.style.marginLeft, "0px",
    "The details pane has an incorrect left margin after uncollapsing.");
  is(detailsPane.style.marginRight, "0px",
    "The details pane has an incorrect right margin after uncollapsing.");
  ok(!detailsPane.hasAttribute("animated"),
    "The details pane has an incorrect attribute after an unanimated uncollapsing.");
  ok(!detailsPane.classList.contains("pane-collapsed") &&
     !detailsPaneToggleButton.classList.contains("pane-collapsed"),
    "The details pane should be visible again after uncollapsing.");

  yield teardown(monitor);
});
