// SJS file for CSP frame ancestor mochitests
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

  // grab the desired policy from the query, and then serve a page
  if (query['csp'])
    response.setHeader("Content-Security-Policy",
                        unescape(query['csp']),
                        false);
  if (query['scriptedreport']) {
    // spit back a script that records that the page loaded
    response.setHeader("Content-Type", "text/javascript", false);
    if (query['double'])
      response.write('window.parent.parent.parent.postMessage({call: "frameLoaded", testname: "' + query['scriptedreport'] + '", uri: "window.location.toString()"}, "*");');
    else
      response.write('window.parent.parent.postMessage({call: "frameLoaded", testname: "' + query['scriptedreport'] + '", uri: "window.location.toString()"}, "*");');
  } else if (query['internalframe']) {
    // spit back an internal iframe (one that might be blocked)
    response.setHeader("Content-Type", "text/html", false);
    response.write('<html><head>');
    if (query['double'])
      response.write('<script src="file_CSP_frameancestors.sjs?double=1&scriptedreport=' + query['testid'] + '"></script>');
    else
      response.write('<script src="file_CSP_frameancestors.sjs?scriptedreport=' + query['testid'] + '"></script>');
    response.write('</head><body>');
    response.write(unescape(query['internalframe']));
    response.write('</body></html>');
  } else if (query['externalframe']) {
    // spit back an internal iframe (one that won't be blocked, and probably
    // has no CSP)
    response.setHeader("Content-Type", "text/html", false);
    response.write('<html><head>');
    response.write('</head><body>');
    response.write(unescape(query['externalframe']));
    response.write('</body></html>');
  } else {
    // default case: error.
    response.setHeader("Content-Type", "text/html", false);
    response.write('<html><body>');
    response.write("ERROR: not sure what to serve.");
    response.write('</body></html>');
  }
}
