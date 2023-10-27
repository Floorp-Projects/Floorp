/**
 * We are used by test_formSubmission.html to immediately activate and start
 * controlling its page.  We operate in 3 modes, conveyed via ?MODE appended to
 * our URL.
 *
 * - "no-fetch": Don't register a fetch listener so that the optimized fetch
 *   event bypass happens.
 * - "reset-fetch": Do register a fetch listener, reset every interception.
 * - "proxy-fetch": Do register a fetch listener, resolve every interception
 *   with fetch(event.request).
 */

const mode = location.search.slice(1);

// Fetch handling.
if (mode !== "no-fetch") {
  addEventListener("fetch", function (event) {
    if (mode === "reset-fetch") {
      // Don't invoke respondWith, resetting the interception.
      return;
    }
    if (mode === "proxy-fetch") {
      // Per the spec, there's an automatic waitUntil() on this too.
      event.respondWith(fetch(event.request));
    }
  });
}

// Go straight to activation, bypassing waiting.
addEventListener("install", function (event) {
  event.waitUntil(skipWaiting());
});
// Control the test document ASAP.
addEventListener("activate", function (event) {
  event.waitUntil(clients.claim());
});
