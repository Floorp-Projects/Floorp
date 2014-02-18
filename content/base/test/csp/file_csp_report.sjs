// SJS file for CSP violation report test
// https://bugzilla.mozilla.org/show_bug.cgi?id=548193
function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  response.setHeader("Content-Type", "text/html", false);

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // set CSP header
  response.setHeader("X-Content-Security-Policy",
                     "allow 'self'; report-uri http://mochi.test:8888/csp-report.cgi",
                     false);

  // content which will trigger a violation report
  response.write('<html><body>');
  response.write('<img src="http://example.org/tests/content/base/test/file_CSP.sjs?testid=img_bad&type=img/png"> </img>');
  response.write('</body></html>');
}
