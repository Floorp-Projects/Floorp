/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.importGlobalProperties(['File']);

const Ci = Components.interfaces;

add_task(async function() {
  // throw if anything goes wrong

  // find the current directory path
  var file = Components.classes["@mozilla.org/file/directory_service;1"]
             .getService(Ci.nsIProperties)
             .get("CurWorkD", Ci.nsIFile);
  file.append("xpcshell.ini");

  // should be able to construct a file
  var f1 = await File.createFromFileName(file.path);
  // and with nsIFiles
  var f2 = await File.createFromNsIFile(file);

  // do some tests
  Assert.ok(f1 instanceof File, "Should be a DOM File");
  Assert.ok(f2 instanceof File, "Should be a DOM File");

  Assert.ok(f1.name == "xpcshell.ini", "Should be the right file");
  Assert.ok(f2.name == "xpcshell.ini", "Should be the right file");

  Assert.ok(f1.type == "", "Should be the right type");
  Assert.ok(f2.type == "", "Should be the right type");

  var threw = false;
  try {
    // Needs a ctor argument
    var f7 = File();
  } catch (e) {
    threw = true;
  }
  Assert.ok(threw, "No ctor arguments should throw");

  var threw = false;
  try {
    // Needs a valid ctor argument
    var f7 = File(Date(132131532));
  } catch (e) {
    threw = true;
  }
  Assert.ok(threw, "Passing a random object should fail");

  var threw = false
  try {
    // Directories fail
    var dir = Components.classes["@mozilla.org/file/directory_service;1"]
                        .getService(Ci.nsIProperties)
                        .get("CurWorkD", Ci.nsIFile);
    var f7 = await File.createFromNsIFile(dir)
  } catch (e) {
    threw = true;
  }
  Assert.ok(threw, "Can't create a File object for a directory");
});
