/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This file is derived from :bkelly's https://glitch.com/edit/#!/html-sw-stream

addEventListener("install", evt => {
  evt.waitUntil(self.skipWaiting());
});

// Create a BroadcastChannel to notify when we have closed our streams.
const channel = new BroadcastChannel("stream-closed");

const MAX_TICK_COUNT = 3000;
const TICK_INTERVAL = 4;
/**
 * Generate a continuous stream of data at a sufficiently high frequency that a
 * there"s a good chance of racing channel cancellation.
 */
function handleStream(evt, filename) {
  // Create some payload to send.
  const encoder = new TextEncoder();
  let strChunk =
    "Static routes are the future of ServiceWorkers! So say we all!\n";
  while (strChunk.length < 1024) {
    strChunk += strChunk;
  }
  const dataChunk = encoder.encode(strChunk);

  evt.waitUntil(
    new Promise(resolve => {
      let body = new ReadableStream({
        start: controller => {
          const closeStream = why => {
            console.log("closing stream: " + JSON.stringify(why) + "\n");
            clearInterval(intervalId);
            resolve();
            // In event of error, the controller will automatically have closed.
            if (why.why != "canceled") {
              try {
                controller.close();
              } catch (ex) {
                // If we thought we should cancel but experienced a problem,
                // that's a different kind of failure and we need to report it.
                // (If we didn't catch the exception here, we'd end up erroneously
                // in the tick() method's canceled handler.)
                channel.postMessage({
                  what: filename,
                  why: "close-failure",
                  message: ex.message,
                  ticks: why.ticks,
                });
                return;
              }
            }
            // Post prior to performing any attempt to close...
            channel.postMessage(why);
          };

          controller.enqueue(dataChunk);
          let count = 0;
          let intervalId;
          function tick() {
            try {
              // bound worst-case behavior.
              if (count++ > MAX_TICK_COUNT) {
                closeStream({
                  what: filename,
                  why: "timeout",
                  message: "timeout",
                  ticks: count,
                });
                return;
              }
              controller.enqueue(dataChunk);
            } catch (e) {
              closeStream({
                what: filename,
                why: "canceled",
                message: e.message,
                ticks: count,
              });
            }
          }
          // Alternately, streams' pull mechanism could be used here, but this
          // test doesn't so much want to saturate the stream as to make sure the
          // data is at least flowing a little bit.  (Also, the author had some
          // concern about slowing down the test by overwhelming the event loop
          // and concern that we might not have sufficent back-pressure plumbed
          // through and an infinite pipe might make bad things happen.)
          intervalId = setInterval(tick, TICK_INTERVAL);
          tick();
        },
      });
      evt.respondWith(
        new Response(body, {
          headers: {
            "Content-Disposition": `attachment; filename="${filename}"`,
            "Content-Type": "application/octet-stream",
          },
        })
      );
    })
  );
}

/**
 * Use an .sjs to generate a similar stream of data to the above, passing the
 * response through directly.  Because we're handing off the response but also
 * want to be able to report when cancellation occurs, we create a second,
 * overlapping long-poll style fetch that will not finish resolving until the
 * .sjs experiences closure of its socket and terminates the payload stream.
 */
function handlePassThrough(evt, filename) {
  evt.waitUntil(
    (async () => {
      console.log("issuing monitor fetch request");
      const response = await fetch("server-stream-download.sjs?monitor");
      console.log("monitor headers received, awaiting body");
      const data = await response.json();
      console.log("passthrough monitor fetch completed, notifying.");
      channel.postMessage({
        what: filename,
        why: data.why,
        message: data.message,
      });
    })()
  );
  evt.respondWith(
    fetch("server-stream-download.sjs").then(response => {
      console.log("server-stream-download.sjs Response received, propagating");
      return response;
    })
  );
}

addEventListener("fetch", evt => {
  console.log(`SW processing fetch of ${evt.request.url}`);
  if (evt.request.url.includes("sw-stream-download")) {
    handleStream(evt, "sw-stream-download");
    return;
  }
  if (evt.request.url.includes("sw-passthrough-download")) {
    handlePassThrough(evt, "sw-passthrough-download");
  }
});

addEventListener("message", evt => {
  if (evt.data === "claim") {
    evt.waitUntil(clients.claim());
  }
});
