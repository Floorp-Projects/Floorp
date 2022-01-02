/**
 * Helper page used by browser_localStorage_xxx.js.
 *
 * We expose methods to be invoked by SpecialPowers.spawn() calls.
 * SpecialPowers.spawn() uses the message manager and is PContent-based.  When
 * LocalStorage was PContent-managed, ordering was inherently ensured so we
 * could assume each page had already received all relevant events.  Now some
 * explicit type of coordination is required.
 *
 * This gets complicated because:
 * - LocalStorage is an ugly API that gives us almost unlimited implementation
 *   flexibility in the face of multiple processes.  It's also an API that sites
 *   may misuse which may encourage us to leverage that flexibility in the
 *   future to improve performance at the expense of propagation latency, and
 *   possibly involving content-observable coalescing of events.
 * - The Quantum DOM effort and its event labeling and separate task queues and
 *   green threading and current LocalStorage implementation mean that using
 *   other PBackground-based APIs such as BroadcastChannel may not provide
 *   reliable ordering guarantees.  Specifically, it's hard to guarantee that
 *   a BroadcastChannel postMessage() issued after a series of LocalStorage
 *   writes won't be received by the target window before the writes are
 *   perceived.  At least not without constraining the implementations of both
 *   APIs.
 * - Some of our tests explicitly want to verify LocalStorage behavior without
 *   having a "storage" listener, so we can't add a storage listener if the test
 *   didn't already want one.
 *
 * We use 2 approaches for coordination:
 * 1. If we're already listening for events, we listen for the sentinel value to
 *    be written.  This is efficient and appropriate in this case.
 * 2. If we're not listening for events, we use setTimeout(0) to poll the
 *    localStorage key and value until it changes to our expected value.
 *    setTimeout(0) eventually clamps to setTimeout(4), so in the event we are
 *    experiencing delays, we have reasonable, non-CPU-consuming back-off in
 *    place that leaves the CPU free to time out and fail our test if something
 *    broke.  This is ugly but makes us less brittle.
 *
 * Both of these involve mutateStorage writing the sentinel value at the end of
 * the batch.  All of our result-returning methods accordingly filter out the
 * sentinel key/value pair.
 **/

var pageName = document.location.search.substring(1);
window.addEventListener("load", () => {
  document.getElementById("pageNameH").textContent = pageName;
});

// Key that conveys the end of a write batch.  Filtered out from state and
// events.
const SENTINEL_KEY = "WRITE_BATCH_SENTINEL";

var storageEventsPromise = null;
function listenForStorageEvents(sentinelValue) {
  const recordedEvents = [];
  storageEventsPromise = new Promise(function(resolve, reject) {
    window.addEventListener("storage", function thisHandler(event) {
      if (event.key === SENTINEL_KEY) {
        // There should be no way for this to have the wrong value, but reject
        // if it is wrong.
        if (event.newValue === sentinelValue) {
          window.removeEventListener("storage", thisHandler);
          resolve(recordedEvents);
        } else {
          reject(event.newValue);
        }
      } else {
        recordedEvents.push([event.key, event.newValue, event.oldValue]);
      }
    });
  });
}

function mutateStorage({ mutations, sentinelValue }) {
  mutations.forEach(function([key, value]) {
    if (key !== null) {
      if (value === null) {
        localStorage.removeItem(key);
      } else {
        localStorage.setItem(key, value);
      }
    } else {
      localStorage.clear();
    }
  });
  localStorage.setItem(SENTINEL_KEY, sentinelValue);
}

// Returns a promise that is resolve when the sentinel key has taken on the
// sentinel value.  Oddly structured to make sure promises don't let us
// accidentally side-step the timeout clamping logic.
function waitForSentinelValue(sentinelValue) {
  return new Promise(function(resolve) {
    function checkFunc() {
      if (localStorage.getItem(SENTINEL_KEY) === sentinelValue) {
        resolve();
      } else {
        // I believe linters will only yell at us if we use a non-zero constant.
        // Other forms of back-off were considered, including attempting to
        // issue a round-trip through PBackground, but that still potentially
        // runs afoul of labeling while also making us dependent on unrelated
        // APIs.
        setTimeout(checkFunc, 0);
      }
    }
    checkFunc();
  });
}

async function getStorageState(maybeSentinel) {
  if (maybeSentinel) {
    await waitForSentinelValue(maybeSentinel);
  }

  let numKeys = localStorage.length;
  let state = {};
  for (var iKey = 0; iKey < numKeys; iKey++) {
    let key = localStorage.key(iKey);
    if (key !== SENTINEL_KEY) {
      state[key] = localStorage.getItem(key);
    }
  }
  return state;
}

function returnAndClearStorageEvents() {
  return storageEventsPromise;
}
