/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  if (
    request.queryString === "check_referrer" &&
    (!request.hasHeader("referer") ||
      request.getHeader("referer") !==
        "https://example.com/browser/dom/security/test/referrer-policy/file_session_history.sjs")
  ) {
    response.setStatusLine(request.httpVersion, 400, "Bad Request");
    response.write("Did not receive referrer");
  } else {
    response.setHeader("Content-Type", "text/html");
    response.write(
      `<span id="ok">OK</span>
  <a id="check_referrer" href="?check_referrer">check_referrer</a>
  <a id="fragment" href="#fragment">fragment</a>
  <script>
  function pushState(){
    history.pushState({}, "", location);
  }
  </script>
  <button id="push_state" onclick="pushState();" >push_state</button>`
    );
  }
}
