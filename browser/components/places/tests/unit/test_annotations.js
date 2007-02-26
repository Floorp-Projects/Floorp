/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Dietrich Ayala <dietrich@mozilla.com>
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

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

// Get annotation service
try {
  var annosvc= Cc["@mozilla.org/browser/annotation-service;1"].getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get annotation service\n");
} 

// main
function run_test() {
  // test URI
  var testURI = uri("http://mozilla.com/");
  var testAnnoName = "moz-test-places/annotations";
  var testAnnoVal = "test";

  // create new annotation
  try {
    annosvc.setAnnotationString(testURI, testAnnoName, testAnnoVal, 0, 0);
  } catch(ex) {
    do_throw("unable to add annotation");
  }

  // get annotation
  var storedAnnoVal = annosvc.getAnnotationString(testURI, testAnnoName);
  do_check_eq(testAnnoVal, storedAnnoVal);

  // get annotation that doesn't exist
  try {
    annosvc.getAnnotationString(testURI, "blah");
    do_throw("fetching annotation that doesn't exist, should've thrown");
  } catch(ex) {}

  // get annotation info
  var flags = {}, exp = {}, mimeType = {}, storageType = {};
  annosvc.getAnnotationInfo(testURI, testAnnoName, flags, exp, mimeType, storageType);
  do_check_eq(flags.value, 0);
  do_check_eq(exp.value, 0);
  do_check_eq(mimeType.value, null);
  do_check_eq(storageType.value, Ci.mozIStorageValueArray.VALUE_TYPE_TEXT);

  // get annotation names for a uri
  var annoNames = annosvc.getPageAnnotationNames(testURI, {});
  do_check_eq(annoNames.length, 1);
  do_check_eq(annoNames[0], "moz-test-places/annotations");

  /* copy annotations to another uri
  var newURI = uri("http://mozilla.org");
  var oldAnnoNames = annosvc.getPageAnnotationNames(testURI, {});
  annosvc.copyAnnotations(testURI, newURI, false);
  var newAnnoNames = annosvc.getPageAnnotationNames(newURI, {});
  do_check_eq(oldAnnoNames.length, newAnnoNames.length);
  */
}
