
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("sidebar.position_start");
  SidebarUI.hide();
});

const EXPECTED_START_ORDINALS = [
  ["browser-border-start", 1],
  ["sidebar-box", 2],
  ["sidebar-splitter", 3],
  ["appcontent", 4],
  ["browser-border-end", 5],
];

const EXPECTED_END_ORDINALS = [
  ["browser-border-start", 1],
  ["sidebar-box", 4],
  ["sidebar-splitter", 3],
  ["appcontent", 2],
  ["browser-border-end", 5],
];

function getBrowserChildrenWithOrdinals() {
  let browser = document.getElementById("browser");
  return [...browser.childNodes].map(node => {
    return [node.id, node.ordinal];
  });
}

add_task(async function() {
  await SidebarUI.show("viewBookmarksSidebar");
  SidebarUI.showSwitcherPanel();

  let reversePositionButton = document.getElementById("sidebar-reverse-position");
  let originalLabel = reversePositionButton.getAttribute("label");
  let box = document.getElementById("sidebar-box");

  // Default (position: left)
  Assert.deepEqual(getBrowserChildrenWithOrdinals(),
    EXPECTED_START_ORDINALS, "Correct ordinal (start)");
  ok(!box.hasAttribute("positionend"), "Positioned start");

  // Moved to right
  SidebarUI.reversePosition();
  SidebarUI.showSwitcherPanel();
  Assert.deepEqual(getBrowserChildrenWithOrdinals(),
    EXPECTED_END_ORDINALS, "Correct ordinal (end)");
  isnot(reversePositionButton.getAttribute("label"), originalLabel, "Label changed");
  ok(box.hasAttribute("positionend"), "Positioned end");

  // Moved to back to left
  SidebarUI.reversePosition();
  SidebarUI.showSwitcherPanel();
  Assert.deepEqual(getBrowserChildrenWithOrdinals(),
    EXPECTED_START_ORDINALS, "Correct ordinal (start)");
  ok(!box.hasAttribute("positionend"), "Positioned start");
  is(reversePositionButton.getAttribute("label"), originalLabel, "Label is back to normal");
});
