/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *      Vivien Nicolas <21@vingtetun.org>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
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
 * ***** END LICENSE BLOCK *****
 */

var MANIFESTS = [
  do_get_file("data/test_bug519468.manifest")
];

registerManifests(MANIFESTS);

var chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                .getService(Ci.nsIXULChromeRegistry)
                .QueryInterface(Ci.nsIToolkitChromeRegistry);

var localeService = Cc["@mozilla.org/intl/nslocaleservice;1"]
                    .getService(Ci.nsILocaleService);

var prefService = Cc["@mozilla.org/preferences-service;1"]
                  .getService(Ci.nsIPrefService)
                  .QueryInterface(Ci.nsIPrefBranch);

function test_locale(aTest) {
  prefService.setBoolPref("intl.locale.matchOS", aTest.matchOS);
  prefService.setCharPref("general.useragent.locale", aTest.selected || "en-US");

  var selectedLocale = chromeReg.getSelectedLocale("testmatchos");
  do_check_eq(selectedLocale, aTest.locale);
}

function run_test()
{
  var systemLocale = localeService.getLocaleComponentForUserAgent();

  var tests = [
    {matchOS: false, selected: null, locale: "en-US"},
    {matchOS: true, selected: null, locale: systemLocale},
    {matchOS: true, selected: "fr-FR", locale: systemLocale},
    {matchOS: false, selected: "fr-FR", locale: "fr-FR"},
    {matchOS: true, selected: null, locale: systemLocale}
  ];

  for (var i = 0; i < tests.length; ++ i) {
    var test = tests[i];
    test_locale(test);
  }
}
