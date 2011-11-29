/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let prefService = Services.prefs;
  let prefs = [
    "network",
    "networkinfo",
    "csserror",
    "cssparser",
    "exception",
    "jswarn",
    "error",
    "warn",
    "info",
    "log"
  ];

  //Set all prefs to true
  prefs.forEach(function(pref) {
    prefService.setBoolPref("devtools.webconsole.filter." + pref, true);
  });

  addTab("about:blank");
  openConsole();
  
  let hud = HUDService.getHudByWindow(content);
  let hudId = HUDService.getHudIdByWindow(content);

  //Check if the filters menuitems exists and is checked
  prefs.forEach(function(pref) {
    let checked = hud.HUDBox.querySelector("menuitem[prefKey=" + pref + "]").
      getAttribute("checked");
    is(checked, "true", "menuitem for " + pref + " exists and is checked");
  });
  
  //Set all prefs to false
  prefs.forEach(function(pref) {
    HUDService.setFilterState(hudId, pref, false);
  });

  //Re-init the console
  closeConsole();
  openConsole();

  hud = HUDService.getHudByWindow(content);
  
  //Check if filters menuitems exists and are unchecked
  prefs.forEach(function(pref) {
    let checked = hud.HUDBox.querySelector("menuitem[prefKey=" + pref + "]").
      getAttribute("checked");
    is(checked, "false", "menuitem for " + pref + " exists and is not checked");
    prefService.clearUserPref("devtools.webconsole.filter." + pref);
  });
  
  gBrowser.removeCurrentTab();
  
  finish();
}
