/* vim:set ts=2 sw=2 sts=2 expandtab */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci, CC } = require('chrome');
const options = require('@loader/options');
const file = require('./io/file');
const runtime = require("./system/runtime");

const appStartup = Cc['@mozilla.org/toolkit/app-startup;1'].
                   getService(Ci.nsIAppStartup);
const appInfo = Cc["@mozilla.org/xre/app-info;1"].
                getService(Ci.nsIXULAppInfo);
const directoryService = Cc['@mozilla.org/file/directory_service;1'].
                         getService(Ci.nsIProperties);

const PR_WRONLY = parseInt("0x02");
const PR_CREATE_FILE = parseInt("0x08");
const PR_APPEND = parseInt("0x10");
const PR_TRUNCATE = parseInt("0x20");

function openFile(path, mode) {
  let file = Cc["@mozilla.org/file/local;1"].
             createInstance(Ci.nsILocalFile);
  file.initWithPath(path);
  let stream = Cc["@mozilla.org/network/file-output-stream;1"].
               createInstance(Ci.nsIFileOutputStream);
  stream.init(file, mode, -1, 0);
  return stream
}

const { eAttemptQuit: E_ATTEMPT, eForceQuit: E_FORCE } = appStartup;

/**
 * Parsed JSON object that was passed via `cfx --static-args "{ foo: 'bar' }"`
 */
exports.staticArgs = options.staticArgs;

/**
 * Environment variables. Environment variables are non-enumerable properties
 * of this object (key is name and value is value).
 */
exports.env = require('./system/environment').env;

/**
 * Ends the process with the specified `code`. If omitted, exit uses the
 * 'success' code 0. To exit with failure use `1`.
 * TODO: Improve platform to actually quit with an exit code.
 */
exports.exit = function exit(code) {
  // This is used by 'cfx' to find out exit code.
  if ('resultFile' in options && options.resultFile) {
    let mode = PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE;
    let stream = openFile(options.resultFile, mode);
    let status = code ? 'FAIL' : 'OK';
    stream.write(status, status.length);
    stream.flush();
    stream.close();
  }

  appStartup.quit(code ? E_ATTEMPT : E_FORCE);
};

// Adapter for nodejs's stdout & stderr:
// http://nodejs.org/api/process.html#process_process_stdout
let stdout = Object.freeze({ write: dump, end: dump });
exports.stdout = stdout;
exports.stderr = stdout;

/**
 * Returns a path of the system's or application's special directory / file
 * associated with a given `id`. For list of possible `id`s please see:
 * https://developer.mozilla.org/en-US/docs/Code_snippets/File_I_O#Getting_files_in_special_directories
 * http://mxr.mozilla.org/mozilla-central/source/xpcom/io/nsAppDirectoryServiceDefs.h
 * @example
 *
 *    // get firefox profile path
 *    let profilePath = require('system').pathFor('ProfD');
 *    // get OS temp files directory (/tmp)
 *    let temps = require('system').pathFor('TmpD');
 *    // get OS desktop path for an active user (~/Desktop on linux
 *    // or C:\Documents and Settings\username\Desktop on windows).
 *    let desktopPath = require('system').pathFor('Desk');
 */
exports.pathFor = function pathFor(id) {
  return directoryService.get(id, Ci.nsIFile).path;
};

/**
 * What platform you're running on (all lower case string).
 * For possible values see:
 * https://developer.mozilla.org/en/OS_TARGET
 */
exports.platform = runtime.OS.toLowerCase();

/**
 * What processor architecture you're running on:
 * `'arm', 'ia32', or 'x64'`.
 */
exports.architecture = runtime.XPCOMABI.split('_')[0];

/**
 * What compiler used for build:
 * `'msvc', 'n32', 'gcc2', 'gcc3', 'sunc', 'ibmc'...`
 */
exports.compiler = runtime.XPCOMABI.split('_')[1];

/**
 * The application's build ID/date, for example "2004051604".
 */
exports.build = appInfo.appBuildID;

/**
 * The XUL application's UUID.
 * This has traditionally been in the form
 * `{AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE}` but for some applications it may
 * be: "appname@vendor.tld".
 */
exports.id = appInfo.ID;

/**
 * The name of the application.
 */
exports.name = appInfo.name;

/**
 * The XUL application's version, for example "0.8.0+" or "3.7a1pre".
 */
exports.version = appInfo.version;

/**
 * XULRunner version.
 */
exports.platformVersion = appInfo.platformVersion;


/**
 * The name of the application vendor, for example "Mozilla".
 */
exports.vendor = appInfo.vendor;
