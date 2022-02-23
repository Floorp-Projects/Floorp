function handleRequest(request, response) {
  let query = request.queryString;
  let action =
    query == "initial"
      ? "cache_control_max_age_3600.sjs?second"
      : "goback.html";
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-Control", "max-age=3600");
  response.write(
    "<html><head><script>window.blockBFCache = new RTCPeerConnection();</script></head>" +
      '<body onload=\'opener.postMessage("loaded", "*")\'>' +
      "<div id='content'>" +
      new Date().getTime() +
      "</div>" +
      "<form action='" +
      action +
      "' method='POST'></form>" +
      "</body></html>"
  );
}
