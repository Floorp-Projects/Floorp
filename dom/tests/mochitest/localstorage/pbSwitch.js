var _PBSvc = null;

function get_PBSvc()
{
  if (_PBSvc)
    return _PBSvc;

  try {
    _PBSvc = SpecialPowers.Components.classes["@mozilla.org/privatebrowsing;1"].
             getService(SpecialPowers.Ci.nsIPrivateBrowsingService);
    return _PBSvc;
  }
  catch (ex) {
  }

  return null;
}

function enterPrivateBrowsing()
{
  if (get_PBSvc()) {
    var prefBranch = SpecialPowers.Components.classes["@mozilla.org/preferences-service;1"].
                     getService(SpecialPowers.Ci.nsIPrefBranch);
    prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);

    get_PBSvc().privateBrowsingEnabled = true;
  }
}

function leavePrivateBrowsing()
{
  if (get_PBSvc()) {
    get_PBSvc().privateBrowsingEnabled = false;

    var prefBranch = SpecialPowers.Components.classes["@mozilla.org/preferences-service;1"].
                     getService(SpecialPowers.Ci.nsIPrefBranch);
    if (prefBranch.prefHasUserValue("browser.privatebrowsing.keep_current_session"))
      prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
  }
}
