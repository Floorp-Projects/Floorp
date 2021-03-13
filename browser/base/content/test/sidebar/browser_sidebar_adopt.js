/* This test checks that the SidebarFocused event doesn't fire in adopted
 * windows when the sidebar gets opened during window opening, to make sure
 * that sidebars don't steal focus from the page in this case (Bug 1394207).
 * There's another case not covered here that has the same expected behavior -
 * during the initial browser startup - but it would be hard to do with a mochitest. */

registerCleanupFunction(() => {
  SidebarUI.hide();
});

function failIfSidebarFocusedFires() {
  ok(false, "This event shouldn't have fired");
}

add_task(function setup() {
  if (CustomizableUI.protonToolbarEnabled) {
    CustomizableUI.addWidgetToArea("sidebar-button", "nav-bar");
    registerCleanupFunction(() =>
      CustomizableUI.removeWidgetFromArea("sidebar-button")
    );
  }
});

add_task(async function testAdoptedTwoWindows() {
  // First open a new window, show the sidebar in that window, and close it.
  // Then, open another new window and confirm that the sidebar is closed since it is
  // being adopted from the main window which doesn't have a shown sidebar. See Bug 1407737.
  info("Ensure that sidebar state is adopted only from the opener");

  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  await win1.SidebarUI.show("viewBookmarksSidebar");
  await BrowserTestUtils.closeWindow(win1);

  let win2 = await BrowserTestUtils.openNewBrowserWindow();
  ok(
    !win2.document.getElementById("sidebar-button").hasAttribute("checked"),
    "Sidebar button isn't checked"
  );
  ok(!win2.SidebarUI.isOpen, "Sidebar is closed");
  await BrowserTestUtils.closeWindow(win2);
});

add_task(async function testEventsReceivedInMainWindow() {
  info(
    "Opening the sidebar and expecting both SidebarShown and SidebarFocused events"
  );

  let initialShown = BrowserTestUtils.waitForEvent(window, "SidebarShown");
  let initialFocus = BrowserTestUtils.waitForEvent(window, "SidebarFocused");

  await SidebarUI.show("viewBookmarksSidebar");
  await initialShown;
  await initialFocus;

  ok(true, "SidebarShown and SidebarFocused events fired on a new window");
});

add_task(async function testEventReceivedInNewWindow() {
  info(
    "Opening a new window and expecting the SidebarFocused event to not fire"
  );

  let promiseNewWindow = BrowserTestUtils.waitForNewWindow();
  let win = OpenBrowserWindow();

  let adoptedShown = BrowserTestUtils.waitForEvent(win, "SidebarShown");
  win.addEventListener("SidebarFocused", failIfSidebarFocusedFires);
  registerCleanupFunction(async function() {
    win.removeEventListener("SidebarFocused", failIfSidebarFocusedFires);
    await BrowserTestUtils.closeWindow(win);
  });

  await promiseNewWindow;
  await adoptedShown;
  ok(true, "SidebarShown event fired on an adopted window");
});
