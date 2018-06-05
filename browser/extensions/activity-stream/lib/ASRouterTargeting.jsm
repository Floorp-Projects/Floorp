ChromeUtils.import("resource://gre/modules/components-utils/FilterExpressions.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "ProfileAge",
  "resource://gre/modules/ProfileAge.jsm");
ChromeUtils.import("resource://gre/modules/Console.jsm");

const FXA_USERNAME_PREF = "services.sync.username";

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
  }
};

this.ASRouterTargeting = {
  Environment: TargetingGetters,

  isMatch(filterExpression, context = this.Environment) {
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
  async findMatchingMessage(messages, context) {
    const arrayOfItems = [...messages];
    let match;
    let candidate;
    while (!match && arrayOfItems.length) {
      candidate = removeRandomItemFromArray(arrayOfItems);
      if (candidate && (!candidate.targeting || await this.isMatch(candidate.targeting, context))) {
        match = candidate;
      }
    }
    return match;
  }
};

this.EXPORTED_SYMBOLS = ["ASRouterTargeting", "removeRandomItemFromArray"];
