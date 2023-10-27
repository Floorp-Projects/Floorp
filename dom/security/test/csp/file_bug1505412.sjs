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

  if (!redirect) {
    // if we somehow got some bogus redirect code here,
    // do a 302 redirect to the same URL as the report URI
    // redirects to - this will fail the test.
    var loc =
      "http://sub1.test1.example.org/tests/dom/security/test/csp/file_bug1505412.sjs?redirected";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", loc, false);
    return;
  }

  // response.setHeader("content-type", "text/application", false);
  // the actual file content.
  // this image load will (intentionally) fail due to the CSP policy of default-src: 'self'
  // specified by the CSP string above.
  var content = "info('Script Loaded')";

  response.write(content);
}
