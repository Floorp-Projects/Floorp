const {actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});
const {ASRouter} = ChromeUtils.import("resource://activity-stream/lib/ASRouter.jsm", {});
ChromeUtils.import("resource://gre/modules/Services.jsm");

const ONBOARDING_FINISHED_PREF = "browser.onboarding.notification.finished";

/**
 * @class ASRouterFeed - Connects ASRouter singleton (see above) to Activity Stream's
 * store so that it can use the RemotePageManager.
 */
class ASRouterFeed {
  constructor(options = {}) {
    this.router = options.router || ASRouter;
  }

  async enable() {
    await this.router.init(this.store._messageChannel.channel,
      this.store.dbStorage.getDbTable("snippets"));
    // Disable onboarding
    Services.prefs.setBoolPref(ONBOARDING_FINISHED_PREF, true);
  }

  disable() {
    if (this.router.initialized) {
      this.router.uninit();
      // Re-enable onboarding
      Services.prefs.setBoolPref(ONBOARDING_FINISHED_PREF, false);
    }
  }

  /**
   * enableOrDisableBasedOnPref - Check the experiment pref
   * (asrouterExperimentEnabled) and enable or disable ASRouter based on
   * its value.
   */
  enableOrDisableBasedOnPref() {
    const prefs = this.store.getState().Prefs.values;
    const isExperimentEnabled = prefs.asrouterExperimentEnabled;
    const isOnboardingExperimentEnabled = prefs.asrouterOnboardingCohort;
    if (!this.router.initialized && (isExperimentEnabled || isOnboardingExperimentEnabled > 0)) {
      this.enable();
    } else if ((!isExperimentEnabled || isOnboardingExperimentEnabled === 0) && this.router.initialized) {
      this.disable();
    }
  }

  onAction(action) {
    switch (action.type) {
      case at.INIT:
      case at.PREF_CHANGED:
        this.enableOrDisableBasedOnPref();
        break;
      case at.UNINIT:
        this.disable();
        break;
    }
  }
}
this.ASRouterFeed = ASRouterFeed;

const EXPORTED_SYMBOLS = ["ASRouterFeed"];
