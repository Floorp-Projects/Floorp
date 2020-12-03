"use strict";

function handleRequest(request, response) {
  response.setHeader(
    "Content-Type",
    "multipart/x-mixed-replace;boundary=BOUNDARY",
    false
  );
  response.setStatusLine(request.httpVersion, 200, "OK");

  response.write(`--BOUNDARY
Content-Type: text/html

<h1>First</h1>
Will be replaced
--BOUNDARY
Content-Type: text/html

<h1>Second</h1>
This will stick around
--BOUNDARY
--BOUNDARY--
`);
}
