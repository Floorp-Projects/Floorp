function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Cache-Control", "no-store", false);

  let state = getState("sessionhistory_do_redirect");
  if (state != "doredirect") {
    response.setHeader("Content-Type", "text/html");
    const contents = `
      <script>
        window.onpageshow = function(event) {
          opener.pageshow();
        }
      </script>
      `;
    response.write(contents);

    // The next load should do a redirect.
    setState("sessionhistory_do_redirect", "doredirect");
  } else {
    setState("sessionhistory_do_redirect", "");

    response.setStatusLine("1.1", 302, "Found");
    response.setHeader(
      "Location",
      "file_session_history_on_redirect_2.html",
      false
    );
  }
}
