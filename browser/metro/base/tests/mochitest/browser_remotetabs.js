/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Globals

Components.utils.import("resource://services-sync/main.js");
////////////////////////////////////////////////////////////////////////////////
//// Test(s)

function test() {
  is(Weave.Status.checkSetup(), Weave.CLIENT_NOT_CONFIGURED, "Sync should be disabled on start");
  // check start page is hidden

  let vbox = document.getElementById("start-remotetabs");
  ok(vbox.hidden, "remote tabs in the start page should be hidden when sync is not enabled");
  // check container link is hidden
  let menulink = document.getElementById("menuitem-remotetabs");
  ok(menulink.hidden, "link to container should be hidden when sync is not enabled");

  // hacky-fake sync setup and enabled. Note the Sync Tracker will spit
  // a number of warnings about undefined ids
  Weave.Status._authManager.username = "jane doe"; // must set username before key
  Weave.Status._authManager.basicPassword = "goatcheesesalad";
  Weave.Status._authManager.syncKey = "a-bcdef-abcde-acbde-acbde-acbde";
  // check that it worked
  isnot(Weave.Status.checkSetup(), Weave.CLIENT_NOT_CONFIGURED, "Sync is enabled");
  Weave.Svc.Obs.notify("weave:service:setup-complete");

  // start page grid should be visible
  ok(vbox, "remote tabs grid is present on start page");
  //PanelUI.show("remotetabs-container");
  is(vbox.hidden, false, "remote tabs should be visible in start page when sync is enabled");
  // container link should be visible
  is(menulink.hidden, false, "link to container should be visible when sync is enabled");

  // hacky-fake sync disable
  Weave.Status._authManager.deleteSyncCredentials();
  Weave.Svc.Obs.notify("weave:service:start-over");
  is(Weave.Status.checkSetup(), Weave.CLIENT_NOT_CONFIGURED, "Sync has been disabled");
  ok(vbox.hidden, "remote tabs in the start page should be hidden when sync is not enabled");
  ok(menulink.hidden, "link to container should be hidden when sync is not enabled");

}
