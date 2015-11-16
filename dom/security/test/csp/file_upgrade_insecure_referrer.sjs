// special *.sjs specifically customized for the needs of
// Bug 1139297 and Bug 663570

const PRE_HEAD =
  "<!DOCTYPE HTML>" +
  "<html>" +
  "<head>";

 const POST_HEAD =
   "<meta charset='utf-8'>" +
   "<title>Bug 1139297 - Implement CSP upgrade-insecure-requests directive</title>" +
   "</head>" +
   "<body>" +
  "<img id='testimage' src='http://example.com/tests/dom/security/test/csp/file_upgrade_insecure_referrer_server.sjs?img'></img>" +
  "</body>" +
  "</html>";

const PRE_CSP = "upgrade-insecure-requests; default-src https:; ";
const CSP_REFERRER_ORIGIN = "referrer origin";
const CSP_REFEFFER_NO_REFERRER = "referrer no-referrer";

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  var queryString = request.queryString;

  if (queryString === "test1") {
    response.setHeader("Content-Security-Policy", PRE_CSP + CSP_REFERRER_ORIGIN, false);
    response.write(PRE_HEAD + POST_HEAD);
  	return;
  }

  if (queryString === "test2") {
    response.setHeader("Content-Security-Policy", PRE_CSP + CSP_REFEFFER_NO_REFERRER, false);
    response.write(PRE_HEAD + POST_HEAD);
  	return;
  }

  if (queryString === "test3") {
  	var metacsp =  "<meta http-equiv=\"Content-Security-Policy\" content = \"" + PRE_CSP + CSP_REFERRER_ORIGIN + "\" >";
    response.write(PRE_HEAD + metacsp + POST_HEAD);
  	return;
  }

  if (queryString === "test4") {
  	var metacsp =  "<meta http-equiv=\"Content-Security-Policy\" content = \"" + PRE_CSP + CSP_REFEFFER_NO_REFERRER + "\" >";
    response.write(PRE_HEAD + metacsp + POST_HEAD);
  	return;
  }

  // we should never get here, but just in case return
  // something unexpected
  response.write("do'h");
}
