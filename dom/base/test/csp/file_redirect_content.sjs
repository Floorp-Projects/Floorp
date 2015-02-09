// https://bugzilla.mozilla.org/show_bug.cgi?id=650386
// This SJS file serves file_redirect_content.html
// with a CSP that will trigger a violation and that will report it
// to file_redirect_report.sjs
//
// This handles 301, 302, 303 and 307 redirects. The HTTP status code
// returned/type of redirect to do comes from the query string
// parameter passed in from the test_bug650386_* files and then also
// uses that value in the report-uri parameter of the CSP
function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  // this gets used in the CSP as part of the report URI.
  var redirect = request.queryString;

  if (redirect < 301 || (redirect > 303 && redirect <= 306) || redirect > 307) {
    // if we somehow got some bogus redirect code here,
    // do a 302 redirect to the same URL as the report URI
    // redirects to - this will fail the test.
    var loc = "http://example.com/some/fake/path";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", loc, false);
    return;
  }

  var csp = "default-src \'self\';report-uri http://mochi.test:8888/tests/dom/base/test/csp/file_redirect_report.sjs?" + redirect;

  response.setHeader("Content-Security-Policy", csp, false);

  // the actual file content.
  // this image load will (intentionally) fail due to the CSP policy of default-src: 'self'
  // specified by the CSP string above.
  var content = "<!DOCTYPE HTML><html><body><img src = \"http://some.other.domain.example.com\"></body></html>";

  response.write(content);

  return;
}
