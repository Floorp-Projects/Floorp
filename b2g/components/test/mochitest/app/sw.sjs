// The update process does not complete unless the service worker script changes.
// The service worker script is a server-side JS file which allows us to get a
// slightly different service worker script file each time the file is fetched.
// Everytime there is either a registration or an update the script will be
// fetched and a slightly different version of the script will be returned.

function handleRequest(request, response) {
  var stateName = request.scheme + "counter";
  if (!getState(stateName)) {
    setState(stateName, "1");
  } else {
    // Make sure that we pass a string value to setState!
    setState(stateName, "" + (parseInt(getState(stateName)) + 1));
  }
  response.setHeader("Content-Type", "application/javascript", false);
  response.write(getScript(stateName));
}

function getScript(stateName) {
  return "oninstall = function(evt) {" +
           "evt.waitUntil(self.skipWaiting());" +
         "}; " +
         "onfetch = function(evt) {" +
           "if (evt.request.url.indexOf('get-sw-version') > -1) {" +
             "evt.respondWith(new Response('" + getState(stateName) + "'));" +
           "}" +
         "};";
}
