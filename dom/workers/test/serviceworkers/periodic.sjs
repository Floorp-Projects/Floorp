function handleRequest(request, response) {
  var stateName = request.scheme + 'periodiccounter';
  if (request.queryString == 'clearcounter') {
    setState(stateName, '');
    return;
  }
  
  if (!getState(stateName)) {
    setState(stateName, '1');
  } else {
    // Make sure that we pass a string value to setState!
    setState(stateName, "" + (parseInt(getState(stateName)) + 1));
  }
  response.setHeader("Content-Type", "application/javascript", false);
  response.write(getScript(stateName));
}

function getScript(stateName) {
  return "onfetch = function(e) {" +
           "if (e.request.url.indexOf('get-sw-version') > -1) {" +
             "e.respondWith(new Response('" + getState(stateName) + "'));" +
           "}" +
         "};";
}
