// sjs for remote about:newtab (bug 1226928)

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.importGlobalProperties(["URLSearchParams"]);

const path = "browser/dom/security/test/contentverifier/";

const goodFileName = "file_about_newtab.html";
const goodFileBase = path + goodFileName;
const goodFile = FileUtils.getDir("TmpD", [], true);
goodFile.append(goodFileName);
const goodSignature = path + "file_about_newtab_good_signature";
const goodKeyId = "RemoteNewTab";

const scriptFileName = "script.js";
const cssFileName = "style.css";
const badFile = path + "file_about_newtab_bad.html";
const brokenSignature = path + "file_about_newtab_broken_signature";
const badSignature = path + "file_about_newtab_bad_signature";
const badKeyId = "OldRemoteNewTabKey";

const sriFile = path + "file_about_newtab_sri.html";
const sriSignature = path + "file_about_newtab_sri_signature";

const badCspFile = path + "file_about_newtab_bad_csp.html";
const badCspSignature = path + "file_about_newtab_bad_csp_signature";

const tempFileNames = [goodFileName, scriptFileName, cssFileName];

// we copy the file to serve as newtab to a temp directory because
// we modify it during tests.
setupTestFiles();

function setupTestFiles() {
  for (let fileName of tempFileNames) {
    let tempFile = FileUtils.getDir("TmpD", [], true);
    tempFile.append(fileName);
    if (!tempFile.exists()) {
      let fileIn = getFileName(path + fileName, "CurWorkD");
      fileIn.copyTo(FileUtils.getDir("TmpD", [], true), "");
    }
  }
}

function getFileName(filePath, dir) {
  // Since it's relative to the cwd of the test runner, we start there and
  // append to get to the actual path of the file.
  let testFile =
    Cc["@mozilla.org/file/directory_service;1"].
      getService(Components.interfaces.nsIProperties).
      get(dir, Components.interfaces.nsILocalFile);
  let dirs = filePath.split("/");
  for (let i = 0; i < dirs.length; i++) {
    testFile.append(dirs[i]);
  }
  return testFile;
}

function loadFile(file) {
  // Load a file to return it.
  let testFileStream =
    Cc["@mozilla.org/network/file-input-stream;1"]
      .createInstance(Components.interfaces.nsIFileInputStream);
  testFileStream.init(file, -1, 0, 0);
  return NetUtil.readInputStreamToString(testFileStream,
                                         testFileStream.available());
}

function appendToFile(aFile, content) {
  try {
    let file = FileUtils.openFileOutputStream(aFile, FileUtils.MODE_APPEND |
                                                     FileUtils.MODE_WRONLY);
    file.write(content, content.length);
    file.close();
  } catch (e) {
    dump(">>> Error in appendToFile "+e);
    return "Error";
  }
  return "Done";
}

function truncateFile(aFile, length) {
  let fileIn = loadFile(aFile);
  fileIn = fileIn.slice(0, -length);

  try {
    let file = FileUtils.openFileOutputStream(aFile, FileUtils.MODE_WRONLY |
                                                     FileUtils.MODE_TRUNCATE);
    file.write(fileIn, fileIn.length);
    file.close();
  } catch (e) {
    dump(">>> Error in truncateFile "+e);
    return "Error";
  }
  return "Done";
}

function cleanupTestFiles() {
  for (let fileName of tempFileNames) {
    let tempFile = FileUtils.getDir("TmpD", [], true);
    tempFile.append(fileName);
    tempFile.remove(true);
  }
}

/*
 * handle requests of the following form:
 * sig=good&key=good&file=good&header=good&cached=no to serve pages with
 * content signatures
 *
 * it further handles invalidateFile=yep and validateFile=yep to change the
 * served file
 */
function handleRequest(request, response) {
  let params = new URLSearchParams(request.queryString);
  let keyType = params.get("key");
  let signatureType = params.get("sig");
  let fileType = params.get("file");
  let headerType = params.get("header");
  let cached = params.get("cached");
  let invalidateFile = params.get("invalidateFile");
  let validateFile = params.get("validateFile");
  let resource = params.get("resource");

  if (params.get("cleanup")) {
    cleanupTestFiles();
    response.setHeader("Content-Type", "text/html", false);
    response.write("Done");
    return;
  }

  if (resource) {
    if (resource == "script") {
      response.setHeader("Content-Type", "application/javascript", false);
      response.write(loadFile(getFileName(scriptFileName, "TmpD")));
    } else { // resource == "css1" || resource == "css2"
      response.setHeader("Content-Type", "text/css", false);
      response.write(loadFile(getFileName(cssFileName, "TmpD")));
    }
    return;
  }

  // if invalidateFile is set, this doesn't actually return a newtab page
  // but changes the served file to invalidate the signature
  // NOTE: make sure to make the file valid again afterwards!
  if (invalidateFile) {
    let r = "Done";
    for (let fileName of tempFileNames) {
      if (appendToFile(getFileName(fileName, "TmpD"), "!") != "Done") {
        r = "Error";
      }
    }
    response.setHeader("Content-Type", "text/html", false);
    response.write(r);
    return;
  }

  // if validateFile is set, this doesn't actually return a newtab page
  // but changes the served file to make the signature valid again
  if (validateFile) {
    let r = "Done";
    for (let fileName of tempFileNames) {
      if (truncateFile(getFileName(fileName, "TmpD"), 1) != "Done") {
        r = "Error";
      }
    }
    response.setHeader("Content-Type", "text/html", false);
    response.write(r);
    return;
  }

  // avoid confusing cache behaviours
  if (!cached) {
    response.setHeader("Cache-Control", "no-cache", false);
  } else {
    response.setHeader("Cache-Control", "max-age=3600", false);
  }

  // send HTML to test allowed/blocked behaviours
  response.setHeader("Content-Type", "text/html", false);

  // set signature header and key for Content-Signature header
  /* By default a good content-signature header is returned. Any broken return
   * value has to be indicated in the url.
   */
  let csHeader = "";
  let keyId = goodKeyId;
  let signature = goodSignature;
  let file = goodFile;
  if (keyType == "bad") {
    keyId = badKeyId;
  }
  if (signatureType == "bad") {
    signature = badSignature;
  } else if (signatureType == "broken") {
    signature = brokenSignature;
  } else if (signatureType == "sri") {
    signature = sriSignature;
  } else if (signatureType == "bad-csp") {
    signature = badCspSignature;
  }
  if (fileType == "bad") {
    file = getFileName(badFile, "CurWorkD");
  } else if (fileType == "sri") {
    file = getFileName(sriFile, "CurWorkD");
  } else if (fileType == "bad-csp") {
    file = getFileName(badCspFile, "CurWorkD");
  }

  if (headerType == "good") {
    // a valid content-signature header
    csHeader = "keyid=" + keyId + ";p384ecdsa=" +
               loadFile(getFileName(signature, "CurWorkD"));
  } else if (headerType == "error") {
    // this content-signature header is missing ; before p384ecdsa
    csHeader = "keyid=" + keyId + "p384ecdsa=" +
               loadFile(getFileName(signature, "CurWorkD"));
  } else if (headerType == "errorInKeyid") {
    // this content-signature header is missing the keyid directive
    csHeader = "keid=" + keyId + ";p384ecdsa=" +
               loadFile(getFileName(signature, "CurWorkD"));
  } else if (headerType == "errorInSignature") {
    // this content-signature header is missing the p384ecdsa directive
    csHeader = "keyid=" + keyId + ";p385ecdsa=" +
               loadFile(getFileName(signature, "CurWorkD"));
  }

  if (csHeader) {
    response.setHeader("Content-Signature", csHeader, false);
  }
  let result = loadFile(file);

  response.write(result);
}
