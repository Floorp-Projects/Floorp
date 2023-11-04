// custom *.sjs for Bug 1224225
// Punycode in CSP host sources

const HTML_PART1 =
  "<!DOCTYPE HTML>" +
  '<html><head><meta charset="utf-8">' +
  "<title>Bug 1224225 - CSP source matching should work for punycoded domain names</title>" +
  "</head>" +
  "<body>" +
  "<script id='script' src='";

// U+00E4 LATIN SMALL LETTER A WITH DIAERESIS, encoded as UTF-8 code units.
// response.write() writes out the provided string characters truncated to
// bytes, so "ä" literally would write a literal \xE4 byte, not the desired
// two-byte UTF-8 sequence.
const TESTCASE1 = "http://sub2.\xC3\xA4lt.example.org/";
const TESTCASE2 = "http://sub2.xn--lt-uia.example.org/";

const HTML_PART2 =
  "tests/dom/security/test/csp/file_punycode_host_src.js'></script>" +
  "</body>" +
  "</html>";

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  const query = new URLSearchParams(request.queryString);

  if (query.get("csp")) {
    response.setHeader("Content-Security-Policy", query.get("csp"), false);
  }
  if (query.get("action") == "script-unicode-csp-punycode") {
    response.write(HTML_PART1 + TESTCASE1 + HTML_PART2);
    return;
  }
  if (query.get("action") == "script-punycode-csp-punycode") {
    response.write(HTML_PART1 + TESTCASE2 + HTML_PART2);
    return;
  }

  // we should never get here, but just in case
  // return something unexpected
  response.write("do'h");
}
