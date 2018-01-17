/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>Web Console test for " +
                 "observer notifications";

let created = false;
let destroyed = false;

add_task(async function () {
  setupObserver();
  await openNewTabAndConsole(TEST_URI);
  await waitFor(() => created);

  await closeTabAndToolbox(gBrowser.selectedTab);
  await waitFor(() => destroyed);
});

function setupObserver() {
  const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

  const observer = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

    observe: function observe(subject, topic) {
      subject = subject.QueryInterface(Ci.nsISupportsString);

      switch (topic) {
        case "web-console-created":
          ok(HUDService.getHudReferenceById(subject.data), "We have a hud reference");
          Services.obs.removeObserver(observer, "web-console-created");
          created = true;
          break;
        case "web-console-destroyed":
          ok(!HUDService.getHudReferenceById(subject.data),
            "We do not have a hud reference");
          Services.obs.removeObserver(observer, "web-console-destroyed");
          destroyed = true;
          break;
      }
    },
  };

  Services.obs.addObserver(observer, "web-console-created");
  Services.obs.addObserver(observer, "web-console-destroyed");
}
