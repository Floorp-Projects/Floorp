/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const runtimeInfo = {
  // device name which is running the runtime,
  // unavailable on this-firefox runtime
  deviceName: PropTypes.string,

  // icon which represents the kind of runtime
  icon: PropTypes.string.isRequired,

  // name of runtime such as "Firefox Nightly"
  name: PropTypes.string.isRequired,

  // version of runtime
  version: PropTypes.string.isRequired,
};

const runtimeTransportDetails = {
  // host name of tcp connection to debugger server
  host: PropTypes.string.isRequired,

  // port number of tcp connection to debugger server
  port: PropTypes.number.isRequired,
};

const runtimeConnection = {
  // debugger client instance
  client: PropTypes.object.isRequired,

  // runtime information
  info: PropTypes.shape(runtimeInfo).isRequired,

  // tcp connection information,
  // unavailable on this-firefox runtime
  transportDetails: PropTypes.shape(runtimeTransportDetails),
};

const networkRuntimeConnectionParameter = {
  // host name of debugger server to connect
  host: PropTypes.string.isRequired,

  // port number of debugger server to connect
  port: PropTypes.number.isRequired,
};

const usbRuntimeConnectionParameter = {
  // socket path to connect debugger server
  socketPath: PropTypes.string.isRequired,
};

const runtimeExtra = {
  // parameter to connect to debugger server
  connectionParameters: PropTypes.oneOfType([
    PropTypes.shape(networkRuntimeConnectionParameter),
    PropTypes.shape(usbRuntimeConnectionParameter),
  ]).isRequired,

  // device name, only available on USB devices
  deviceName: PropTypes.string,
};

const runtime = {
  // unique id for the runtime
  id: PropTypes.string.isRequired,

  // available after the connection to the runtime is established
  connection: PropTypes.shape(runtimeConnection),

  // object containing non standard properties that depend on the runtime type,
  // unavailable on this-firefox runtime
  extra: PropTypes.shape(runtimeExtra),

  // display name of the runtime
  name: PropTypes.string.isRequired,

  // runtime type, for instance "network", "usb" ...
  type: PropTypes.string.isRequired,
};

/**
 * Export type of runtime
 */
exports.runtime = PropTypes.shape(runtime);
