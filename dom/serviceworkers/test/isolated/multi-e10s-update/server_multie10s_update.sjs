// stolen from file_blocked_script.sjs
function setGlobalState(data, key) {
  x = {
    data,
    QueryInterface(iid) {
      return this;
    },
  };
  x.wrappedJSObject = x;
  setObjectState(key, x);
}

function getGlobalState(key) {
  var data;
  getObjectState(key, function (x) {
    data = x && x.wrappedJSObject.data;
  });
  return data;
}

function completeBlockingRequest(response) {
  response.write("42");
  response.finish();
}

// This stores the response that's currently blocking, or true if the release
// got here before the blocking request.
const BLOCKING_KEY = "multie10s-update-release";
// This tracks the number of blocking requests we received up to this point in
// time.  This value will be cleared when fetched.  It's on the caller to make
// sure that all the blocking requests that might occurr have already occurred.
const COUNT_KEY = "multie10s-update-count";

/**
 * Serve a request that will only be completed when the ?release variant of this
 * .sjs is fetched.  This allows us to avoid using a timer, which slows down the
 * tests and is brittle under slow hardware.
 */
function handleBlockingRequest(request, response) {
  response.processAsync();
  response.setHeader("Content-Type", "application/javascript", false);

  const existingCount = getGlobalState(COUNT_KEY) || 0;
  setGlobalState(existingCount + 1, COUNT_KEY);

  const alreadyReleased = getGlobalState(BLOCKING_KEY);
  if (alreadyReleased === true) {
    completeBlockingRequest(response);
    setGlobalState(null, BLOCKING_KEY);
  } else if (alreadyReleased) {
    // If we've got another response stacked up, this means something is wrong
    // with the test.  The count mechanism will detect this, so just let this
    // one through so we fail fast rather than hanging.
    dump("we got multiple blocking requests stacked up!!\n");
    completeBlockingRequest(response);
  } else {
    setGlobalState(response, BLOCKING_KEY);
  }
}

function handleReleaseRequest(request, response) {
  const blockingResponse = getGlobalState(BLOCKING_KEY);
  if (blockingResponse) {
    completeBlockingRequest(blockingResponse);
    setGlobalState(null, BLOCKING_KEY);
  } else {
    setGlobalState(true, BLOCKING_KEY);
  }

  response.setHeader("Content-Type", "application/json", false);
  response.write(JSON.stringify({ released: true }));
}

function handleCountRequest(request, response) {
  const count = getGlobalState(COUNT_KEY) || 0;
  // --verify requires that we clear this so the test can be re-run.
  setGlobalState(0, COUNT_KEY);

  response.setHeader("Content-Type", "application/json", false);
  response.write(JSON.stringify({ count }));
}

function handleRequest(request, response) {
  dump(
    "server_multie10s_update.sjs: processing request for " +
      request.path +
      "?" +
      request.queryString +
      "\n"
  );
  const query = new URLSearchParams(request.queryString);
  if (query.has("release")) {
    handleReleaseRequest(request, response);
  } else if (query.has("get-and-clear-count")) {
    handleCountRequest(request, response);
  } else {
    handleBlockingRequest(request, response);
  }
}
