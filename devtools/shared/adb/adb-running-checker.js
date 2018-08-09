/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Uses host:version service to detect if ADB is running
 * Modified from adb-file-transfer from original ADB
 */

"use strict";

const client = require("./adb-client");

exports.check = async function check() {
  let socket;
  let state;

  console.debug("Asking for host:version");

  return new Promise(resolve => {
    let runFSM = function runFSM(aData) {
      console.debug("runFSM " + state);
      switch (state) {
        case "start":
          let req = client.createRequest("host:version");
          socket.send(req);
          state = "wait-version";
          break;
        case "wait-version":
          // TODO: Actually check the version number to make sure the daemon
          //       supports the commands we want to use
          let { length, data } = client.unpackPacket(aData);
          console.debug("length: ", length, "data: ", data);
          socket.close();
          let version = parseInt(data, "16");
          if (version >= 31) {
            resolve(true);
          } else {
            console.log("killing existing adb as we need version >= 31");
            resolve(false);
          }
          break;
        default:
          console.debug("Unexpected State: " + state);
          resolve(false);
      }
    };

    let setupSocket = function() {
      socket.s.onerror = function(aEvent) {
        console.debug("running checker onerror");
        resolve(false);
      };

      socket.s.onopen = function(aEvent) {
        console.debug("running checker onopen");
        state = "start";
        runFSM();
      };

      socket.s.onclose = function(aEvent) {
        console.debug("running checker onclose");
      };

      socket.s.ondata = function(aEvent) {
        console.debug("running checker ondata");
        runFSM(aEvent.data);
      };
    };

    socket = client.connect();
    setupSocket();
  });
};
