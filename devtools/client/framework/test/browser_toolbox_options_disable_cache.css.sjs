/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  // This returns always new and different CSS content.
  const page = `body::before { content: "${Date.now()}"; }`;
  response.setHeader("Content-Type", "text/css; charset=utf-8", false);
  response.setHeader("Content-Length", page.length + "", false);
  response.write(page);
}
