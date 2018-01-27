function sendResponseToParent(response) {
  return `
    <!DOCTYPE html>
    <script>
      window.parent.postMessage({status: "done", data: "${response}"}, "*");
    </script>
  `;
}

self.addEventListener("fetch", function(event) {
  if (event.request.url.indexOf("index.html") >= 0) {
    var response = "good";
    try {
      importScripts("http://example.org/tests/dom/workers/test/foreign.js");
    } catch(e) {
      dump("Got error " + e + " when importing the script\n");
    }
    if (response === "good") {
      try {
        importScripts("/tests/dom/workers/test/redirect_to_foreign.sjs");
      } catch(e) {
        dump("Got error " + e + " when importing the script\n");
      }
    }
    event.respondWith(new Response(sendResponseToParent(response),
                                   {headers: {'Content-Type': 'text/html'}}));
  }
});
