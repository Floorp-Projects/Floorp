/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci } = require('chrome');
const system = require('../sdk/system');
const runtime = require('../sdk/system/runtime');
const oscpu = Cc["@mozilla.org/network/protocol;1?name=http"]
                 .getService(Ci.nsIHttpProtocolHandler).oscpu;
const hostname = Cc["@mozilla.org/network/dns-service;1"]
                 .getService(Ci.nsIDNSService).myHostName;
const isWindows = system.platform === 'win32';
const endianness = ((new Uint32Array((new Uint8Array([1,2,3,4])).buffer))[0] === 0x04030201) ? 'LE' : 'BE';

/**
 * Returns a path to a temp directory
 */
exports.tmpdir = () => system.pathFor('TmpD');

/**
 * Returns the endianness of the architecture: either 'LE' or 'BE'
 */
exports.endianness = () => endianness;

/**
 * Returns hostname of the machine
 */
exports.hostname = () => hostname;

/**
 * Name of the OS type
 * Possible values:
 * https://developer.mozilla.org/en/OS_TARGET
 */
exports.type = () => runtime.OS;

/**
 * Name of the OS Platform in lower case string.
 * Possible values:
 * https://developer.mozilla.org/en/OS_TARGET
 */
exports.platform = () => system.platform;

/**
 * Type of processor architecture running:
 * 'arm', 'ia32', 'x86', 'x64'
 */
exports.arch = () => system.architecture;

/**
 * Returns the operating system release.
 */
exports.release = () => {
  let match = oscpu.match(/(\d[\.\d]*)/);
  return match && match.length > 1 ? match[1] : oscpu;
};

/**
 * Returns EOL character for the OS
 */
exports.EOL = isWindows ? '\r\n' : '\n';

/**
 * Returns [0, 0, 0], as this is not implemented.
 */
exports.loadavg = () => [0, 0, 0];

['uptime', 'totalmem', 'freemem', 'cpus'].forEach(method => {
  exports[method] = () => { throw new Error('os.' + method + ' is not supported.'); };
});
