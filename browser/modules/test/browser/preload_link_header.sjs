/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const document = `\
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8"/>
  </head>
  <body>
    <img src="http://example.com/browser/browser/modules/test/browser/moz.png">
  </body>
</html>
`;

const nestedDocument = `\
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8"/>
  </head>
  <body>
    <iframe src="http://example.org/browser/browser/modules/test/browser/preload_link_header.sjs"></iframe>
  </body>
</html>
`;

function handleRequest(req, resp) {
  resp.setHeader("Content-Type", "text/html");
  if (req.queryString === "nested") {
    resp.write(nestedDocument);
  } else {
    resp.setHeader(
      "Link",
      `<http://example.com/browser/browser/modules/test/browser/moz.png>; rel=preload; as=image`);
    resp.write(document);
  }
}
