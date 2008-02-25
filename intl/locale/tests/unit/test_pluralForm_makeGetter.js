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
 * The Original Code is Plural Form l10n Test Code.
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

/**
 * This unit test makes sure the plural form for Irish Gaeilge is working by
 * using the makeGetter method instead of using the default language (by
 * development), English.
 */

Components.utils.import("resource://gre/modules/PluralForm.jsm");

function run_test()
{
  // Irish is plural rule #11
  let [get, numForms] = PluralForm.makeGetter(11);

  // Irish has 5 plural forms
  do_check_eq(5, numForms());

  // I don't really know Irish, so I'll stick in some dummy text
  let words = "is 1;is 2;is 3-6;is 7-10;everything else";

  let test = function(text, low, high) {
    for (let num = low; num <= high; num++)
      do_check_eq(text, get(num, words));
  };

  // Make sure for good inputs, things work as expected
  test("everything else", 0, 0);
  test("is 1", 1, 1);
  test("is 2", 2, 2);
  test("is 3-6", 3, 6);
  test("is 7-10", 7, 10);
  test("everything else", 11, 200);
}
