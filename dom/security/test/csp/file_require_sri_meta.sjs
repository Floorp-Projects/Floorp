// custom *.sjs for Bug 1277557
// META CSP: require-sri-for script;

const PRE_INTEGRITY =
  "<!DOCTYPE HTML>" +
  "<html><head><meta charset=\"utf-8\">" +
  "<title>Bug 1277557 - CSP require-sri-for does not block when CSP is in meta tag</title>" +
  "<meta http-equiv=\"Content-Security-Policy\" content=\"require-sri-for script; script-src 'unsafe-inline' *\">" +
  "</head>" +
  "<body>" +
  "<script id=\"testscript\"" +
  // Using math.random() to avoid confusing cache behaviors within the test
  "        src=\"http://mochi.test:8888/tests/dom/security/test/csp/file_require_sri_meta.js?" + Math.random() + "\"";

const WRONG_INTEGRITY =
  "        integrity=\"sha384-oqVuAfXRKap7fdgcCY5uykM6+R9GqQ8K/uxy9rx7HNQlGYl1kPzQho1wx4JwY8wC\"";

const CORRECT_INEGRITY =
  "        integrity=\"sha384-PkcuZQHmjBQKRyv1v3x0X8qFmXiSyFyYIP+f9SU86XWvRneifdNCPg2cYFWBuKsF\"";

const POST_INTEGRITY =
  "        onload=\"window.parent.postMessage({result: 'script-loaded'}, '*');\"" +
  "        onerror=\"window.parent.postMessage({result: 'script-blocked'}, '*');\"" +
  "></script>" +
  "</body>" +
  "</html>";

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  var queryString = request.queryString;

  if (queryString === "no-sri") {
    response.write(PRE_INTEGRITY + POST_INTEGRITY);
    return;
  }

  if (queryString === "wrong-sri") {
    response.write(PRE_INTEGRITY + WRONG_INTEGRITY + POST_INTEGRITY);
    return;
  }

  if (queryString === "correct-sri") {
    response.write(PRE_INTEGRITY + CORRECT_INEGRITY + POST_INTEGRITY);
    return;
  }

  // we should never get here, but just in case
  // return something unexpected
  response.write("do'h");
}
