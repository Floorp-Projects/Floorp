/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gTestTab;
var gContentAPI;
var gContentWindow;
var gMessageHandlers;
var loopButton;
var fakeRoom;
var loopPanel = document.getElementById("loop-notification-panel");

const { LoopAPI } = Cu.import("chrome://loop/content/modules/MozLoopAPI.jsm", {});
const { LoopRooms } = Cu.import("chrome://loop/content/modules/LoopRooms.jsm", {});
const { MozLoopServiceInternal } = Cu.import("chrome://loop/content/modules/MozLoopService.jsm", {});

const FTU_VERSION = 1;

function test() {
  UITourTest();
}

function runOffline(fun) {
  return (done) => {
    Services.io.offline = true;
    fun(function onComplete() {
      Services.io.offline = false;
      done();
    });
  }
}

var tests = [
  taskify(function* test_gettingStartedClicked_linkOpenedWithExpectedParams() {
    // Set latestFTUVersion to lower number to show FTU panel.
    Services.prefs.setIntPref("loop.gettingStarted.latestFTUVersion", 0);
    Services.prefs.setCharPref("loop.gettingStarted.url", "http://example.com");
    is(loopButton.open, false, "Menu should initially be closed");
    loopButton.click();

    yield waitForConditionPromise(() => {
      return loopButton.open;
    }, "Menu should be visible after showMenu()");

    gContentAPI.registerPageID("hello-tour_OpenPanel_testPage");
    yield new Promise(resolve => {
      gContentAPI.ping(() => resolve());
    });

    let loopDoc = document.getElementById("loop-notification-panel").children[0].contentDocument;
    yield waitForConditionPromise(() => {
      return loopDoc.readyState == 'complete';
    }, "Loop notification panel document should be fully loaded.", 50);
    let gettingStartedButton = loopDoc.getElementById("fte-button");
    ok(gettingStartedButton, "Getting Started button should be found");

    let newTabPromise = waitForConditionPromise(() => {
      return gBrowser.currentURI.path.includes("utm_source=firefox-browser");
    }, "New tab with utm_content=testPageNewID should have opened");

    gettingStartedButton.click();
    yield newTabPromise;
    ok(gBrowser.currentURI.path.includes("utm_content=hello-tour_OpenPanel_testPage"),
        "Expected URL opened (" + gBrowser.currentURI.path + ")");
    yield gBrowser.removeCurrentTab();

    checkLoopPanelIsHidden();
  }),
  taskify(function* test_gettingStartedClicked_linkOpenedWithExpectedParams2() {
    // Set latestFTUVersion to lower number to show FTU panel.
    Services.prefs.setIntPref("loop.gettingStarted.latestFTUVersion", 0);
    // Force a refresh of the loop panel since going from seen -> unseen doesn't trigger
    // automatic re-rendering.
    let loopWin = document.getElementById("loop-notification-panel").children[0].contentWindow;
    var event = new loopWin.CustomEvent("GettingStartedSeen", { detail: false });
    loopWin.dispatchEvent(event);

    UITour.pageIDsForSession.clear();
    Services.prefs.setCharPref("loop.gettingStarted.url", "http://example.com");
    is(loopButton.open, false, "Menu should initially be closed");
    loopButton.click();

    yield waitForConditionPromise(() => {
      return loopButton.open;
    }, "Menu should be visible after showMenu()");


    gContentAPI.registerPageID("hello-tour_OpenPanel_testPageOldId");
    yield new Promise(resolve => {
      gContentAPI.ping(() => resolve());
    });
    // Set the time of the page ID to 10 hours earlier, so that it is considered "expired".
    UITour.pageIDsForSession.set("hello-tour_OpenPanel_testPageOldId",
                                   {lastSeen: Date.now() - (10 * 60 * 60 * 1000)});

    let loopDoc = loopWin.document;
    let gettingStartedButton = loopDoc.getElementById("fte-button");
    ok(gettingStartedButton, "Getting Started button should be found");

    let newTabPromise = waitForConditionPromise(() => {
      Services.console.logStringMessage(gBrowser.currentURI.path);
      return gBrowser.currentURI.path.includes("utm_source=firefox-browser");
    }, "New tab with utm_content=testPageNewID should have opened");

    gettingStartedButton.click();
    yield newTabPromise;
    ok(!gBrowser.currentURI.path.includes("utm_content=hello-tour_OpenPanel_testPageOldId"),
       "Expected URL opened without the utm_content parameter (" +
        gBrowser.currentURI.path + ")");
    yield gBrowser.removeCurrentTab();

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
    let gettingStartedSeen = Services.prefs.getIntPref("loop.gettingStarted.latestFTUVersion") >= FTU_VERSION;
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
  runOffline(function test_notifyLoopChatWindowOpenedClosed(done) {
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
  }),
  runOffline(function test_notifyLoopRoomURLCopied(done) {
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

        let window = chat.content.contentWindow;
        waitForConditionPromise(
          () => chat.content.contentDocument.querySelector(".btn-copy"),
          "Copy button should be there"
        ).then(() => chat.content.contentDocument.querySelector(".btn-copy").click());
      });
    });
    LoopRooms.open("fakeTourRoom");
  }),
  runOffline(function test_notifyLoopRoomURLEmailed(done) {
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

        gMessageHandlers.ComposeEmail = function(message, reply) {
          let [subject, body, recipient] = message.data;
          ok(subject, "composeEmail should be invoked with at least a subject value");
          composeEmailCalled = true;
          reply();
        };

        waitForConditionPromise(
          () => chat.content.contentDocument.querySelector(".btn-email"),
          "Email button should be there"
        ).then(() => chat.content.contentDocument.querySelector(".btn-email").click());
      });
    });
    LoopRooms.open("fakeTourRoom");
  }),
  taskify(function* test_arrow_panel_position() {
    is(loopButton.open, false, "Menu should initially be closed");
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
        is(params.conversationOpen, false, "conversationOpen should be false");
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
      return gBrowser.currentURI.path.includes("incomingConversation=waiting");
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
  let room = Object.create(fakeRoom);
  let roomsMap = new Map([
    [room.roomToken, room]
  ]);
  LoopRooms.stubCache(roomsMap);
  return roomsMap;
}

if (Services.prefs.getBoolPref("loop.enabled")) {
  loopButton = window.LoopUI.toolbarButton.node;

  fakeRoom = {
    decryptedContext: { roomName: "fakeTourRoom" },
    participants: [],
    maxSize: 2,
    ctime: Date.now()
  };
  for (let prop of ["roomToken", "roomOwner", "roomUrl"])
    fakeRoom[prop] = "fakeTourRoom";

  LoopAPI.stubMessageHandlers(gMessageHandlers = {
    // Stub the rooms object API to fully control the test behavior.
    "Rooms:*": function(action, message, reply) {
      switch (action.split(":").pop()) {
        case "GetAll":
          reply([fakeRoom]);
          break;
        case "Get":
          reply(fakeRoom);
          break;
        case "Join":
          reply({
            apiKey: "fakeTourRoom",
            sessionToken: "fakeTourRoom",
            sessionId: "fakeTourRoom",
            expires: Date.now() + 240000
          });
          break;
        case "RefreshMembership":
          reply({ expires: Date.now() + 240000 });
        default:
          reply();
      }
    },
    // Stub the metadata retrieval to suppress console warnings and return faster.
    GetSelectedTabMetadata: function(message, reply) {
      reply({ favicon: null });
    }
  });

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("loop.gettingStarted.resumeOnFirstJoin");
    Services.prefs.clearUserPref("loop.gettingStarted.latestFTUVersion");
    Services.prefs.clearUserPref("loop.gettingStarted.url");
    Services.io.offline = false;

    // Copied from browser/components/loop/test/mochitest/head.js
    // Remove the iframe after each test. This also avoids mochitest complaining
    // about leaks on shutdown as we intentionally hold the iframe open for the
    // life of the application.
    let frameId = loopButton.getAttribute("notificationFrameId");
    let frame = document.getElementById(frameId);
    if (frame) {
      frame.remove();
    }

    // Remove the stubbed rooms.
    LoopRooms.stubCache(null);
    // Restore the stubbed handlers.
    LoopAPI.restore();
  });
} else {
  ok(true, "Loop is disabled so skip the UITour Loop tests");
  tests = [];
}
