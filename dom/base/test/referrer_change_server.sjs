var BASE_URL = 'example.com/tests/dom/base/test/referrer_change_server.sjs';

function createTestUrl(aPolicy, aAction, aName) {
  return 'http://' + BASE_URL + '?' +
         'action=' + aAction + '&' +
         'policy=' + aPolicy + '&' +
         'name=' + aName + '&' +
         'type=link';
}

function createTest(aMetaPolicy, aReferrerPolicy, aName) {
  return '<!DOCTYPE HTML>\n\
         <html>'+
           '<meta name="referrer" content="' + aMetaPolicy + '">' +
           '<body>' +
             '<a href="' + createTestUrl(aReferrerPolicy, 'test', aName + aReferrerPolicy) + '" id="link">' + aReferrerPolicy + '</a>' +
             '<script>' +

               // LOAD EVENT (of the test)
               // fires when the page is loaded, then click link
               // first change meta referrer, then click link
               'window.addEventListener("load", function() {\n\
                 document.getElementsByName("referrer")[0].content = "'+aReferrerPolicy+'";\n\
                 document.getElementById("link").click();\n\
               }.bind(window), false);' +

             '</script>\n\
           </body>\n\
         </html>';
}

function createTest2(aMetaPolicy, aReferrerPolicy, aName) {
  return '<!DOCTYPE HTML>\n\
         <html>'+
           '<meta name="referrer" content="' + aMetaPolicy + '">' +
           '<body>' +
             '<a href="' + createTestUrl(aReferrerPolicy, 'test', aName + aReferrerPolicy) + '" id="link">' + aReferrerPolicy + '</a>' +
             '<script>' +

               // LOAD EVENT (of the test)
               // fires when the page is loaded, then click link
               // first change meta referrer, then click link
               'window.addEventListener("load", function() {\n\
                 document.getElementsByName("referrer")[0].setAttribute("content", "'+aReferrerPolicy+'");\n\
                 document.getElementById("link").click();\n\
               }.bind(window), false);' +

             '</script>\n\
           </body>\n\
         </html>';
}

function handleRequest(request, response) {
  var sharedKey = 'referrer_change_server.sjs';
  var params = request.queryString.split('&');
  var action = params[0].split('=')[1];

  if (action === 'resetState') {
    var state = getSharedState(sharedKey);
    state = {};
    setSharedState(sharedKey, JSON.stringify(state));
    response.write("");
    return;
  } else if (action === 'test') {
    // ?action=test&policy=origin&name=name
    var policy = params[1].split('=')[1];
    var name = params[2].split('=')[1];
    var type = params[3].split('=')[1];
    var result = getSharedState(sharedKey);

    if (result === '') {
      result = {};
    } else {
      result = JSON.parse(result);
    }

    if (!result["tests"]) {
      result["tests"] = {};
    }

    var referrerLevel = "none";
    var test = {}
    if (request.hasHeader('Referer')) {
        let referrer = request.getHeader('Referer');
        if (referrer.indexOf("referrer_change_server") > 0) {
          referrerLevel = "full";
        } else if (referrer == "http://mochi.test:8888") {
          referrerLevel = "origin";
        }
      test.referrer = request.getHeader('Referer');
    } else {
      test.referrer = '';
    }
    test.policy = referrerLevel;
    test.expected = policy;

    result["tests"][name] = test;

    setSharedState(sharedKey, JSON.stringify(result));

    // forward link click to redirect URL to finish test
    if (type === 'link') {
      var loc = 'https://example.com/tests/dom/base/test/file_change_policy_redirect.html';
      response.setStatusLine('1.1', 302, 'Found');
      response.setHeader('Location', loc, false);
    }

    return;
  } else if (action === 'get-test-results') {
    // ?action=get-result
    response.setHeader('Cache-Control', 'no-cache', false);
    response.setHeader('Content-Type', 'text/plain', false);
    response.write(getSharedState(sharedKey));
    return;
  } else if (action === 'generate-policy-test') {
    // ?action=generate-policy-test&referrerPolicy=b64-encoded-string&name=name&newPolicy=b64-encoded-string
    response.setHeader('Cache-Control', 'no-cache', false);
    response.setHeader('Content-Type', 'text/html; charset=utf-8', false);
    var referrerPolicy = unescape(params[1].split('=')[1]);
    var name = unescape(params[2].split('=')[1]);
    var newPolicy = params[3].split('=')[1];

    response.write(createTest(referrerPolicy, newPolicy, name));
    return;
  } else if (action === 'generate-policy-test2') {
    // ?action=generate-policy-test2&referrerPolicy=b64-encoded-string&name=name&newPolicy=b64-encoded-string
    response.setHeader('Cache-Control', 'no-cache', false);
    response.setHeader('Content-Type', 'text/html; charset=utf-8', false);
    var referrerPolicy = unescape(params[1].split('=')[1]);
    var name = unescape(params[2].split('=')[1]);
    var newPolicy = params[3].split('=')[1];

    response.write(createTest2(referrerPolicy, newPolicy, name));
    return;
  } else {
    response.write("I don't know action "+action);
    return;
  }
}
