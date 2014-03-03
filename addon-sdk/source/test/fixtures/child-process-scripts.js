/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { platform, pathFor } = require('sdk/system');
const { defer } = require('sdk/core/promise');
const { emit } = require('sdk/event/core');
const { join } = require('sdk/fs/path');
const { writeFile, unlinkSync, existsSync } = require('sdk/io/fs');
const PROFILE_DIR= pathFor('ProfD');
const isWindows = platform.toLowerCase().indexOf('win') === 0;
const isOSX = platform.toLowerCase().indexOf('darwin') === 0;

let scripts = {
  'args.sh': 'echo $1 $2 $3 $4',
  'args.bat': 'echo %1 %2 %3 %4',
  'check-env.sh': 'echo $CHILD_PROCESS_ENV_TEST',
  'check-env.bat': 'echo %CHILD_PROCESS_ENV_TEST%',
  'check-pwd.sh': 'echo $PWD',
  'check-pwd.bat': 'cd',
  'large-err.sh': 'for n in `seq 0 $1` ; do echo "E" 1>&2; done',
  'large-err-mac.sh': 'for ((i=0; i<$1; i=i+1)); do echo "E" 1>&2; done',
  'large-err.bat': 'FOR /l %%i in (0,1,%1) DO echo "E" 1>&2',
  'large-out.sh': 'for n in `seq 0 $1` ; do echo "O"; done',
  'large-out-mac.sh': 'for ((i=0; i<$1; i=i+1)); do echo "O"; done',
  'large-out.bat': 'FOR /l %%i in (0,1,%1) DO echo "O"',
  'wait.sh': 'sleep 2',
  // Use `ping` to an invalid IP address because `timeout` isn't
  // on all environments? http://stackoverflow.com/a/1672349/1785755
  'wait.bat': 'ping 1.1.1.1 -n 1 -w 2000 > nul'
};

Object.keys(scripts).forEach(filename => {
  if (/\.sh$/.test(filename))
    scripts[filename] = '#!/bin/sh\n' + scripts[filename];
  else if (/\.bat$/.test(filename))
    scripts[filename] = '@echo off\n' + scripts[filename];
});

function getScript (name) {
  // Use specific OSX script if exists
  if (isOSX && scripts[name + '-mac.sh'])
    name = name + '-mac';
  let fileName = name + (isWindows ? '.bat' : '.sh');
  return createFile(fileName, scripts[fileName]);
}
exports.getScript = getScript;

function createFile (name, data) {
  let { promise, resolve, reject } = defer();
  let fileName = join(PROFILE_DIR, name);
  writeFile(fileName, data, function (err) {
    if (err) reject();
    else {
      makeExecutable(fileName);
      resolve(fileName);
    }
  });
  return promise;
}

// TODO Use fs.chmod once implemented, bug 914606
function makeExecutable (name) {
  let { CC } = require('chrome');
  let nsILocalFile = CC('@mozilla.org/file/local;1', 'nsILocalFile', 'initWithPath');
  let file = nsILocalFile(name);
  file.permissions = parseInt('0777', 8);
}

function deleteFile (name) {
  let file = join(PROFILE_DIR, name);
  if (existsSync(file))
    unlinkSync(file);
}

function cleanUp () {
  Object.keys(scripts).forEach(deleteFile);
}
exports.cleanUp = cleanUp;
