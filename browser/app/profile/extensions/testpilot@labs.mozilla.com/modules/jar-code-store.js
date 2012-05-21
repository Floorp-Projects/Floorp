/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


function JarStore() {
  try {
  let baseDirName = "TestPilotExperimentFiles"; // this should go in pref?
  this._baseDir = null;
  this._localOverrides = {}; //override with code for debugging purposes
  this._index = {}; // tells us which jar file to look in for each module
  this._lastModified = {}; // tells us when each jar file was last modified
  this._init( baseDirName );
  } catch (e) {
    console.warn("Error instantiating jar store: " + e);
  }
}
JarStore.prototype = {

  _init: function( baseDirectory ) {
    let prefs = require("preferences-service");
    this._localOverrides = JSON.parse(
      prefs.get("extensions.testpilot.codeOverride", "{}"));

    let dir = Cc["@mozilla.org/file/directory_service;1"].
      getService(Ci.nsIProperties).get("ProfD", Ci.nsIFile);
    dir.append(baseDirectory);
    this._baseDir = dir;
    if( !this._baseDir.exists() || !this._baseDir.isDirectory() ) {
      // if jar storage directory doesn't exist, create it:
      console.info("Creating: " + this._baseDir.path + "\n");
      this._baseDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0777);
    } else {
      // Process any jar files already on disk from previous runs:
      // Build lookup index of module->jar file and modified dates
      let jarFiles = this._baseDir.directoryEntries;
      while(jarFiles.hasMoreElements()) {
        let jarFile = jarFiles.getNext().QueryInterface(Ci.nsIFile);
        // Make sure this is actually a jar file:
        if (jarFile.leafName.indexOf(".jar") != jarFile.leafName.length - 4) {
          continue;
        }
        this._lastModified[jarFile.leafName] = jarFile.lastModifiedTime;
        this._indexJar(jarFile);
      }
    }
  },

  _indexJar: function(jarFile) {
    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                .createInstance(Ci.nsIZipReader);
    zipReader.open(jarFile); // must already be nsIFile
    let entries = zipReader.findEntries(null);
    while(entries.hasMore()) {
      // Find all .js files inside jar file:
      let entry = entries.getNext();
      if (entry.indexOf(".js") == entry.length - 3) {
        // add entry to index
       let moduleName = entry.slice(0, entry.length - 3);
       this._index[moduleName] = jarFile.path;
      }
    }
    zipReader.close();
  },

  _verifyJar: function(jarFile, expectedHash) {
    // Compare the jar file's hash to the expected hash from the
    // index file.
    // from https://developer.mozilla.org/en/nsICryptoHash#Computing_the_Hash_of_a_File
    console.info("Attempting to verify jarfile vs hash = " + expectedHash);
    let istream = Cc["@mozilla.org/network/file-input-stream;1"]
                        .createInstance(Ci.nsIFileInputStream);
    // open for reading
    istream.init(jarFile, 0x01, 0444, 0);
    let ch = Cc["@mozilla.org/security/hash;1"]
                   .createInstance(Ci.nsICryptoHash);
    // Use SHA256, it's more secure than MD5:
    ch.init(ch.SHA256);
    // this tells updateFromStream to read the entire file
    const PR_UINT32_MAX = 0xffffffff;
    ch.updateFromStream(istream, PR_UINT32_MAX);
    // pass false here to get binary data back
    let hash = ch.finish(false);

    // return the two-digit hexadecimal code for a byte
    function toHexString(charCode)
    {
      return ("0" + charCode.toString(16)).slice(-2);
    }

    // convert the binary hash data to a hex string.
    let s = [toHexString(hash.charCodeAt(i)) for (i in hash)].join("");
    // s now contains your hash in hex

    return (s == expectedHash);
  },

  saveJarFile: function( filename, rawData, expectedHash ) {
    console.info("Saving a JAR file as " + filename + " hash = " + expectedHash);
    // rawData is a string of binary data representing a jar file
    let jarFile;
    try {
      jarFile = this._baseDir.clone();
      // filename may have directories in it; use just the last part
      jarFile.append(filename.split("/").pop());

      // If a file of that name already exists, remove it!
      if (jarFile.exists()) {
        jarFile.remove(false);
      }
      // From https://developer.mozilla.org/en/Code_snippets/File_I%2f%2fO#Getting_special_files
      jarFile.create( Ci.nsIFile.NORMAL_FILE_TYPE, 0600);
      let stream = Cc["@mozilla.org/network/safe-file-output-stream;1"].
                      createInstance(Ci.nsIFileOutputStream);
      stream.init(jarFile, 0x04 | 0x08 | 0x20, 0600, 0); // readwrite, create, truncate
      stream.write(rawData, rawData.length);
      if (stream instanceof Ci.nsISafeOutputStream) {
        stream.finish();
      } else {
        stream.close();
      }
      // Verify hash; if it's good, index and set last modified time.
      // If not good, remove it.
      if (this._verifyJar(jarFile, expectedHash)) {
        this._indexJar(jarFile);
        this._lastModified[jarFile.leafName] = jarFile.lastModifiedTime;
      } else {
        console.warn("Bad JAR file, doesn't match hash: " + expectedHash);
        jarFile.remove(false);
      }
    } catch(e) {
      console.warn("Error in saving jar file: " + e);
      // Remove any partially saved file
      if (jarFile.exists()) {
        jarFile.remove(false);
      }
    }
  },

  resolveModule: function(root, path) {
    // Root will be null if require() was done by absolute path.
    if (root != null) {
      // TODO I don't think we need to do anything special here.
    }
    // drop ".js" off end of path to get module
    let module;
    if (path.indexOf(".js") == path.length - 3) {
      module = path.slice(0, path.length - 3);
    } else {
      module = path;
    }
    if (this._index[module]) {
      let resolvedPath = this._index[module] + "!" + module + ".js";
      return resolvedPath;
    }
    return null;
    // must return a path... which gets passed to getFile.
  },

  getFile: function(path) {
    // used externally by cuddlefish; takes the path and returns
    // {contents: data}.
    if (this._localOverrides[path]) {
      let code = this._localOverrides[path];
      return {contents: code};
    }
    let parts = path.split("!");
    let filePath = parts[0];
    let entryName = parts[1];
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    file.initWithPath(filePath);
    return this._readEntryFromJarFile(file, entryName);
  },

  _readEntryFromJarFile: function(jarFile, entryName) {
    // Reads out content of entry, without unzipping jar file to disk.
    // Open the jar file
    let zipReader = Cc["@mozilla.org/libjar/zip-reader;1"]
                .createInstance(Ci.nsIZipReader);
    zipReader.open(jarFile); // must already be nsIFile
    let rawStream = zipReader.getInputStream(entryName);
    let stream = Cc["@mozilla.org/scriptableinputstream;1"].
      createInstance(Ci.nsIScriptableInputStream);
    stream.init(rawStream);
    try {
      let data = new String();
      let chunk = {};
      do {
        chunk = stream.read(-1);
        data += chunk;
      } while (chunk.length > 0);
      return {contents: data};
    } catch(e) {
      console.warn("Error reading entry from jar file: " + e );
    }
    return null;
  },


  getFileModifiedDate: function(filename) {
    // used by remote experiment loader to know whether we have to redownload
    // a thing or not.
    filename = filename.split("/").pop();
    if (this._lastModified[filename]) {
      return (this._lastModified[filename]);
    } else {
      return 0;
    }
  },

  listAllFiles: function() {
    // used by remote experiment loader
    let x;
    let list = [x for (x in this._index)];
    return list;
  },

  setLocalOverride: function(path, code) {
    let prefs = require("preferences-service");
    this._localOverrides[path] = code;
    prefs.set("extensions.testpilot.codeOverride",
              JSON.stringify(this._localOverrides));
  }
};

exports.JarStore = JarStore;