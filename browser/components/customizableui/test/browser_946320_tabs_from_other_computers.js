/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Preferences = Cu.import("resource://gre/modules/Preferences.jsm", {}).Preferences;
Cu.import("resource://gre/modules/Promise.jsm");

add_task(function() {
  let shownPanelPromise = promisePanelShown(window);
  PanelUI.toggle({type: "command"});
  yield shownPanelPromise;

  let historyButton = document.getElementById("history-panelmenu");
  let historySubview = document.getElementById("PanelUI-history");
  let subviewShownPromise = subviewShown(historySubview);
  EventUtils.synthesizeMouseAtCenter(historyButton, {});
  yield subviewShownPromise;

  let tabsFromOtherComputers = document.getElementById("sync-tabs-menuitem2");
  is(tabsFromOtherComputers.hidden, true, "The Tabs From Other Computers menuitem should be hidden when sync isn't enabled.");

  let subviewHiddenPromise = subviewHidden(historySubview);
  let panelMultiView = document.getElementById("PanelUI-multiView");
  panelMultiView.showMainView();
  yield subviewHiddenPromise;

  // Part 2 - When Sync is enabled the menuitem should be shown.
  Weave.Service.createAccount("john@doe.com", "mysecretpw",
                              "challenge", "response");
  Weave.Service.identity.account = "john@doe.com";
  Weave.Service.identity.basicPassword = "mysecretpw";
  Weave.Service.identity.syncKey = Weave.Utils.generatePassphrase();
  Weave.Svc.Prefs.set("firstSync", "newAccount");
  Weave.Service.persistLogin();

  subviewShownPromise = subviewShown(historySubview);
  EventUtils.synthesizeMouseAtCenter(historyButton, {});
  yield subviewShownPromise;

  is(tabsFromOtherComputers.hidden, false, "The Tabs From Other Computers menuitem should be shown when sync is enabled.");

  let syncPrefBranch = new Preferences("services.sync.");
  syncPrefBranch.resetBranch("");
  Services.logins.removeAllLogins();

  let hiddenPanelPromise = promisePanelHidden(window);
  PanelUI.toggle({type: "command"});
  yield hiddenPanelPromise;
});
