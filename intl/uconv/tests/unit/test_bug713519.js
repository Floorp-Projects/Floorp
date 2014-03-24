/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  let svc =  Components.classes["@mozilla.org/charset-converter-manager;1"]
                       .getService(Components.interfaces.nsICharsetConverterManager);

  // Ensure normal calls to getCharsetAlias do work.
  do_check_eq(svc.getCharsetAlias("Windows-1255"), "windows-1255");

  try {
    svc.getCharsetAlias("no such thing");
    do_throw("Calling getCharsetAlias with invalid value should throw.");
  }
  catch (ex) {
    // The exception is expected.
  }
}
