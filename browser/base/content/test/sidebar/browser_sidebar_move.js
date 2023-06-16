registerCleanupFunction(() => {
  Services.prefs.clearUserPref("sidebar.position_start");
  SidebarUI.hide();
});

const EXPECTED_START_ORDINALS = [
  ["sidebar-box", 1],
  ["sidebar-splitter", 2],
  ["appcontent", 3],
];

const EXPECTED_END_ORDINALS = [
  ["sidebar-box", 3],
  ["sidebar-splitter", 2],
  ["appcontent", 1],
];

function getBrowserChildrenWithOrdinals() {
  let browser = document.getElementById("browser");
  return [...browser.children].map(node => {
    return [node.id, node.style.order];
  });
}

add_task(async function () {
  await SidebarUI.show("viewBookmarksSidebar");
  SidebarUI.showSwitcherPanel();

  let reversePositionButton = document.getElementById(
    "sidebar-reverse-position"
  );
  let originalLabel = reversePositionButton.getAttribute("label");
  let box = document.getElementById("sidebar-box");

  // Default (position: left)
  Assert.deepEqual(
    getBrowserChildrenWithOrdinals(),
    EXPECTED_START_ORDINALS,
    "Correct ordinal (start)"
  );
  ok(!box.hasAttribute("positionend"), "Positioned start");

  // Moved to right
  SidebarUI.reversePosition();
  SidebarUI.showSwitcherPanel();
  Assert.deepEqual(
    getBrowserChildrenWithOrdinals(),
    EXPECTED_END_ORDINALS,
    "Correct ordinal (end)"
  );
  isnot(
    reversePositionButton.getAttribute("label"),
    originalLabel,
    "Label changed"
  );
  ok(box.hasAttribute("positionend"), "Positioned end");

  // Moved to back to left
  SidebarUI.reversePosition();
  SidebarUI.showSwitcherPanel();
  Assert.deepEqual(
    getBrowserChildrenWithOrdinals(),
    EXPECTED_START_ORDINALS,
    "Correct ordinal (start)"
  );
  ok(!box.hasAttribute("positionend"), "Positioned start");
  is(
    reversePositionButton.getAttribute("label"),
    originalLabel,
    "Label is back to normal"
  );
});
