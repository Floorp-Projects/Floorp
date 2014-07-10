/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A worker dedicated to loop-report sanitation and writing for MozLoopService.
 */

"use strict";

importScripts("resource://gre/modules/osfile.jsm");

let File = OS.File;
let Encoder = new TextEncoder();
let Counter = 0;

const MAX_LOOP_LOGS = 5;
/**
 * Communications with the controller.
 *
 * Accepts messages:
 * { path: filepath, ping: data }
 *
 * Sends messages:
 * { ok: true }
 * { fail: serialized_form_of_OS.File.Error }
 */

onmessage = function(e) {
  if (++Counter > MAX_LOOP_LOGS) {
    postMessage({
      fail: "Maximum " + MAX_LOOP_LOGS + "loop reports reached for this session"
    });
    return;
  }

  let directory = e.data.directory;
  let filename = e.data.filename;
  let ping = e.data.ping;

  let pingStr = JSON.stringify(ping);

  // Save to disk
  let array = Encoder.encode(pingStr);
  try {
    File.makeDir(directory,
                 { unixMode: OS.Constants.S_IRWXU, ignoreExisting: true });
    File.writeAtomic(OS.Path.join(directory, filename), array);
    postMessage({ ok: true });
  } catch (ex if ex instanceof File.Error) {
    // Instances of OS.File.Error know how to serialize themselves
    postMessage({fail: File.Error.toMsg(ex)});
  }
};
