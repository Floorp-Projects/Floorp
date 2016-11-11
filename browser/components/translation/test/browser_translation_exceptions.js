/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// tests the translation infobar, using a fake 'Translation' implementation.

var tmp = {};
Cu.import("resource:///modules/translation/Translation.jsm", tmp);
Cu.import("resource://gre/modules/Promise.jsm", tmp);
var {Translation, Promise} = tmp;

const kLanguagesPref = "browser.translation.neverForLanguages";
const kShowUIPref = "browser.translation.ui.show";

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(kShowUIPref, true);
  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
    Services.prefs.clearUserPref(kShowUIPref);
  });
  tab.linkedBrowser.addEventListener("load", function onload() {
    tab.linkedBrowser.removeEventListener("load", onload, true);
    Task.spawn(function* () {
      for (let test of gTests) {
        info(test.desc);
        yield test.run();
      }
    }).then(finish, ex => {
     ok(false, "Unexpected Exception: " + ex);
     finish();
    });
   }, true);

  content.location = "http://example.com/";
}

function getLanguageExceptions() {
  let langs = Services.prefs.getCharPref(kLanguagesPref);
  return langs ? langs.split(",") : [];
}

function getDomainExceptions() {
  let results = [];
  let enumerator = Services.perms.enumerator;
  while (enumerator.hasMoreElements()) {
    let perm = enumerator.getNext().QueryInterface(Ci.nsIPermission);

    if (perm.type == "translate" &&
        perm.capability == Services.perms.DENY_ACTION)
      results.push(perm.principal);
  }

  return results;
}

function getInfoBar() {
  let deferred = Promise.defer();
  let infobar =
    gBrowser.getNotificationBox().getNotificationWithValue("translation");

  if (!infobar) {
    deferred.resolve();
  } else {
    // Wait for all animations to finish
    Promise.all(infobar.getAnimations().map(animation => animation.finished))
      .then(() => deferred.resolve(infobar));
  }

  return deferred.promise;
}

function openPopup(aPopup) {
  let deferred = Promise.defer();

  aPopup.addEventListener("popupshown", function popupShown() {
    aPopup.removeEventListener("popupshown", popupShown);
    deferred.resolve();
  });

  aPopup.focus();
  // One down event to open the popup.
  EventUtils.synthesizeKey("VK_DOWN",
                           { altKey: !navigator.platform.includes("Mac") });

  return deferred.promise;
}

function waitForWindowLoad(aWin) {
  let deferred = Promise.defer();

  aWin.addEventListener("load", function onload() {
    aWin.removeEventListener("load", onload, true);
    deferred.resolve();
  }, true);

  return deferred.promise;
}


var gTests = [

{
  desc: "clean exception lists at startup",
  run: function checkNeverForLanguage() {
    is(getLanguageExceptions().length, 0,
       "we start with an empty list of languages to never translate");
    is(getDomainExceptions().length, 0,
       "we start with an empty list of sites to never translate");
  }
},

{
  desc: "never for language",
  run: function* checkNeverForLanguage() {
    // Show the infobar for example.com and fr.
    Translation.documentStateReceived(gBrowser.selectedBrowser,
                                      {state: Translation.STATE_OFFER,
                                       originalShown: true,
                                       detectedLanguage: "fr"});
    let notif = yield getInfoBar();
    ok(notif, "the infobar is visible");
    let ui = gBrowser.selectedBrowser.translationUI;
    let uri = gBrowser.selectedBrowser.currentURI;
    ok(ui.shouldShowInfoBar(uri, "fr"),
       "check shouldShowInfoBar initially returns true");

    // Open the "options" drop down.
    yield openPopup(notif._getAnonElt("options"));
    ok(notif._getAnonElt("options").getAttribute("open"),
       "the options menu is open");

    // Check that the item is not disabled.
    ok(!notif._getAnonElt("neverForLanguage").disabled,
       "The 'Never translate <language>' item isn't disabled");

    // Click the 'Never for French' item.
    notif._getAnonElt("neverForLanguage").click();
    notif = yield getInfoBar();
    ok(!notif, "infobar hidden");

    // Check this has been saved to the exceptions list.
    let langs = getLanguageExceptions();
    is(langs.length, 1, "one language in the exception list");
    is(langs[0], "fr", "correct language in the exception list");
    ok(!ui.shouldShowInfoBar(uri, "fr"),
       "the infobar wouldn't be shown anymore");

    // Reopen the infobar.
    PopupNotifications.getNotification("translate").anchorElement.click();
    notif = yield getInfoBar();
    // Open the "options" drop down.
    yield openPopup(notif._getAnonElt("options"));
    ok(notif._getAnonElt("neverForLanguage").disabled,
       "The 'Never translate French' item is disabled");

    // Cleanup.
    Services.prefs.setCharPref(kLanguagesPref, "");
    notif.close();
  }
},

{
  desc: "never for site",
  run: function* checkNeverForSite() {
    // Show the infobar for example.com and fr.
    Translation.documentStateReceived(gBrowser.selectedBrowser,
                                      {state: Translation.STATE_OFFER,
                                       originalShown: true,
                                       detectedLanguage: "fr"});
    let notif = yield getInfoBar();
    ok(notif, "the infobar is visible");
    let ui = gBrowser.selectedBrowser.translationUI;
    let uri = gBrowser.selectedBrowser.currentURI;
    ok(ui.shouldShowInfoBar(uri, "fr"),
       "check shouldShowInfoBar initially returns true");

    // Open the "options" drop down.
    yield openPopup(notif._getAnonElt("options"));
    ok(notif._getAnonElt("options").getAttribute("open"),
       "the options menu is open");

    // Check that the item is not disabled.
    ok(!notif._getAnonElt("neverForSite").disabled,
       "The 'Never translate site' item isn't disabled");

    // Click the 'Never for French' item.
    notif._getAnonElt("neverForSite").click();
    notif = yield getInfoBar();
    ok(!notif, "infobar hidden");

    // Check this has been saved to the exceptions list.
    let sites = getDomainExceptions();
    is(sites.length, 1, "one site in the exception list");
    is(sites[0].origin, "http://example.com", "correct site in the exception list");
    ok(!ui.shouldShowInfoBar(uri, "fr"),
       "the infobar wouldn't be shown anymore");

    // Reopen the infobar.
    PopupNotifications.getNotification("translate").anchorElement.click();
    notif = yield getInfoBar();
    // Open the "options" drop down.
    yield openPopup(notif._getAnonElt("options"));
    ok(notif._getAnonElt("neverForSite").disabled,
       "The 'Never translate French' item is disabled");

    // Cleanup.
    Services.perms.remove(makeURI("http://example.com"), "translate");
    notif.close();
  }
},

{
  desc: "language exception list",
  run: function* checkLanguageExceptions() {
    // Put 2 languages in the pref before opening the window to check
    // the list is displayed on load.
    Services.prefs.setCharPref(kLanguagesPref, "fr,de");

    // Open the translation exceptions dialog.
    let win = openDialog("chrome://browser/content/preferences/translation.xul",
                         "Browser:TranslationExceptions",
                         "", null);
    yield waitForWindowLoad(win);

    // Check that the list of language exceptions is loaded.
    let getById = win.document.getElementById.bind(win.document);
    let tree = getById("languagesTree");
    let remove = getById("removeLanguage");
    let removeAll = getById("removeAllLanguages");
    is(tree.view.rowCount, 2, "The language exceptions list has 2 items");
    ok(remove.disabled, "The 'Remove Language' button is disabled");
    ok(!removeAll.disabled, "The 'Remove All Languages' button is enabled");

    // Select the first item.
    tree.view.selection.select(0);
    ok(!remove.disabled, "The 'Remove Language' button is enabled");

    // Click the 'Remove' button.
    remove.click();
    is(tree.view.rowCount, 1, "The language exceptions now contains 1 item");
    is(getLanguageExceptions().length, 1, "One exception in the pref");

    // Clear the pref, and check the last item is removed from the display.
    Services.prefs.setCharPref(kLanguagesPref, "");
    is(tree.view.rowCount, 0, "The language exceptions list is empty");
    ok(remove.disabled, "The 'Remove Language' button is disabled");
    ok(removeAll.disabled, "The 'Remove All Languages' button is disabled");

    // Add an item and check it appears.
    Services.prefs.setCharPref(kLanguagesPref, "fr");
    is(tree.view.rowCount, 1, "The language exceptions list has 1 item");
    ok(remove.disabled, "The 'Remove Language' button is disabled");
    ok(!removeAll.disabled, "The 'Remove All Languages' button is enabled");

    // Click the 'Remove All' button.
    removeAll.click();
    is(tree.view.rowCount, 0, "The language exceptions list is empty");
    ok(remove.disabled, "The 'Remove Language' button is disabled");
    ok(removeAll.disabled, "The 'Remove All Languages' button is disabled");
    is(Services.prefs.getCharPref(kLanguagesPref), "", "The pref is empty");

    win.close();
  }
},

{
  desc: "domains exception list",
  run: function* checkDomainExceptions() {
    // Put 2 exceptions before opening the window to check the list is
    // displayed on load.
    let perms = Services.perms;
    perms.add(makeURI("http://example.org"), "translate", perms.DENY_ACTION);
    perms.add(makeURI("http://example.com"), "translate", perms.DENY_ACTION);

    // Open the translation exceptions dialog.
    let win = openDialog("chrome://browser/content/preferences/translation.xul",
                         "Browser:TranslationExceptions",
                         "", null);
    yield waitForWindowLoad(win);

    // Check that the list of language exceptions is loaded.
    let getById = win.document.getElementById.bind(win.document);
    let tree = getById("sitesTree");
    let remove = getById("removeSite");
    let removeAll = getById("removeAllSites");
    is(tree.view.rowCount, 2, "The sites exceptions list has 2 items");
    ok(remove.disabled, "The 'Remove Site' button is disabled");
    ok(!removeAll.disabled, "The 'Remove All Sites' button is enabled");

    // Select the first item.
    tree.view.selection.select(0);
    ok(!remove.disabled, "The 'Remove Site' button is enabled");

    // Click the 'Remove' button.
    remove.click();
    is(tree.view.rowCount, 1, "The site exceptions now contains 1 item");
    is(getDomainExceptions().length, 1, "One exception in the permissions");

    // Clear the permissions, and check the last item is removed from the display.
    perms.remove(makeURI("http://example.org"), "translate");
    perms.remove(makeURI("http://example.com"), "translate");
    is(tree.view.rowCount, 0, "The site exceptions list is empty");
    ok(remove.disabled, "The 'Remove Site' button is disabled");
    ok(removeAll.disabled, "The 'Remove All Site' button is disabled");

    // Add an item and check it appears.
    perms.add(makeURI("http://example.com"), "translate", perms.DENY_ACTION);
    is(tree.view.rowCount, 1, "The site exceptions list has 1 item");
    ok(remove.disabled, "The 'Remove Site' button is disabled");
    ok(!removeAll.disabled, "The 'Remove All Sites' button is enabled");

    // Click the 'Remove All' button.
    removeAll.click();
    is(tree.view.rowCount, 0, "The site exceptions list is empty");
    ok(remove.disabled, "The 'Remove Site' button is disabled");
    ok(removeAll.disabled, "The 'Remove All Sites' button is disabled");
    is(getDomainExceptions().length, 0, "No exceptions in the permissions");

    win.close();
  }
}

];
