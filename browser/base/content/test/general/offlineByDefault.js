var offlineByDefault = {
  defaultValue: false,
  set(allow) {
    this.defaultValue = SpecialPowers.Services.prefs.getBoolPref("offline-apps.allow_by_default", false);
    SpecialPowers.Services.prefs.setBoolPref("offline-apps.allow_by_default", allow);
  },
  reset() {
    SpecialPowers.Services.prefs.setBoolPref("offline-apps.allow_by_default", this.defaultValue);
  }
};

offlineByDefault.set(false);
