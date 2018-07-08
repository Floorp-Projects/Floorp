ChromeUtils.import("resource://gre/modules/components-utils/FilterExpressions.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "ProfileAge",
  "resource://gre/modules/ProfileAge.jsm");
ChromeUtils.import("resource://gre/modules/Console.jsm");

const FXA_USERNAME_PREF = "services.sync.username";
const ONBOARDING_EXPERIMENT_PREF = "browser.newtabpage.activity-stream.asrouterOnboardingCohort";

/**
 * removeRandomItemFromArray - Removes a random item from the array and returns it.
 *
 * @param {Array} arr An array of items
 * @returns one of the items in the array
 */
function removeRandomItemFromArray(arr) {
  return arr.splice(Math.floor(Math.random() * arr.length), 1)[0];
}

const TargetingGetters = {
  get profileAgeCreated() {
    return new ProfileAge(null, null).created;
  },
  get profileAgeReset() {
    return new ProfileAge(null, null).reset;
  },
  get hasFxAccount() {
    return Services.prefs.prefHasUserValue(FXA_USERNAME_PREF);
  },
  // Temporary targeting function for the purposes of running the simplified onboarding experience
  get isInExperimentCohort() {
    return Services.prefs.getIntPref(ONBOARDING_EXPERIMENT_PREF, 0);
  }
};

this.ASRouterTargeting = {
  Environment: TargetingGetters,

  isMatch(filterExpression, target, context = this.Environment) {
    return FilterExpressions.eval(filterExpression, context);
  },

  /**
   * findMatchingMessage - Given an array of messages, returns one message
   *                       whos targeting expression evaluates to true
   *
   * @param {Array} messages An array of AS router messages
   * @param {obj|null} context A FilterExpression context. Defaults to TargetingGetters above.
   * @returns {obj} an AS router message
   */
  async findMatchingMessage(messages, target, context) {
    const arrayOfItems = [...messages];
    let match;
    let candidate;
    while (!match && arrayOfItems.length) {
      candidate = removeRandomItemFromArray(arrayOfItems);
      if (candidate && !candidate.trigger && (!candidate.targeting || await this.isMatch(candidate.targeting, target, context))) {
        match = candidate;
      }
    }
    return match;
  },

  async findMatchingMessageWithTrigger(messages, target, trigger, context) {
    const arrayOfItems = [...messages];
    let match;
    let candidate;
    while (!match && arrayOfItems.length) {
      candidate = removeRandomItemFromArray(arrayOfItems);
      if (candidate && candidate.trigger === trigger && (!candidate.targeting || await this.isMatch(candidate.targeting, target, context))) {
        match = candidate;
      }
    }
    return match;
  }
};

this.EXPORTED_SYMBOLS = ["ASRouterTargeting", "removeRandomItemFromArray"];
