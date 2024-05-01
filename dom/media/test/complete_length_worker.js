"use strict";

let client;
function is(got, expected, name) {
  client.postMessage({ type: "is", got, expected, name });
}

self.onactivate = e =>
  e.waitUntil(
    (async () => {
      await self.clients.claim();
      const allClients = await self.clients.matchAll();
      client = allClients[0];
      is(allClients.length, 1, "allClients.length");
    })()
  );

let expected_start = 0;
let response_data = [
  // One Array element for each response in order:
  {
    complete_length: "*",
    body: "O",
  },
  {
    complete_length: "3",
    body: "g",
  },
  {
    // Extend length to test that the remainder is fetched.
    complete_length: "6",
    body: "g",
  },
  {
    // Reduce length to test that no more is fetched.
    complete_length: "4",
    body: "S",
  },
];

self.onfetch = e => {
  if (!e.request.url.endsWith("/media-resource")) {
    return; // fall back to network fetch
  }
  is(
    response_data.length >= 1,
    true,
    `response_data.length (${response_data.length}) > 0`
  );
  const { complete_length, body } = response_data.shift();
  const range = e.request.headers.get("Range");
  const match = range.match(/^bytes=(\d+)-/);
  is(Array.isArray(match), true, `Array.isArray(match) for ${range}`);
  const first = parseInt(match[1]);
  is(first, expected_start, "first");
  const last = first + body.length - 1; // inclusive
  expected_start = last + 1;
  const init = {
    status: 206, // Partial Content
    headers: {
      "Accept-Ranges": "bytes",
      "Content-Type": "audio/ogg",
      "Content-Range": `bytes ${first}-${last}/${complete_length}`,
      "Content-Length": body.length,
    },
  };
  e.respondWith(new Response(body, init));
};

self.onmessage = e => {
  switch (e.data.type) {
    case "got error event":
      // Check that all expected requests were received.
      is(response_data.length, 0, "missing fetch count");
      client.postMessage({ type: "done" });
      return;
    default:
      is(e.data.type, "__KNOWN__", "e.data.type");
  }
};
