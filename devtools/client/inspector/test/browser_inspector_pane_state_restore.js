/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the previous inspector split box sizes are restored when reopening the
// inspector.

const EXPECTED_INITIAL_WIDTH = 101;
const EXPECTED_INITIAL_HEIGHT = 102;
const EXPECTED_INITIAL_SIDEBAR_WIDTH = 103;

const EXPECTED_NEW_WIDTH = 150;
const EXPECTED_NEW_HEIGHT = 100;
const EXPECTED_NEW_SIDEBAR_WIDTH = 250;

add_task(async function() {
  // Simulate that the user has already stored their preferred split boxes widths.
  await pushPref(
    "devtools.toolsidebar-width.inspector",
    EXPECTED_INITIAL_WIDTH
  );
  await pushPref(
    "devtools.toolsidebar-height.inspector",
    EXPECTED_INITIAL_HEIGHT
  );
  await pushPref(
    "devtools.toolsidebar-width.inspector.splitsidebar",
    EXPECTED_INITIAL_SIDEBAR_WIDTH
  );

  const { inspector } = await openInspectorForURL("about:blank");

  info("Check the initial size of the inspector.");
  const { width, height, splitSidebarWidth } = inspector.getSidebarSize();
  is(width, EXPECTED_INITIAL_WIDTH, "Got correct initial width.");
  is(height, EXPECTED_INITIAL_HEIGHT, "Got correct initial height.");
  is(
    splitSidebarWidth,
    EXPECTED_INITIAL_SIDEBAR_WIDTH,
    "Got correct initial split sidebar width."
  );

  info("Simulate updates to the dimensions of the various splitboxes.");
  inspector.splitBox.setState({
    width: EXPECTED_NEW_WIDTH,
    height: EXPECTED_NEW_HEIGHT,
  });
  inspector.sidebarSplitBoxRef.current.setState({
    width: EXPECTED_NEW_SIDEBAR_WIDTH,
  });

  await closeToolbox();

  info(
    "Check the stored sizes of the inspector in the preferences when the inspector " +
      "is closed"
  );
  const storedWidth = Services.prefs.getIntPref(
    "devtools.toolsidebar-width.inspector"
  );
  const storedHeight = Services.prefs.getIntPref(
    "devtools.toolsidebar-height.inspector"
  );
  const storedSplitSidebarWidth = Services.prefs.getIntPref(
    "devtools.toolsidebar-width.inspector.splitsidebar"
  );
  is(storedWidth, EXPECTED_NEW_WIDTH, "Got correct stored width.");
  is(storedHeight, EXPECTED_NEW_HEIGHT, "Got correct stored height");
  is(
    storedSplitSidebarWidth,
    EXPECTED_NEW_SIDEBAR_WIDTH,
    "Got correct stored split sidebar width."
  );
});
