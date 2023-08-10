/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The code below is mostly is a slight modification of the now removed
 * intl/locale/PluralForm.jsm that removes dependencies on chrome privileged
 * APIs. To make maintenance easier, this file is kept as close as possible to
 * the original in terms of implementation. The modified methods here are
 * - makeGetter (remove code adding the caller name to the log)
 * - get ruleNum() (rely on LocalizationHelper instead of String.services)
 * - log() (rely on console.log)
 *
 * Disable eslint warnings to preserve original code style.
 */

/* eslint-disable */

/**
 * This module provides the PluralForm object which contains a method to figure
 * out which plural form of a word to use for a given number based on the
 * current localization. There is also a makeGetter method that creates a get
 * function for the desired plural rule. This is useful for extensions that
 * specify their own plural rule instead of relying on the browser default.
 * (I.e., the extension hasn't been localized to the browser's locale.)
 *
 * See: http://developer.mozilla.org/en/docs/Localization_and_Plurals
 *
 * List of methods:
 *
 * string pluralForm
 * get(int aNum, string aWords)
 *
 * int numForms
 * numForms()
 *
 * [string pluralForm get(int aNum, string aWords), int numForms numForms()]
 * makeGetter(int aRuleNum)
 * Note: Basically, makeGetter returns 2 functions that do "get" and "numForm"
 */

const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
const L10N = new LocalizationHelper("toolkit/locales/intl.properties");

// These are the available plural functions that give the appropriate index
// based on the plural rule number specified. The first element is the number
// of plural forms and the second is the function to figure out the index.
const gFunctions = [
  // 0: Chinese
  [1, (n) => 0],
  // 1: English
  [2, (n) => n!=1?1:0],
  // 2: French
  [2, (n) => n>1?1:0],
  // 3: Latvian
  [3, (n) => n%10==1&&n%100!=11?1:n%10==0?0:2],
  // 4: Scottish Gaelic
  [4, (n) => n==1||n==11?0:n==2||n==12?1:n>0&&n<20?2:3],
  // 5: Romanian
  [3, (n) => n==1?0:n==0||n%100>0&&n%100<20?1:2],
  // 6: Lithuanian
  [3, (n) => n%10==1&&n%100!=11?0:n%10>=2&&(n%100<10||n%100>=20)?2:1],
  // 7: Russian
  [3, (n) => n%10==1&&n%100!=11?0:n%10>=2&&n%10<=4&&(n%100<10||n%100>=20)?1:2],
  // 8: Slovak
  [3, (n) => n==1?0:n>=2&&n<=4?1:2],
  // 9: Polish
  [3, (n) => n==1?0:n%10>=2&&n%10<=4&&(n%100<10||n%100>=20)?1:2],
  // 10: Slovenian
  [4, (n) => n%100==1?0:n%100==2?1:n%100==3||n%100==4?2:3],
  // 11: Irish Gaeilge
  [5, (n) => n==1?0:n==2?1:n>=3&&n<=6?2:n>=7&&n<=10?3:4],
  // 12: Arabic
  [6, (n) => n==0?5:n==1?0:n==2?1:n%100>=3&&n%100<=10?2:n%100>=11&&n%100<=99?3:4],
  // 13: Maltese
  [4, (n) => n==1?0:n==0||n%100>0&&n%100<=10?1:n%100>10&&n%100<20?2:3],
  // 14: Unused
  [3, (n) => n%10==1?0:n%10==2?1:2],
  // 15: Icelandic, Macedonian
  [2, (n) => n%10==1&&n%100!=11?0:1],
  // 16: Breton
  [5, (n) => n%10==1&&n%100!=11&&n%100!=71&&n%100!=91?0:n%10==2&&n%100!=12&&n%100!=72&&n%100!=92?1:(n%10==3||n%10==4||n%10==9)&&n%100!=13&&n%100!=14&&n%100!=19&&n%100!=73&&n%100!=74&&n%100!=79&&n%100!=93&&n%100!=94&&n%100!=99?2:n%1000000==0&&n!=0?3:4],
  // 17: Shuar
  [2, (n) => n!=0?1:0],
  // 18: Welsh
  [6, (n) => n==0?0:n==1?1:n==2?2:n==3?3:n==6?4:5],
  // 19: Bosnian, Croatian, Serbian
  [3, (n) => n % 10 == 1 && n % 100 != 11 ? 0 : n % 10 >= 2 && n % 10 <= 4 && (n % 100 < 10 || n % 100 >= 20) ? 1 : 2],
];

const PluralForm = {
  /**
   * Get the correct plural form of a word based on the number
   *
   * @param aNum
   *        The number to decide which plural form to use
   * @param aWords
   *        A semi-colon (;) separated string of words to pick the plural form
   * @return The appropriate plural form of the word
   */
  get get()
  {
    // This method will lazily load to avoid perf when it is first needed and
    // creates getPluralForm function. The function it creates is based on the
    // value of pluralRule specified in the intl stringbundle.
    // See: http://developer.mozilla.org/en/docs/Localization_and_Plurals

    // Delete the getters to be overwritten
    delete this.numForms;
    delete this.get;

    // Make the plural form get function and set it as the default get
    [this.get, this.numForms] = this.makeGetter(this.ruleNum);
    return this.get;
  },

  /**
   * Create a pair of plural form functions for the given plural rule number.
   *
   * @param aRuleNum
   *        The plural rule number to create functions
   * @return A pair: [function that gets the right plural form,
   *                  function that returns the number of plural forms]
   */
  makeGetter: function(aRuleNum)
  {
    // Default to "all plural" if the value is out of bounds or invalid
    if (aRuleNum < 0 || aRuleNum >= gFunctions.length || isNaN(aRuleNum)) {
      log(["Invalid rule number: ", aRuleNum, " -- defaulting to 0"]);
      aRuleNum = 0;
    }

    // Get the desired pluralRule function
    let [numForms, pluralFunc] = gFunctions[aRuleNum];

    // Return functions that give 1) the number of forms and 2) gets the right
    // plural form
    return [function(aNum, aWords) {
      // Figure out which index to use for the semi-colon separated words
      let index = pluralFunc(aNum ? Number(aNum) : 0);
      let words = aWords ? aWords.split(/;/) : [""];

      // Explicitly check bounds to avoid strict warnings
      let ret = index < words.length ? words[index] : undefined;

      // Check for array out of bounds or empty strings
      if ((ret == undefined) || (ret == "")) {
        // Display a message in the error console
        log(["Index #", index, " of '", aWords, "' for value ", aNum,
            " is invalid -- plural rule #", aRuleNum, ";"]);

        // Default to the first entry (which might be empty, but not undefined)
        ret = words[0];
      }

      return ret;
    }, () => numForms];
  },

  /**
   * Get the number of forms for the current plural rule
   *
   * @return The number of forms
   */
  get numForms()
  {
    // We lazily load numForms, so trigger the init logic with get()
    this.get();
    return this.numForms;
  },

  /**
   * Get the plural rule number from the intl stringbundle
   *
   * @return The plural rule number
   */
  get ruleNum()
  {
    try {
      return parseInt(L10N.getStr("pluralRule"), 10);
    } catch (e) {
    // Fallback to English if the pluralRule property is not available.
      return 1;
    }
  }
};

/**
 * Private helper function to log errors to the error console and command line
 *
 * @param aMsg
 *        Error message to log or an array of strings to concat
 */
function log(aMsg)
{
  let msg = "plural-form.js: " + (aMsg.join ? aMsg.join("") : aMsg);
  console.log(msg + "\n");
}

exports.PluralForm = PluralForm;
exports.get = PluralForm.get;

/* eslint-enable */
