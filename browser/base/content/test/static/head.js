/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* Shorthand constructors to construct an nsI(Local)File and zip reader: */
const LocalFile = new Components.Constructor("@mozilla.org/file/local;1", Ci.nsIFile, "initWithPath");
const ZipReader = new Components.Constructor("@mozilla.org/libjar/zip-reader;1", "nsIZipReader", "open");


ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");


/**
 * Returns a promise that is resolved with a list of files that have one of the
 * extensions passed, represented by their nsIURI objects, which exist inside
 * the directory passed.
 *
 * @param dir the directory which to scan for files (nsIFile)
 * @param extensions the extensions of files we're interested in (Array).
 */
function generateURIsFromDirTree(dir, extensions) {
  if (!Array.isArray(extensions)) {
    extensions = [extensions];
  }
  let dirQueue = [dir.path];
  return (async function() {
    let rv = [];
    while (dirQueue.length) {
      let nextDir = dirQueue.shift();
      let {subdirs, files} = await iterateOverPath(nextDir, extensions);
      dirQueue.push(...subdirs);
      rv.push(...files);
    }
    return rv;
  })();
}

/**
 * Uses OS.File.DirectoryIterator to asynchronously iterate over a directory.
 * It returns a promise that is resolved with an object with two properties:
 *  - files: an array of nsIURIs corresponding to files that match the extensions passed
 *  - subdirs: an array of paths for subdirectories we need to recurse into
 *             (handled by generateURIsFromDirTree above)
 *
 * @param path the path to check (string)
 * @param extensions the file extensions we're interested in.
 */
function iterateOverPath(path, extensions) {
  let iterator = new OS.File.DirectoryIterator(path);
  let parentDir = new LocalFile(path);
  let subdirs = [];
  let files = [];

  let pathEntryIterator = (entry) => {
    if (entry.isDir) {
      subdirs.push(entry.path);
    } else if (extensions.some((extension) => entry.name.endsWith(extension))) {
      let file = parentDir.clone();
      file.append(entry.name);
      // the build system might leave dead symlinks hanging around, which are
      // returned as part of the directory iterator, but don't actually exist:
      if (file.exists()) {
        let uriSpec = getURLForFile(file);
        files.push(Services.io.newURI(uriSpec));
      }
    } else if (entry.name.endsWith(".ja") || entry.name.endsWith(".jar") ||
               entry.name.endsWith(".zip") || entry.name.endsWith(".xpi")) {
      let file = parentDir.clone();
      file.append(entry.name);
      for (let extension of extensions) {
        let jarEntryIterator = generateEntriesFromJarFile(file, extension);
        files.push(...jarEntryIterator);
      }
    }
  };

  return new Promise((resolve, reject) => {
    (async function() {
      try {
        // Iterate through the directory
        await iterator.forEach(pathEntryIterator);
        resolve({files, subdirs});
      } catch (ex) {
        reject(ex);
      } finally {
        iterator.close();
      }
    })();
  });
}

/* Helper function to generate a URI spec (NB: not an nsIURI yet!)
 * given an nsIFile object */
function getURLForFile(file) {
  let fileHandler = Services.io.getProtocolHandler("file");
  fileHandler = fileHandler.QueryInterface(Ci.nsIFileProtocolHandler);
  return fileHandler.getURLSpecFromActualFile(file);
}

/**
 * A generator that generates nsIURIs for particular files found in jar files
 * like omni.ja.
 *
 * @param jarFile an nsIFile object for the jar file that needs checking.
 * @param extension the extension we're interested in.
 */
function* generateEntriesFromJarFile(jarFile, extension) {
  let zr = new ZipReader(jarFile);
  const kURIStart = getURLForFile(jarFile);

  for (let entry of zr.findEntries("*" + extension + "$")) {
    // Ignore the JS cache which is stored in omni.ja
    if (entry.startsWith("jsloader") || entry.startsWith("jssubloader")) {
      continue;
    }
    let entryURISpec = "jar:" + kURIStart + "!/" + entry;
    yield Services.io.newURI(entryURISpec);
  }
  zr.close();
}

function fetchFile(uri) {
  return new Promise((resolve, reject) => {
    let xhr = new XMLHttpRequest();
    xhr.responseType = "text";
    xhr.open("GET", uri, true);
    xhr.onreadystatechange = function() {
      if (this.readyState != this.DONE) {
        return;
      }
      try {
        resolve(this.responseText);
      } catch (ex) {
        ok(false, `Script error reading ${uri}: ${ex}`);
        resolve("");
      }
    };
    xhr.onerror = error => {
      ok(false, `XHR error reading ${uri}: ${error}`);
      resolve("");
    };
    xhr.send(null);
  });
}

async function throttledMapPromises(iterable, task, limit = 64) {
  let promises = new Set();
  for (let data of iterable) {
    while (promises.size >= limit) {
      await Promise.race(promises);
    }

    let promise = task(data);
    if (promise) {
      promise.finally(() => promises.delete(promise));
      promises.add(promise);
    }
  }

  await Promise.all(promises);
}
