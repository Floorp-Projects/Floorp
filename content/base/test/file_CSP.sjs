// SJS file for CSP mochitests

function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  var isPreflight = request.method == "OPTIONS";


  //avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if ("main" in query) {
    var xhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]  
                   .createInstance(Components.interfaces.nsIXMLHttpRequest);
    //serve the main page with a CSP header!
    // -- anything served from 'self' (localhost:8888) will be allowed,
    // -- anything served from other hosts (example.com:80) will be blocked.
    // -- XHR tests are set up in the file_CSP_main.js file which is sourced.
    response.setHeader("X-Content-Security-Policy", 
                       "allow mochi.test",
                       false);

    var proxyURI = "http://" + query["proxy"];
    xhr.open("GET", proxyURI + "/tests/content/base/test/file_CSP_main.html", false);
    xhr.send(null);
    if(xhr.status == 200) {
      response.write(xhr.responseText);
    }
  } else {
    if ("type" in query) {
      response.setHeader("Content-Type", unescape(query['type']), false);
    } else {
      response.setHeader("Content-Type", "text/html", false);
    }

    if ("content" in query) {
      response.setHeader("Content-Type", "text/html", false);
      response.write(unescape(query['content']));
    }
  }
}
