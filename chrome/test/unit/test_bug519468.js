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

function write_locale(stream, locale, package) {
  var s = "locale " + package + " " + locale + " jar:" + locale + ".jar!";
  s += "/locale/" + locale + "/" + package +"/\n";
  stream.write(s, s.length);
}

var localeService = Cc["@mozilla.org/intl/nslocaleservice;1"]
                    .getService(Ci.nsILocaleService);

var systemLocale = localeService.getLocaleComponentForUserAgent();

var locales;

if (systemLocale == "en-US")
  locales = [ "en-US", "fr-FR", "de-DE" ];
else if (systemLocale == "fr-FR")
  locales = [ "en-US", systemLocale, "de-DE" ];
else
  locales = [ "en-US", systemLocale, "fr-FR" ];

do_get_profile();
var workingDir = Cc["@mozilla.org/file/directory_service;1"].
                 getService(Ci.nsIProperties).get("ProfD", Ci.nsIFile);
var manifest = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
manifest.initWithFile(workingDir);
manifest.append("test_bug519468.manifest");
manifest.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0600);
var stream = Cc["@mozilla.org/network/file-output-stream;1"].
             createInstance(Ci.nsIFileOutputStream);
stream.init(manifest, 0x04 | 0x08 | 0x20, 0600, 0); // write, create, truncate
locales.slice(0,2).forEach(function(l) write_locale(stream, l, "testmatchos"));
write_locale(stream, locales[2], "testnomatchos");
stream.close();

var MANIFESTS = [
  manifest
];

registerManifests(MANIFESTS);

var chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                .getService(Ci.nsIXULChromeRegistry)
                .QueryInterface(Ci.nsIToolkitChromeRegistry);
chromeReg.checkForNewChrome();

var prefService = Cc["@mozilla.org/preferences-service;1"]
                  .getService(Ci.nsIPrefService)
                  .QueryInterface(Ci.nsIPrefBranch);

function test_locale(aTest) {
  prefService.setBoolPref("intl.locale.matchOS", aTest.matchOS);
  if (aTest.selected)
    prefService.setCharPref("general.useragent.locale", aTest.selected);
  else
    try {
      prefService.clearUserPref("general.useragent.locale");
    } catch(e) {}

  var selectedLocale = chromeReg.getSelectedLocale(aTest.package);
  do_check_eq(selectedLocale, aTest.locale);
}

function run_test()
{
  var tests = [
    {matchOS: true, selected: null, locale: systemLocale},
    {matchOS: true, selected: locales[0], locale: locales[0]},
    {matchOS: true, selected: locales[1], locale: locales[1]},
    {matchOS: true, selected: locales[2], locale: locales[0]},
    {matchOS: true, selected: null, locale: locales[2], package: "testnomatchos"},
    {matchOS: false, selected: null, locale: locales[0]},
    {matchOS: false, selected: locales[0], locale: locales[0]},
    {matchOS: false, selected: locales[1], locale: locales[1]},
    {matchOS: false, selected: locales[2], locale: locales[0]},
  ];

  for (var i = 0; i < tests.length; ++ i) {
    var test = tests[i];
    if (!test.package)
      test.package = "testmatchos";
    test_locale(test);
  }
}
