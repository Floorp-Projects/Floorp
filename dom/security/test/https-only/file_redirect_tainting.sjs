/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

const body = `<!DOCTYPE html>
<html lang="en">
  <body>
    <script>
      let image = new Image();
      image.crossOrigin = "anonymous";
      image.src = "http://example.net/browser/dom/security/test/https-only/file_redirect_tainting.sjs?img";
      image.id = "test_image";
      document.body.appendChild(image);
    </script>
  </body>
</html>`;

function handleRequest(request, response) {
  if (request.queryString === "html") {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html");
    response.write(body);
    return;
  }

  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "image/png");
  let origin = request.getHeader("Origin");
  response.setHeader("Access-Control-Allow-Origin", origin);
  response.write(IMG_BYTES);
}
