/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this
 * file, you can obtain one at http://mozilla.org/mpl/2.0/. */

"use strict";

const { JSTracer } = ChromeUtils.importESModule(
  "resource://devtools/server/tracer/tracer.sys.mjs",
  { global: "shared" }
);

let lineToTrace;

const fileContents = new Map();

function getFileContent(url) {
  let content = fileContents.get(url);
  if (content) {
    return content;
  }
  content = readURI(url).split("\n");
  fileContents.set(url, content);
  return content;
}

function isNestedFrame(frame, topFrame) {
  if (frame.older) {
    // older will be a Debugger.Frame
    while ((frame = frame.older)) {
      if (frame == topFrame) {
        return true;
      }
    }
  } else if (frame.olderSavedFrame) {
    // olderSavedFrame will be a SavedStack object
    frame = frame.olderSavedFrame;
    const { lineNumber, columnNumber } = topFrame.script.getOffsetMetadata(
      top.offset
    );
    while ((frame = frame.parent || frame.asyncParent)) {
      if (
        frame.source == topFrame.script.source.url &&
        frame.line == lineNumber &&
        frame.column == columnNumber
      ) {
        return true;
      }
    }
  }
  return false;
}

// Store the top most frame running at `lineToTrace` line.
// We will then log all frames which are children of this top one.
let initialFrame = null;
let previousSourceUrl = null;

function traceFrame({ frame }) {
  const { script } = frame;
  const { lineNumber, columnNumber } = script.getOffsetMetadata(frame.offset);
  if (lineToTrace) {
    if (lineNumber == lineToTrace) {
      // Stop the first tracer started from `exports.start()` which was only waiting for the particular test script line to run
      JSTracer.stopTracing();

      const { url } = script.source;
      const filename = url.substr(url.lastIndexOf("/") + 1);
      const line = getFileContent(url)[lineNumber - 1];
      logStep(`Start tracing ${filename} @ ${lineNumber} :: ${line}`);
      previousSourceUrl = url;
      // Restart a new tracer which would go track all the globals and not restrict to the test script.
      const tracerOptions = {
        // Ensure tracing all globals in this thread
        traceAllGlobals: true,
        // Ensure tracing each execution within functions (and not only function calls)
        traceSteps: true,
      };
      lineToTrace = null;
      JSTracer.startTracing(tracerOptions);
    }
    return false;
  }
  // We executed the test line we wanted to trace and now log all frames via a second tracer instance

  // First pick up the very first executed frame, so that we can trace all nested frame from this one.
  if (!initialFrame) {
    initialFrame = frame;
  } else if (initialFrame.terminated) {
    // If the traced top frame completed its execution, stop tracing.
    // Note that terminated will only be true once any possibly asynchronous work of the traced function
    // is done executing.
    logStep("End of execution");
    exports.stop();
    return false;
  } else if (!initialFrame.onStack) {
    // If we are done executing the traced Frame, it will be declared out of the stack.
    // By we should keep tracing as, if the traced Frame involves async work, it may be later restored onto the stack.
    return false;
  } else if (frame != initialFrame && !isNestedFrame(frame, initialFrame)) {
    // Then, only log frame which ultimately related to this first frame we picked.
    // Because of asynchronous calls and concurrent event loops, we may have in-between frames
    // that we ignore which relates to another event loop and another top frame.
    //
    // Note that the tracer may notify us about the exact same Frame object multiple times.
    // (its offset/location will change, but the object will be the same)
    return false;
  }

  const { url } = script.source;

  // Print the full source URL each time we start tracing a new source
  if (previousSourceUrl && previousSourceUrl !== url) {
    logStep("");
    logStep(url);
    // Log a grey line separator
    logStep(`\x1b[2m` + `\u2500`.repeat(url.length) + `\x1b[0m`);
    previousSourceUrl = url;
  }

  const line = getFileContent(url)[lineNumber - 1];
  // Grey out the beginning of the line, before frame's column,
  // and display an arrow before displaying the rest of the line.
  const code =
    "\x1b[2m" +
    line.substr(0, columnNumber - 1) +
    "\x1b[0m" +
    "\u21A6 " +
    line.substr(columnNumber - 1);

  const position = (lineNumber + ":" + columnNumber).padEnd(7);
  logStep(`${position} \u007C ${code}`);

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

exports.start = function (testGlobal, testUrl, line) {
  lineToTrace = line;
  const tracerOptions = {
    global: testGlobal,
    // Ensure tracing each execution within functions (and not only function calls)
    traceSteps: true,
    // Only trace the running test and nothing else
    filterFrameSourceUrl: testUrl,
  };
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
