const {actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});
const {MessageCenterRouter} = ChromeUtils.import("resource://activity-stream/lib/MessageCenterRouter.jsm", {});
ChromeUtils.import("resource://gre/modules/Services.jsm");

const ONBOARDING_FINISHED_PREF = "browser.onboarding.notification.finished";

/**
 * @class MessageCenterFeed - Connects MessageCenter singleton (see above) to Activity Stream's
 * store so that it can use the RemotePageManager.
 */
class MessageCenterFeed {
  constructor(options = {}) {
    this.router = options.router || MessageCenterRouter;
  }

  enable() {
    this.router.init(this.store._messageChannel.channel);
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
   * (messageCenterExperimentEnabled) and enable or disable MessageCenter based on
   * its value.
   */
  enableOrDisableBasedOnPref() {
    const isExperimentEnabled = this.store.getState().Prefs.values.messageCenterExperimentEnabled;
    if (!this.router.initialized && isExperimentEnabled) {
      this.enable();
    } else if (!isExperimentEnabled && this.router.initialized) {
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
this.MessageCenterFeed = MessageCenterFeed;

const EXPORTED_SYMBOLS = ["MessageCenterFeed"];
