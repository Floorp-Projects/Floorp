var Ci = Components.interfaces;
var Cc = Components.classes;

var nativeJSON = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
var workingDir = dirSvc.get("TmpD", Ci.nsIFile);

var outputName = "json-test-output";
var outputDir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
outputDir.initWithFile(workingDir);
outputDir.append(outputName);

if (!outputDir.exists()) {
  outputDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0777);
} else if (!outputDir.isDirectory()) {
  do_throw(outputName + " is not a directory?")
}
var crockfordJSON = null;
do_import_script("dom/src/json/test/json2.js");
