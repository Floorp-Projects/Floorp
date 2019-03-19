/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the onmozbrowsertitlechange event works.
"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();
browserElementTestHelpers.allowTopLevelDataURINavigation();

function runTest() {
  var iframe1 = document.createElement("iframe");
  iframe1.setAttribute("mozbrowser", "true");
  document.body.appendChild(iframe1);

  // iframe2 is a red herring; we modify its title but don't listen for
  // titlechanges; we want to make sure that its titlechange events aren't
  // picked up by the listener on iframe1.
  var iframe2 = document.createElement("iframe");
  iframe2.setAttribute("mozbrowser", "true");
  document.body.appendChild(iframe2);

  // iframe3 is another red herring.  It's not a mozbrowser, so we shouldn't
  // get any titlechange events on it.
  var iframe3 = document.createElement("iframe");
  document.body.appendChild(iframe3);

  var numTitleChanges = 0;

  iframe1.addEventListener("mozbrowsertitlechange", function(e) {
    // Ignore empty titles; these come from about:blank.
    if (e.detail == "")
      return;

    numTitleChanges++;

    if (numTitleChanges == 1) {
      is(e.detail, "Title");
      SpecialPowers.getBrowserFrameMessageManager(iframe1)
                   .loadFrameScript("data:,content.document.title='New title';",
                                    /* allowDelayedLoad = */ false);
      SpecialPowers.getBrowserFrameMessageManager(iframe2)
                   .loadFrameScript("data:,content.document.title='BAD TITLE 2';",
                                    /* allowDelayedLoad = */ false);
    } else if (numTitleChanges == 2) {
      is(e.detail, "New title");
      iframe1.src = "data:text/html,<html><head><title>Title 3</title></head><body></body></html>";
    } else if (numTitleChanges == 3) {
      is(e.detail, "Title 3");
      SimpleTest.finish();
    } else {
      ok(false, "Too many titlechange events.");
    }
  });

  iframe3.addEventListener("mozbrowsertitlechange", function(e) {
    ok(false, "Should not get a titlechange event for iframe3.");
  });

  iframe1.src = "data:text/html,<html><head><title>Title</title></head><body></body></html>";
  iframe2.src = "data:text/html,<html><head><title>BAD TITLE</title></head><body></body></html>";
  iframe3.src = "data:text/html,<html><head><title>SHOULD NOT GET EVENT</title></head><body></body></html>";
}

addEventListener("testready", runTest);
