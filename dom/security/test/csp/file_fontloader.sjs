// custom *.sjs for Bug 1195172
// CSP: 'block-all-mixed-content'

const PRE_HEAD =
  "<!DOCTYPE HTML>" +
  "<html><head><meta charset=\"utf-8\">" +
  "<title>Bug 1195172 - CSP should block font from cache</title>";

const CSP_BLOCK =
  "<meta http-equiv=\"Content-Security-Policy\" content=\"font-src 'none'\">";

const CSP_ALLOW =
  "<meta http-equiv=\"Content-Security-Policy\" content=\"font-src *\">";

const CSS =
  "<style>" +
  "  @font-face {" +
  "    font-family: myFontTest;" +
  "    src: url(file_fontloader.woff);" +
  "  }" +
  "  div {" +
  "    font-family: myFontTest;" +
  "  }" +
  "</style>";

const POST_HEAD_AND_BODY =
  "</head>" +
  "<body>" +
  "<div> Just testing the font </div>" +
  "</body>" +
  "</html>";

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
 
  var queryString = request.queryString;

  if (queryString == "baseline") {
    response.write(PRE_HEAD + POST_HEAD_AND_BODY);
    return;
  }
  if (queryString == "no-csp") {
  	response.write(PRE_HEAD + CSS + POST_HEAD_AND_BODY);
  	return;
  }
  if (queryString == "csp-block") {
  	response.write(PRE_HEAD + CSP_BLOCK + CSS + POST_HEAD_AND_BODY);
    return;
  }
  if (queryString == "csp-allow") {
  	response.write(PRE_HEAD + CSP_ALLOW + CSS + POST_HEAD_AND_BODY);
    return;
  }
  // we should never get here, but just in case return something unexpected
  response.write("do'h");
}
