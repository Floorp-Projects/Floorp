var offlineByDefault = {
  defaultValue: false,
  prefBranch: SpecialPowers.Cc["@mozilla.org/preferences-service;1"].getService(SpecialPowers.Ci.nsIPrefBranch),
  set: function(allow) {
    try {
      this.defaultValue = this.prefBranch.getBoolPref("offline-apps.allow_by_default");
    } catch (e) {
      this.defaultValue = false
    }
    this.prefBranch.setBoolPref("offline-apps.allow_by_default", allow);
  },
  reset: function() {
    this.prefBranch.setBoolPref("offline-apps.allow_by_default", this.defaultValue);
  }
}

offlineByDefault.set(false);
