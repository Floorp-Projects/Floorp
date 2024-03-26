/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this
 * file, you can obtain one at http://mozilla.org/mpl/2.0/. */

"use strict";

const { JSTracer } = ChromeUtils.importESModule(
  "resource://devtools/server/tracer/tracer.sys.mjs",
  { global: "contextual" }
);

let testFileContent;

function traceFrame({ frame }) {
  const { script } = frame;
  const { lineNumber, columnNumber } = script.getOffsetMetadata(frame.offset);

  const { url } = script.source;
  const filename = url.substr(url.lastIndexOf("/") + 1);
  const line = testFileContent[lineNumber - 1];
  // Grey out the beginning of the line, before frame's column,
  // and display an arrow before displaying the rest of the line.
  const code =
    "\x1b[2m" +
    line.substr(0, columnNumber - 1) +
    "\x1b[0m" +
    "\u21A6 " +
    line.substr(columnNumber - 1);

  const position = (lineNumber + ":" + columnNumber).padEnd(7);
  logStep(`${filename} @ ${position} :: ${code}`);

  // Disable builtin tracer logging
  return false;
}

function logStep(message) {
  dump(` \x1b[2m[STEP]\x1b[0m ${message}\n`);
}

const tracingListener = {
  onTracingFrame: traceFrame,
  onTracingFrameStep: traceFrame,
};

exports.start = function (testGlobal, testUrl, pause) {
  const tracerOptions = {
    global: testGlobal,
    // Ensure tracing each execution within functions (and not only function calls)
    traceSteps: true,
    // Only trace the running test and nothing else
    filterFrameSourceUrl: testUrl,
  };
  testFileContent = readURI(testUrl).split("\n");
  // Only pause on each step if the passed value is a number,
  // otherwise we are only going to print each executed line in the test script.
  if (!isNaN(pause)) {
    // Delay each step by an amount of milliseconds
    tracerOptions.pauseOnStep = Number(pause);
    logStep(`Tracing all test script steps with ${pause}ms pause`);
    logStep(
      `/!\\ Be conscious about each pause releasing the event loop and breaking run-to-completion.`
    );
  } else {
    logStep(`Tracing all test script steps`);
  }
  logStep(
    `'\u21A6 ' symbol highlights what precise instruction is being called`
  );
  JSTracer.startTracing(tracerOptions);
  JSTracer.addTracingListener(tracingListener);
};

exports.stop = function () {
  JSTracer.stopTracing();
  JSTracer.removeTracingListener(tracingListener);
};

function readURI(uri) {
  const { NetUtil } = ChromeUtils.importESModule(
    "resource://gre/modules/NetUtil.sys.mjs",
    { global: "contextual" }
  );
  const stream = NetUtil.newChannel({
    uri: NetUtil.newURI(uri, "UTF-8"),
    loadUsingSystemPrincipal: true,
  }).open();
  const count = stream.available();
  const data = NetUtil.readInputStreamToString(stream, count, {
    charset: "UTF-8",
  });

  stream.close();
  return data;
}
