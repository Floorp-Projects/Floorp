var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

function handleRequest(aRequest, aResponse) {
  aResponse.setHeader("Access-Control-Allow-Origin", "*", false);

  var query = getQuery(aRequest);

  if ("setVersion" in query) {
    setState("version", query.setVersion);
    aResponse.write("OK");
    return;
  }
  var version = Number(getState("version"));

  // setPrecompile sets the "precompile" field in the app manifest
  if ("setPrecompile" in query) {
    setState("precompile", query.setPrecompile);
    aResponse.write("OK");
    return;
  }
  var precompile = getState("precompile");

  if ("getPackage" in query) {
    aResponse.setHeader("Content-Type", "application/zip", false);
    aResponse.write(buildAppPackage(version, precompile));
    return;
  }

  if ("getManifest" in query) {
    aResponse.setHeader("Content-Type", "application/x-web-app-manifest+json", false);
    aResponse.write(getManifest(version, precompile));
    return;
  }
}

function getQuery(aRequest) {
  var query = {};
  aRequest.queryString.split('&').forEach(function(val) {
    var [name, value] = val.split('=');
    query[decodeURIComponent(name)] = decodeURIComponent(value);
  });
  return query;
}

function getTestFile(aName) {
  var file = Services.dirsvc.get("CurWorkD", Ci.nsIFile);

  var path = "chrome/dom/apps/tests/asmjs/" + aName;

  path.split("/").forEach(function(component) {
    file.append(component);
  });

  return file;
}

function readFile(aFile) {
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(aFile, -1, 0, 0);
  var data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}

function getManifest(aVersion, aPrecompile) {
  return readFile(getTestFile("manifest.webapp")).
           replace(/VERSIONTOKEN/g, aVersion).
           replace(/PRECOMPILETOKEN/g, aPrecompile);
}

function buildAppPackage(aVersion, aPrecompile) {
  const PR_RDWR        = 0x04;
  const PR_CREATE_FILE = 0x08;
  const PR_TRUNCATE    = 0x20;

  let zipFile = Services.dirsvc.get("TmpD", Ci.nsIFile);
  zipFile.append("application.zip");

  let zipWriter = Cc["@mozilla.org/zipwriter;1"].createInstance(Ci.nsIZipWriter);
  zipWriter.open(zipFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  // Add index.html file to the zip file
  zipWriter.addEntryFile("index.html",
                         Ci.nsIZipWriter.COMPRESSION_NONE,
                         getTestFile("index.html"),
                         false);

  // Add manifest to the zip file
  var manStream = Cc["@mozilla.org/io/string-input-stream;1"].
                  createInstance(Ci.nsIStringInputStream);
  var manifest = getManifest(aVersion, aPrecompile);
  manStream.setData(manifest, manifest.length);
  zipWriter.addEntryStream("manifest.webapp", Date.now(),
                           Ci.nsIZipWriter.COMPRESSION_NONE,
                           manStream, false);

  // Add an asm.js module to the zip file
  // The module also contains some code to perform tests

  // Creates a module big enough to be cached
  // The module is different according to the app version, this way
  // we can test that on update the new module is compiled
  var code = "function f() { 'use asm';\n";
  for (var i = 0; i < 5000; i++) {
    code += "function g" + i + "() { return " + i + "}\n";
  }
  code += "return g" + aVersion + " }\n\
var jsFuns = SpecialPowers.Cu.getJSTestingFunctions();\n\
ok(jsFuns.isAsmJSCompilationAvailable(), 'asm.js compilation is available');\n\
ok(jsFuns.isAsmJSModule(f), 'f is an asm.js module');\n\
var g" + aVersion + " = f();\n\
ok(jsFuns.isAsmJSFunction(g" + aVersion + "), 'g" + aVersion + " is an asm.js function');\n\
ok(g" + aVersion + "() === " + aVersion + ", 'g" + aVersion + " returns the correct result');";

  // If the "precompile" field contains this module, we test that it is loaded
  // from the cache.
  // If the "precompile" field doesn't contain the module or is incorrect, we
  // test that the module is not loaded from the cache.
  if (aPrecompile == '["asmjsmodule.js"]') {
    code += "ok(jsFuns.isAsmJSModuleLoadedFromCache(f), 'module loaded from cache');\n";
  } else {
    code += "ok(!jsFuns.isAsmJSModuleLoadedFromCache(f), 'module not loaded from cache');\n";
  }

  code += "alert('DONE');";

  var jsStream = Cc["@mozilla.org/io/string-input-stream;1"].
                 createInstance(Ci.nsIStringInputStream);
  jsStream.setData(code, code.length);
  zipWriter.addEntryStream("asmjsmodule.js", Date.now(),
                           Ci.nsIZipWriter.COMPRESSION_NONE,
                           jsStream, false);

  zipWriter.close();

  return readFile(zipFile);
}
