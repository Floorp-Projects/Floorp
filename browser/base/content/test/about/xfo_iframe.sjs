/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  // let's enjoy the amazing XFO setting
  response.setHeader("X-Frame-Options", "SAMEORIGIN");

  // let's avoid caching issues
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Cache-Control", "no-cache", false);

  // everything is fine - no needs to worry :)
  response.setStatusLine(request.httpVersion, 200);

  response.setHeader("Content-Type", "text/html", false);
  let txt =
    "<html><head><title>XFO page</title></head>" +
    "<body><h1>" +
    "XFO blocked page opened in new window!" +
    "</h1></body></html>";
  response.write(txt);

  let cookie = request.hasHeader("Cookie")
    ? request.getHeader("Cookie")
    : "<html><body>" +
      "<h2 id='strictCookie'>No same site strict cookie header</h2></body>" +
      "</html>";
  response.write(cookie);

  if (!request.hasHeader("Cookie")) {
    let strictCookie = `matchaCookie=creamy; Domain=.example.org; SameSite=Strict`;
    response.setHeader("Set-Cookie", strictCookie);
  }
}
