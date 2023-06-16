addEventListener("install", function (evt) {
  evt.waitUntil(self.skipWaiting());
});

/**
 * Given a multipart/form-data encoded string that we know to have only a single
 * part, return the contents of the part.  (MIME multipart encoding is too
 * exciting to delve into.)
 */
function extractBlobFromMultipartFormData(text) {
  const lines = text.split(/\r\n/g);
  const firstBlank = lines.indexOf("");
  const foo = lines.slice(firstBlank + 1, -2).join("\n");
  return foo;
}

self.addEventListener("fetch", event => {
  const url = new URL(event.request.url);
  const mode = url.searchParams.get("mode");

  if (mode === "synthetic") {
    event.respondWith(
      (async () => {
        // This works even if there wasn't a body explicitly associated with the
        // request.  We just get a zero-length string in that case.
        const requestBodyContents = await event.request.text();
        const blobContents =
          extractBlobFromMultipartFormData(requestBodyContents);

        return new Response(
          `<!DOCTYPE HTML><head><meta charset="utf-8"/></head><body>
          <h1 id="url">${event.request.url}</h1>
          <div id="source">ServiceWorker</div>
          <div id="blob">${blobContents}</div>
        </body>`,
          { headers: { "Content-Type": "text/html" } }
        );
      })()
    );
  } else if (mode === "fetch") {
    event.respondWith(fetch(event.request));
  } else if (mode === "clone") {
    // In order for the act of cloning to be interesting, we want the original
    // request to remain alive so that any pipes end up having to buffer.
    self.originalRequest = event.request;
    event.respondWith(fetch(event.request.clone()));
  } else {
    event.respondWith(
      new Response(
        `<!DOCTYPE HTML><head><meta charset="utf-8"/></head><body>
          <h1 id="error">Bad mode: ${mode}</h1>
          <div id="source">ServiceWorker::Error</div>
          <div id="blob">No, this is an error.</div>
        </body>`,
        { headers: { "Content-Type": "text/html" }, status: 400 }
      )
    );
  }
});
