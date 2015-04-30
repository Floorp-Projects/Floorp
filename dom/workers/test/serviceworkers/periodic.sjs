function handleRequest(request, response) {
  if (!getState('periodiccounter')) {
    setState('periodiccounter', '1');
  } else {
    // Make sure that we pass a string value to setState!
    setState('periodiccounter', "" + (parseInt(getState('periodiccounter')) + 1));
  }
  response.setHeader("Content-Type", "application/javascript", false);
  response.write(getScript());
}

function getScript() {
  return "onfetch = function(e) {" +
           "if (e.request.url.indexOf('get-sw-version') > -1) {" +
             "e.respondWith(new Response('" + getState('periodiccounter') + "'));" +
           "}" +
         "};";
}
