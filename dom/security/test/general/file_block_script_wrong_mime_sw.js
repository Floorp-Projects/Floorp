/**
 * Service Worker that runs in 2 modes: 1) direct pass-through via
 * fetch(event.request) and 2) indirect pass-through via
 * fetch(event.request.url).
 *
 * Because this is updating a pre-existing mochitest that didn't use a SW and
 * used a single test document, we use a SW idiom where the SW claims the
 * existing window client.  And because we operate in two modes and we
 * parameterize via URL, we also ensure that we skipWaiting.
 **/

/* eslint-env serviceworker */

// We are parameterized by "mode".
const params = new URLSearchParams(location.search);
const fetchMode = params.get("fetchMode");

// When activating on initial install, claim the existing window client.
// For synchronziation, also message the controlled document to report our mode.
self.addEventListener("activate", event => {
  event.waitUntil(
    (async () => {
      await clients.claim();
      const allClients = await clients.matchAll();
      for (const client of allClients) {
        client.postMessage({
          fetchMode,
        });
      }
    })()
  );
});

// When updating the SW to change our mode of operation, skipWaiting so we
// advance directly to activating without waiting for the test window client
// to stop being controlled by our previous configuration.
self.addEventListener("install", () => {
  self.skipWaiting();
});

self.addEventListener("fetch", event => {
  switch (fetchMode) {
    case "direct":
      event.respondWith(fetch(event.request));
      break;

    case "indirect":
      event.respondWith(fetch(event.request.url));
      break;
  }
});
