/* ***** BEGIN LICENSE BLOCK *****
 *   Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Plural Form l10n Code.
 *
 * The Initial Developer of the Original Code is
 * Edward Lee <edward.lee@engineering.uiuc.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var EXPORTED_SYMBOLS = [ "PluralForm" ];

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

const Cc = Components.classes;
const Ci = Components.interfaces;

const kIntlProperties = "chrome://global/locale/intl.properties";

// These are the available plural functions that give the appropriate index
// based on the plural rule number specified. The first element is the number
// of plural forms and the second is the function to figure out the index.
let gFunctions = [
  // 0: Chinese
  [1, function(n) 0],
  // 1: English
  [2, function(n) n!=1?1:0],
  // 2: French
  [2, function(n) n>1?1:0],
  // 3: Latvian
  [3, function(n) n%10==1&&n%100!=11?1:n!=0?2:0],
  // 4: Scottish Gaelic
  [3, function(n) n==1?0:n==2?1:2],
  // 5: Romanian
  [3, function(n) n==1?0:n==0||n%100>0&&n%100<20?1:2],
  // 6: Lithuanian
  [3, function(n) n%10==1&&n%100!=11?0:n%10>=2&&(n%100<10||n%100>=20)?2:1],
  // 7: Russian
  [3, function(n) n%10==1&&n%100!=11?0:n%10>=2&&n%10<=4&&(n%100<10||n%100>=20)?1:2],
  // 8: Slovak
  [3, function(n) n==1?0:n>=2&&n<=4?1:2],
  // 9: Polish
  [3, function(n) n==1?0:n%10>=2&&n%10<=4&&(n%100<10||n%100>=20)?1:2],
  // 10: Slovenian
  [4, function(n) n%100==1?0:n%100==2?1:n%100==3||n%100==4?2:3],
  // 11: Irish Gaeilge
  [5, function(n) n==1?0:n==2?1:n>=3&&n<=6?2:n>=7&&n<=10?3:4],
  // 12: Arabic
  [6, function(n) n==0?5:n==1?0:n==2?1:n%100>=3&&n%100<=10?2:n%100>=11&&n%100<=99?3:4],
  // 13: Maltese
  [4, function(n) n==1?0:n==0||n%100>0&&n%100<=10?1:n%100>10&&n%100<20?2:3],
  // 14: Macedonian
  [3, function(n) n%10==1?0:n%10==2?1:2],
  // 15: Icelandic
  [2, function(n) n%10==1&&n%100!=11?0:1],
];

let PluralForm = {
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
    delete PluralForm.numForms;
    delete PluralForm.get;

    // Get the plural rule number from the intl stringbundle
    let ruleNum = Number(Cc["@mozilla.org/intl/stringbundle;1"].
      getService(Ci.nsIStringBundleService).createBundle(kIntlProperties).
      GetStringFromName("pluralRule"));

    // Make the plural form get function and set it as the default get
    [PluralForm.get, PluralForm.numForms] = PluralForm.makeGetter(ruleNum);
    return PluralForm.get;
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
        // Report the caller to help figure out who is causing badness
        let caller = PluralForm.get.caller ? PluralForm.get.caller.name : "top";

        // Display a message in the error console
        log(["Index #", index, " of '", aWords, "' for value ", aNum,
            " is invalid -- plural rule #", aRuleNum, "; called by ", caller]);

        // Default to the first entry (which might be empty, but not undefined)
        ret = words[0];
      }

      return ret;
    }, function() numForms];
  },

  /**
   * Get the number of forms for the current plural rule
   *
   * @return The number of forms
   */
  get numForms()
  {
    // We lazily load numForms, so trigger the init logic with get()
    PluralForm.get();
    return PluralForm.numForms;
  },
};

/**
 * Private helper function to log errors to the error console and command line
 *
 * @param aMsg
 *        Error message to log or an array of strings to concat
 */
function log(aMsg)
{
  let msg = "PluralForm.jsm: " + (aMsg.join ? aMsg.join("") : aMsg);
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).
    logStringMessage(msg);
  dump(msg + "\n");
}
