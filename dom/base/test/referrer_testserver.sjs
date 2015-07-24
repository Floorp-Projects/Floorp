/*
* Test server for iframe, anchor, and area referrer attributes.
* https://bugzilla.mozilla.org/show_bug.cgi?id=1175736
* Also server for further referrer tests such as redirecting tests
* bug 1174913, bug 1175736, bug 1184781
*/

Components.utils.importGlobalProperties(["URLSearchParams"]);
const SJS = "referrer_testserver.sjs?";
const BASE_URL = "example.com/tests/dom/base/test/" + SJS;
const SHARED_KEY = SJS;
const SAME_ORIGIN = "mochi.test:8888/tests/dom/base/test/" + SJS;
const CROSS_ORIGIN = "test1.example.com/tests/dom/base/test/" + SJS;

const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
  "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");

function createTestUrl(aPolicy, aAction, aName, aType) {
  return "http://" + BASE_URL +
         "ACTION=" + aAction + "&" +
         "policy=" + aPolicy + "&" +
         "NAME=" + aName + "&" +
         "type=" + aType;
}

// test page using iframe referrer attribute
// if aParams are set this creates a test where the iframe url is a redirect
function createIframeTestPageUsingRefferer(aMetaPolicy, aAttributePolicy, aNewAttributePolicy, aName, aParams, aChangingMethod) {
  var metaString = "";
  if (aMetaPolicy) {
    metaString = `<meta name="referrer" content="${aMetaPolicy}">`;
  }
  var changeString = "";
  if (aChangingMethod === "setAttribute") {
    changeString = `document.getElementById("myframe").setAttribute("referrer", "${aNewAttributePolicy}")`;
  } else if (aChangingMethod === "property") {
    changeString = `document.getElementById("myframe").referrer = "${aNewAttributePolicy}"`;
  }
  var iFrameString = `<iframe src="" id="myframe" ${aAttributePolicy ? ` referrer="${aAttributePolicy}"` : ""}>iframe</iframe>`;
  var iframeUrl = "";
  if (aParams) {
    aParams.delete("ACTION");
    aParams.append("ACTION", "redirectIframe");
    iframeUrl = "http://" + CROSS_ORIGIN + aParams.toString();
  } else {
    iframeUrl = createTestUrl(aAttributePolicy, "test", aName, "iframe");
  }

  return `<!DOCTYPE HTML>
           <html>
             <head>
             ${metaString}
             </head>
             <body>
               ${iFrameString}
               <script>
                 window.addEventListener("load", function() {
                   ${changeString}
                   document.getElementById("myframe").onload = function(){
                    parent.postMessage("childLoadComplete", "http://mochi.test:8888");
                   };
                   document.getElementById("myframe").src = "${iframeUrl}";
                 }.bind(window), false);
               </script>
             </body>
           </html>`;
}

function buildAnchorString(aMetaPolicy, aReferrerPolicy, aName, aRelString){
  if (aReferrerPolicy) {
    return `<a href="${createTestUrl(aReferrerPolicy, 'test', aName, 'link')}" referrer="${aReferrerPolicy}" id="link" ${aRelString}>${aReferrerPolicy}</a>`;
  }
  return `<a href="${createTestUrl(aMetaPolicy, 'test', aName, 'link')}" id="link" ${aRelString}>link</a>`;
}

function buildAreaString(aMetaPolicy, aReferrerPolicy, aName, aRelString){
  var result = `<img src="file_mozfiledataurl_img.jpg" alt="image" usemap="#imageMap">`;
  result += `<map name="imageMap">`;
  if (aReferrerPolicy) {
    result += `<area shape="circle" coords="1,1,1" href="${createTestUrl(aReferrerPolicy, 'test', aName, 'link')}" alt="theArea" referrer="${aReferrerPolicy}" id="link" ${aRelString}>`;
  } else {
    result += `<area shape="circle" coords="1,1,1" href="${createTestUrl(aMetaPolicy, 'test', aName, 'link')}" alt="theArea" id="link" ${aRelString}>`;
  }
  result += `</map>`;

  return result;
}

// test page using anchor or area referrer attribute
function createAETestPageUsingRefferer(aMetaPolicy, aAttributePolicy, aNewAttributePolicy, aName, aRel, aStringBuilder, aChangingMethod) {
  var metaString = "";
  if (aMetaPolicy) {
    metaString = `<head><meta name="referrer" content="${aMetaPolicy}"></head>`;
  }
  var changeString = "";
  if (aChangingMethod === "setAttribute") {
    changeString = `document.getElementById("link").setAttribute("referrer", "${aNewAttributePolicy}")`;
  } else if (aChangingMethod === "property") {
    changeString = `document.getElementById("link").referrer = "${aNewAttributePolicy}"`;
  }
  var relString = "";
  if (aRel) {
    relString = `rel="noreferrer"`;
  }
  var elementString = aStringBuilder(aMetaPolicy, aAttributePolicy, aName, relString);

  return `<!DOCTYPE HTML>
           <html>
             ${metaString}
             <body>
               ${elementString}
               <script>
                 window.addEventListener("load", function() {
                   ${changeString}
                   document.getElementById("link").click();
                 }.bind(window), false);
               </script>
             </body>
           </html>`;
}

// creates test page with img that is a redirect
function createRedirectImgTestCase(aParams, aAttributePolicy) {
  var metaString = "";
  if (aParams.has("META_POLICY")) {
    metaString = `<meta name="referrer" content="${aParams.get('META_POLICY')}">`;
  }
  aParams.delete("ACTION");
  aParams.append("ACTION", "redirectImg");
  var imgUrl = "http://" + CROSS_ORIGIN + aParams.toString();

  return `<!DOCTYPE HTML>
          <html>
          <head>
          <meta charset="utf-8">
          ${metaString}
          <title>Test referrer policies on redirect (img)</title>
          </head>
          <body>
          <img id="testImg" src="${imgUrl}" ${aAttributePolicy ? ` referrer="${aAttributePolicy}"` : ""}>
          <script>
            window.addEventListener("load", function() {
              parent.postMessage("childLoadComplete", "http://mochi.test:8888");
            }.bind(window), false);
          </script>
          </body>
          </html>`;
}

function handleRequest(request, response) {
  var params = new URLSearchParams(request.queryString);
  var action = params.get("ACTION");

  if (action === "resetState") {
    setSharedState(SHARED_KEY, "{}");
    response.write("");
    return;
  }
  if (action === "get-test-results") {
    // ?action=get-result
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "text/plain", false);
    response.write(getSharedState(SHARED_KEY));
    return;
  }
  if (action === "redirect") {
    response.write('<script>parent.postMessage("childLoadComplete", "http://mochi.test:8888");</script>');
    return;
  }
  if (action === "redirectImg"){
    params.delete("ACTION");
    params.append("ACTION", "test");
    params.append("type", "img");
    // 302 found, 301 Moved Permanently, 303 See Other, 307 Temporary Redirect
    response.setStatusLine("1.1", 302, "found");
    response.setHeader("Location",  "http://" + CROSS_ORIGIN + params.toString(), false);
    return;
  }
  if (action === "redirectIframe"){
    params.delete("ACTION");
    params.append("ACTION", "test");
    params.append("type", "iframe");
    // 302 found, 301 Moved Permanently, 303 See Other, 307 Temporary Redirect
    response.setStatusLine("1.1", 302, "found");
    response.setHeader("Location",  "http://" + CROSS_ORIGIN + params.toString(), false);
    return;
  }
  if (action === "test") {
    // ?action=test&policy=origin&name=name
    var policy = params.get("policy");
    var name = params.get("NAME");
    var type = params.get("type");
    var result = getSharedState(SHARED_KEY);

    result = result ? JSON.parse(result) : {};

    var referrerLevel = "none";
    var test = {}
    if (request.hasHeader("Referer")) {
      var referrer = request.getHeader("Referer");
      if (referrer.indexOf("referrer_testserver") > 0) {
        referrerLevel = "full";
      } else if (referrer.indexOf("http://mochi.test:8888") == 0) {
        referrerLevel = "origin";
      } else {
        // this is never supposed to happen
        referrerLevel = "other-origin";
      }
      test.referrer = referrer;
    } else {
      test.referrer = "";
    }
    test.policy = referrerLevel;
    test.expected = policy;

    result[name] = test;

    setSharedState(SHARED_KEY, JSON.stringify(result));

    if (type === "img") {
      // return image
      response.setHeader("Content-Type", "image/png");
      response.write(IMG_BYTES);
      return;
    }
    if (type === "iframe") {
      // return iframe page
      response.write("<html><body>I am the iframe</body></html>");
      return;
    }
    if (type === "link") {
      // forward link click to redirect URL to finish test
      var loc = "http://" + BASE_URL + "ACTION=redirect";
      response.setStatusLine("1.1", 302, "Found");
      response.setHeader("Location", loc, false);
    }
    return;
  }

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);

  // parse test arguments and start test
  var attributePolicy = params.get("ATTRIBUTE_POLICY") || "";
  var newAttributePolicy = params.get("NEW_ATTRIBUTE_POLICY") || "";
  var metaPolicy = params.get("META_POLICY") || "";
  var rel = params.get("REL") || "";
  var name = params.get("NAME");

  // anchor & area
  var _getPage = createAETestPageUsingRefferer.bind(null, metaPolicy, attributePolicy, newAttributePolicy, name, rel);
  var _getAnchorPage = _getPage.bind(null, buildAnchorString);
  var _getAreaPage = _getPage.bind(null, buildAreaString);

  // aMetaPolicy, aAttributePolicy, aNewAttributePolicy, aName, aChangingMethod, aStringBuilder
  if (action === "generate-anchor-policy-test") {
    response.write(_getAnchorPage());
    return;
  }
  if (action === "generate-anchor-changing-policy-test-set-attribute") {
    response.write(_getAnchorPage("setAttribute"));
    return;
  }
  if (action === "generate-anchor-changing-policy-test-property") {
    response.write(_getAnchorPage("property"));
    return;
  }
  if (action === "generate-area-policy-test") {
    response.write(_getAreaPage());
    return;
  }
  if (action === "generate-area-changing-policy-test-set-attribute") {
    response.write(_getAreaPage("setAttribute"));
    return;
  }
  if (action === "generate-area-changing-policy-test-property") {
    response.write(_getAreaPage("property"));
    return;
  }

  // iframe
  _getPage = createIframeTestPageUsingRefferer.bind(null, metaPolicy, attributePolicy, newAttributePolicy, name, "");

  // aMetaPolicy, aAttributePolicy, aNewAttributePolicy, aName, aChangingMethod
  if (action === "generate-iframe-policy-test") {
    response.write(_getPage());
    return;
  }
  if (action === "generate-iframe-changing-policy-test-set-attribute") {
    response.write(_getPage("setAttribute"));
    return;
  }
  if (action === "generate-iframe-changing-policy-test-property") {
    response.write(_getPage("property"));
    return;
  }

  // redirect tests with img and iframe
  if (action === "generate-img-redirect-policy-test") {
    response.write(createRedirectImgTestCase(params, attributePolicy));
    return;
  }
  if (action === "generate-iframe-redirect-policy-test") {
    response.write(createIframeTestPageUsingRefferer(metaPolicy, attributePolicy, newAttributePolicy, name, params));
    return;
  }

  response.write("I don't know action " + action);
  return;
}
