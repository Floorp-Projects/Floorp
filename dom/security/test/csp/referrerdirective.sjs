// Used for bug 965727 to serve up really simple scripts reflecting the
// referrer sent to load this back to the loader.


function handleRequest(request, response) {
  // skip speculative loads.

  var splits = request.queryString.split('&');
  var params = {};
  splits.forEach(function(v) {
    let parts = v.split('=');
    params[parts[0]] = unescape(parts[1]);
  });

  var loadType = params['type'];
  var referrerLevel = 'error';

  if (request.hasHeader('Referer')) {
    var referrer = request.getHeader('Referer');
    if (referrer.indexOf("file_testserver.sjs") > -1) {
      referrerLevel = "full";
    } else {
      referrerLevel = "origin";
    }
  } else {
    referrerLevel = 'none';
  }

  var theScript = 'window.postResult("' + loadType + '", "' + referrerLevel + '");';
  response.setHeader('Content-Type', 'application/javascript; charset=utf-8', false);
  response.setHeader('Cache-Control', 'no-cache', false);

  if (request.method != "OPTIONS") {
  response.write(theScript);
  }
}
