/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* Shorthand constructors to construct an nsI(Local)File and zip reader: */
const LocalFile = new Components.Constructor(
  "@mozilla.org/file/local;1",
  Ci.nsIFile,
  "initWithPath"
);
const ZipReader = new Components.Constructor(
  "@mozilla.org/libjar/zip-reader;1",
  "nsIZipReader",
  "open"
);

const IS_ALPHA = /^[a-z]+$/i;

var { PerfTestHelpers } = ChromeUtils.import(
  "resource://testing-common/PerfTestHelpers.jsm"
);

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
      let { subdirs, files } = await iterateOverPath(nextDir, extensions);
      dirQueue.push(...subdirs);
      rv.push(...files);
    }
    return rv;
  })();
}

/**
 * Iterate over the children of |path| and find subdirectories and files with
 * the given extension.
 *
 * This function recurses into ZIP and JAR archives as well.
 *
 * @param {string} path The path to check.
 * @param {string[]} extensions The file extensions we're interested in.
 *
 * @returns {Promise<object>}
 *           A promise that resolves to an object containing the following
 *           properties:
 *           - files: an array of nsIURIs corresponding to
 *             files that match the extensions passed
 *           - subdirs: an array of paths for subdirectories we need to recurse
 *             into (handled by generateURIsFromDirTree above)
 */
async function iterateOverPath(path, extensions) {
  const children = await IOUtils.getChildren(path);

  const files = [];
  const subdirs = [];

  for (const entry of children) {
    const stat = await IOUtils.stat(entry);

    if (stat.type === "directory") {
      subdirs.push(entry);
    } else if (extensions.some(extension => entry.endsWith(extension))) {
      if (await IOUtils.exists(entry)) {
        const spec = PathUtils.toFileURI(entry);
        files.push(Services.io.newURI(spec));
      }
    } else if (
      entry.endsWith(".ja") ||
      entry.endsWith(".jar") ||
      entry.endsWith(".zip") ||
      entry.endsWith(".xpi")
    ) {
      const file = new LocalFile(entry);
      for (const extension of extensions) {
        files.push(...generateEntriesFromJarFile(file, extension));
      }
    }
  }

  return { files, subdirs };
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

/**
 * Returns whether or not a word (presumably in en-US) is capitalized per
 * expectations.
 *
 * @param {String} word The single word to check.
 * @param {boolean} expectCapitalized True if the word should be capitalized.
 * @returns {boolean} True if the word matches the expected capitalization.
 */
function hasExpectedCapitalization(word, expectCapitalized) {
  let firstChar = word[0];
  if (!IS_ALPHA.test(firstChar)) {
    return true;
  }

  let isCapitalized = firstChar == firstChar.toLocaleUpperCase("en-US");
  return isCapitalized == expectCapitalized;
}
