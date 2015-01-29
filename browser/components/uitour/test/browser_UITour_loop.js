/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let gTestTab;
let gContentAPI;
let gContentWindow;
let loopButton;
let loopPanel = document.getElementById("loop-notification-panel");

Components.utils.import("resource:///modules/UITour.jsm");
const { LoopRooms } = Components.utils.import("resource:///modules/loop/LoopRooms.jsm", {});

function test() {
  UITourTest();
}

let tests = [
  taskify(function* test_menu_show_hide() {
    ise(loopButton.open, false, "Menu should initially be closed");
    gContentAPI.showMenu("loop");

    yield waitForConditionPromise(() => {
      return loopButton.open;
    }, "Menu should be visible after showMenu()");

    ok(loopPanel.hasAttribute("noautohide"), "@noautohide should be on the loop panel");
    ok(loopPanel.hasAttribute("panelopen"), "The panel should have @panelopen");
    is(loopPanel.state, "open", "The panel should be open");
    ok(loopButton.hasAttribute("open"), "Loop button should know that the menu is open");

    gContentAPI.hideMenu("loop");
    yield waitForConditionPromise(() => {
        return !loopButton.open;
    }, "Menu should be hidden after hideMenu()");

    checkLoopPanelIsHidden();
  }),
  // Test the menu was cleaned up in teardown.
  taskify(function* setup_menu_cleanup() {
    gContentAPI.showMenu("loop");

    yield waitForConditionPromise(() => {
      return loopButton.open;
    }, "Menu should be visible after showMenu()");

    // Leave it open so it gets torn down and we can test below that teardown was succesful.
  }),
  taskify(function* test_menu_cleanup() {
    // Test that the open menu from above was torn down fully.
    checkLoopPanelIsHidden();
  }),
  function test_availableTargets(done) {
    gContentAPI.showMenu("loop");
    gContentAPI.getConfiguration("availableTargets", (data) => {
      for (let targetName of ["loop-newRoom", "loop-roomList", "loop-signInUpLink"]) {
        isnot(data.targets.indexOf(targetName), -1, targetName + " should exist");
      }
      done();
    });
  },
  function test_getConfigurationLoop(done) {
    let gettingStartedSeen = Services.prefs.getBoolPref("loop.gettingStarted.seen");
    gContentAPI.getConfiguration("loop", (data) => {
      is(data.gettingStartedSeen, gettingStartedSeen,
         "The configuration property should equal that of the pref");
      done();
    });
  },
  function test_hideMenuHidesAnnotations(done) {
    let infoPanel = document.getElementById("UITourTooltip");
    let highlightPanel = document.getElementById("UITourHighlightContainer");

    gContentAPI.showMenu("loop", function menuCallback() {
      gContentAPI.showHighlight("loop-roomList");
      gContentAPI.showInfo("loop-newRoom", "Make a new room", "AKA. conversation");
      UITour.getTarget(window, "loop-newRoom").then((target) => {
        waitForPopupAtAnchor(infoPanel, target.node, Task.async(function* checkPanelIsOpen() {
          isnot(loopPanel.state, "closed", "Loop panel should still be open");
          ok(loopPanel.hasAttribute("noautohide"), "@noautohide should still be on the loop panel");
          is(highlightPanel.getAttribute("targetName"), "loop-roomList", "Check highlight @targetname");
          is(infoPanel.getAttribute("targetName"), "loop-newRoom", "Check info panel @targetname");

          info("Close the loop menu and make sure the annotations inside disappear");
          let hiddenPromises = [promisePanelElementHidden(window, infoPanel),
                                promisePanelElementHidden(window, highlightPanel)];
          gContentAPI.hideMenu("loop");
          yield Promise.all(hiddenPromises);
          isnot(infoPanel.state, "open", "Info panel should have automatically hid");
          isnot(highlightPanel.state, "open", "Highlight panel should have automatically hid");
          done();
        }), "Info panel should be anchored to the new room button");
      });
    });
  },
  function test_notifyLoopChatWindowOpenedClosed(done) {
    gContentAPI.observe((event, params) => {
      is(event, "Loop:ChatWindowOpened", "Check Loop:ChatWindowOpened notification");
      gContentAPI.observe((event, params) => {
        is(event, "Loop:ChatWindowShown", "Check Loop:ChatWindowShown notification");
        gContentAPI.observe((event, params) => {
          is(event, "Loop:ChatWindowClosed", "Check Loop:ChatWindowClosed notification");
          gContentAPI.observe((event, params) => {
            ok(false, "No more notifications should have arrived");
          });
        });
        done();
      });
      document.querySelector("#pinnedchats > chatbox").close();
    });
    LoopRooms.open("fakeTourRoom");
  },
  function test_notifyLoopRoomURLCopied(done) {
    gContentAPI.observe((event, params) => {
      is(event, "Loop:ChatWindowOpened", "Loop chat window should've opened");
      gContentAPI.observe((event, params) => {
        is(event, "Loop:ChatWindowShown", "Check Loop:ChatWindowShown notification");

        let chat = document.querySelector("#pinnedchats > chatbox");
        gContentAPI.observe((event, params) => {
          is(event, "Loop:RoomURLCopied", "Check Loop:RoomURLCopied notification");
          gContentAPI.observe((event, params) => {
            is(event, "Loop:ChatWindowClosed", "Check Loop:ChatWindowClosed notification");
          });
          chat.close();
          done();
        });
        chat.content.contentDocument.querySelector(".btn-copy").click();
      });
    });
    setupFakeRoom();
    LoopRooms.open("fakeTourRoom");
  },
  function test_notifyLoopRoomURLEmailed(done) {
    gContentAPI.observe((event, params) => {
      is(event, "Loop:ChatWindowOpened", "Loop chat window should've opened");
      gContentAPI.observe((event, params) => {
        is(event, "Loop:ChatWindowShown", "Check Loop:ChatWindowShown notification");

        let chat = document.querySelector("#pinnedchats > chatbox");
        let composeEmailCalled = false;

        gContentAPI.observe((event, params) => {
          is(event, "Loop:RoomURLEmailed", "Check Loop:RoomURLEmailed notification");
          ok(composeEmailCalled, "mozLoop.composeEmail should be called");
          gContentAPI.observe((event, params) => {
            is(event, "Loop:ChatWindowClosed", "Check Loop:ChatWindowClosed notification");
          });
          chat.close();
          done();
        });

        let chatWin = chat.content.contentWindow;
        let oldComposeEmail = chatWin.navigator.wrappedJSObject.mozLoop.composeEmail;
        chatWin.navigator.wrappedJSObject.mozLoop.composeEmail = function(recipient, subject, body) {
          ok(recipient, "composeEmail should be invoked with at least a recipient value");
          composeEmailCalled = true;
          chatWin.navigator.wrappedJSObject.mozLoop.composeEmail = oldComposeEmail;
        };
        chatWin.document.querySelector(".btn-email").click();
      });
    });
    LoopRooms.open("fakeTourRoom");
  },
  taskify(function* test_arrow_panel_position() {
    ise(loopButton.open, false, "Menu should initially be closed");
    let popup = document.getElementById("UITourTooltip");

    yield showMenuPromise("loop");

    let currentTarget = "loop-newRoom";
    yield showInfoPromise(currentTarget, "This is " + currentTarget, "My arrow should be on the side");
    is(popup.popupBoxObject.alignmentPosition, "start_before", "Check " + currentTarget + " position");

    currentTarget = "loop-roomList";
    yield showInfoPromise(currentTarget, "This is " + currentTarget, "My arrow should be on the side");
    is(popup.popupBoxObject.alignmentPosition, "start_before", "Check " + currentTarget + " position");

    currentTarget = "loop-signInUpLink";
    yield showInfoPromise(currentTarget, "This is " + currentTarget, "My arrow should be underneath");
    is(popup.popupBoxObject.alignmentPosition, "after_end", "Check " + currentTarget + " position");
  }),
  taskify(function* test_setConfiguration() {
    is(Services.prefs.getBoolPref("loop.gettingStarted.resumeOnFirstJoin"), false, "pref should be false but exist");
    gContentAPI.setConfiguration("Loop:ResumeTourOnFirstJoin", true);

    yield waitForConditionPromise(() => {
      return Services.prefs.getBoolPref("loop.gettingStarted.resumeOnFirstJoin");
    }, "Pref should change to true via setConfiguration");

    Services.prefs.clearUserPref("loop.gettingStarted.resumeOnFirstJoin");
  }),
  taskify(function* test_resumeViaMenuPanel_roomClosedTabOpen() {
    Services.prefs.setBoolPref("loop.gettingStarted.resumeOnFirstJoin", true);

    // Create a fake room and then add a fake non-owner participant
    let roomsMap = setupFakeRoom();
    roomsMap.get("fakeTourRoom").participants = [{
      owner: false,
    }];

    // Set the tour URL to be the current page with a different query param
    let gettingStartedURL = gTestTab.linkedBrowser.currentURI.resolve("?gettingstarted=1");
    Services.prefs.setCharPref("loop.gettingStarted.url", gettingStartedURL);

    let observationPromise = new Promise((resolve) => {
      gContentAPI.observe((event, params) => {
        is(event, "Loop:IncomingConversation", "Page should have been notified about incoming conversation");
        ise(params.conversationOpen, false, "conversationOpen should be false");
        is(gBrowser.selectedTab, gTestTab, "The same tab should be selected");
        resolve();
      });
    });

    // Now open the menu while that non-owner is in the fake room to trigger resuming the tour
    yield showMenuPromise("loop");

    yield observationPromise;
    Services.prefs.clearUserPref("loop.gettingStarted.resumeOnFirstJoin");
  }),
  taskify(function* test_resumeViaMenuPanel_roomClosedTabClosed() {
    Services.prefs.setBoolPref("loop.gettingStarted.resumeOnFirstJoin", true);

    // Create a fake room and then add a fake non-owner participant
    let roomsMap = setupFakeRoom();
    roomsMap.get("fakeTourRoom").participants = [{
      owner: false,
    }];

    // Set the tour URL to a page that's not open yet
    Services.prefs.setCharPref("loop.gettingStarted.url", gBrowser.currentURI.prePath);

    let newTabPromise = waitForConditionPromise(() => {
      return gBrowser.currentURI.path.contains("incomingConversation=waiting");
    }, "New tab with incomingConversation=waiting should have opened");

    // Now open the menu while that non-owner is in the fake room to trigger resuming the tour
    yield showMenuPromise("loop");

    yield newTabPromise;

    yield gBrowser.removeCurrentTab();
    Services.prefs.clearUserPref("loop.gettingStarted.resumeOnFirstJoin");
  }),
];

// End tests

function checkLoopPanelIsHidden() {
  ok(!loopPanel.hasAttribute("noautohide"), "@noautohide on the loop panel should have been cleaned up");
  ok(!loopPanel.hasAttribute("panelopen"), "The panel shouldn't have @panelopen");
  isnot(loopPanel.state, "open", "The panel shouldn't be open");
  is(loopButton.hasAttribute("open"), false, "Loop button should know that the panel is closed");
}

function setupFakeRoom() {
  let room = {};
  for (let prop of ["roomToken", "roomName", "roomOwner", "roomUrl", "participants"])
    room[prop] = "fakeTourRoom";
  let roomsMap = new Map([
    [room.roomToken, room]
  ]);
  LoopRooms.stubCache(roomsMap);
  return roomsMap;
}

if (Services.prefs.getBoolPref("loop.enabled")) {
  loopButton = window.LoopUI.toolbarButton.node;
  // The targets to highlight only appear after getting started is launched.
  Services.prefs.setBoolPref("loop.gettingStarted.seen", true);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("loop.gettingStarted.resumeOnFirstJoin");
    Services.prefs.clearUserPref("loop.gettingStarted.seen");
    Services.prefs.clearUserPref("loop.gettingStarted.url");

    // Copied from browser/components/loop/test/mochitest/head.js
    // Remove the iframe after each test. This also avoids mochitest complaining
    // about leaks on shutdown as we intentionally hold the iframe open for the
    // life of the application.
    let frameId = loopButton.getAttribute("notificationFrameId");
    let frame = document.getElementById(frameId);
    if (frame) {
      frame.remove();
    }

    // Remove the stubbed rooms
    LoopRooms.stubCache(null);
  });
} else {
  ok(true, "Loop is disabled so skip the UITour Loop tests");
  tests = [];
}
