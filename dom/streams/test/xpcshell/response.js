"use strict";

add_task(async function () {
  return new Response(new Blob([], { type: "text/plain" })).body.cancel();
});

add_task(function () {
  var response = new Response(
    new Blob(["This is data"], { type: "text/plain" })
  );
  var reader = response.body.getReader();
  reader.read();
  return reader.cancel();
});

add_task(function () {
  var response = new Response(new Blob(["T"], { type: "text/plain" }));
  var reader = response.body.getReader();

  var closedPromise = reader.closed.then(function () {
    return reader.cancel();
  });
  reader.read().then(function readMore({ done }) {
    if (!done) {
      return reader.read().then(readMore);
    }
    return undefined;
  });
  return closedPromise;
});
