const workerURL =
  "http://mochi.test:8888/tests/dom/workers/test/importScripts_3rdParty_worker.js";

/**
 * An Error can be a JS Error or a DOMException.  The primary difference is that
 * JS Errors have a SpiderMonkey specific `fileName` for the filename and
 * DOMEXCEPTION uses `filename`.
 */
function normalizeError(err) {
  if (!err) {
    return null;
  }

  const isDOMException = "filename" in err;

  return {
    message: err.message,
    name: err.name,
    isDOMException,
    code: err.code,
    // normalize to fileName
    fileName: isDOMException ? err.filename : err.fileName,
    hasFileName: !!err.fileName,
    hasFilename: !!err.filename,
    lineNumber: err.lineNumber,
    columnNumber: err.columnNumber,
    stack: err.stack,
    stringified: err.toString(),
  };
}

function normalizeErrorEvent(event) {
  if (!event) {
    return null;
  }

  return {
    message: event.message,
    filename: event.filename,
    lineno: event.lineno,
    colno: event.colno,
    error: normalizeError(event.error),
    stringified: event.toString(),
  };
}

/**
 * Normalize the `OnErrorEventHandlerNonNull onerror` invocation.  The
 * special handling in JSEventHandler::HandleEvent ends up spreading out the
 * contents of the ErrorEvent into discrete arguments.  The one thing lost is
 * we can't toString the ScriptEvent itself, but this should be the same as the
 * message anyways.
 *
 * The spec for the invocation is the "special error event handling" logic
 * described in step 4 at:
 * https://html.spec.whatwg.org/multipage/webappapis.html#the-event-handler-processing-algorithm
 * noting that the step somewhat glosses over that it's only "onerror" that is
 * OnErrorEventHandlerNonNull and capable of processing 5 arguments and that's
 * why an addEventListener "error" listener doesn't get this handling.
 *
 * Argument names here are made to match the call-site in JSEventHandler.
 */
function normalizeOnError(
  msgOrEvent,
  fileName,
  lineNumber,
  columnNumber,
  error
) {
  return {
    message: msgOrEvent,
    filename: fileName,
    lineno: lineNumber,
    colno: columnNumber,
    error: normalizeError(error),
    stringified: null,
  };
}

/**
 * Helper to postMessage the provided data after a setTimeout(0) so that any
 * error event currently being dispatched that will bubble to our parent will
 * be delivered before our postMessage.
 */
function delayedPostMessage(data) {
  setTimeout(() => {
    postMessage(data);
  }, 0);
}

onmessage = function (a) {
  const args = a.data;
  // Messages are either nested (forward to a nested worker) or should be
  // processed locally.
  if (a.data.nested) {
    const worker = new Worker(workerURL);
    let firstErrorEvent;

    // When the test mode is "catch"

    worker.onmessage = function (event) {
      delayedPostMessage({
        nestedMessage: event.data,
        errorEvent: firstErrorEvent,
      });
    };

    worker.onerror = function (event) {
      firstErrorEvent = normalizeErrorEvent(event);
      event.preventDefault();
    };

    a.data.nested = false;
    worker.postMessage(a.data);
    return;
  }

  // Local test.
  if (a.data.mode === "catch") {
    try {
      importScripts(a.data.url);
      workerMethod();
    } catch (ex) {
      delayedPostMessage({
        args,
        error: normalizeError(ex),
      });
    }
  } else if (a.data.mode === "uncaught") {
    const onerrorPromise = new Promise(resolve => {
      self.onerror = (...onerrorArgs) => {
        resolve(normalizeOnError(...onerrorArgs));
      };
    });
    const listenerPromise = new Promise(resolve => {
      self.addEventListener("error", evt => {
        resolve(normalizeErrorEvent(evt));
      });
    });

    Promise.all([onerrorPromise, listenerPromise]).then(
      ([onerrorEvent, listenerEvent]) => {
        delayedPostMessage({
          args,
          onerrorEvent,
          listenerEvent,
        });
      }
    );

    importScripts(a.data.url);
    workerMethod();
    // we will have thrown by this point, which will trigger an "error" event
    // on our global and then will propagate to our parent (which could be a
    // window or a worker, if nested).
    //
    // To avoid hangs, throw a different error here that will fail equivalence
    // tests.
    throw new Error("We expected an error and this is a failsafe for hangs.");
  }
};
