/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const NS_ERROR_STORAGE_BUSY = Cr.NS_ERROR_STORAGE_BUSY;

function getProfileDir() {
  return Services.dirsvc.get("ProfD", Ci.nsIFile);
}

// Given a "/"-delimited path relative to a base file (or the profile
// directory if a base file is not provided) return an nsIFile representing the
// path.  This does not test for the existence of the file or parent
// directories.  It is safe even on Windows where the directory separator is
// not "/", but make sure you're not passing in a "\"-delimited path.
function getRelativeFile(relativePath, baseFile) {
  if (!baseFile) {
    baseFile = getProfileDir();
  }

  let file = baseFile.clone();

  if (Services.appinfo.OS === "WINNT") {
    let winFile = file.QueryInterface(Ci.nsILocalFileWin);
    winFile.useDOSDevicePathSyntax = true;
  }

  relativePath.split("/").forEach(function(component) {
    if (component == "..") {
      file = file.parent;
    } else {
      file.append(component);
    }
  });

  return file;
}

function getCurrentPrincipal() {
  return Cc["@mozilla.org/systemprincipal;1"].createInstance(Ci.nsIPrincipal);
}

function getSimpleDatabase(principal, persistence) {
  let connection = Cc["@mozilla.org/dom/sdb-connection;1"].createInstance(
    Ci.nsISDBConnection
  );

  if (!principal) {
    principal = getCurrentPrincipal();
  }

  connection.init(principal, persistence);

  return connection;
}

function requestFinished(request) {
  return new Promise(function(resolve, reject) {
    request.callback = function(req) {
      if (req.resultCode == Cr.NS_OK) {
        resolve(req.result);
      } else {
        reject(req.resultCode);
      }
    };
  });
}
