var BASE_URL = 'example.com/tests/dom/base/test/img_referrer_testserver.sjs';

function createTestUrl(aPolicy, aAction, aName) {
  return 'http://' + BASE_URL + '?' +
         'action=' + aAction + '&' +
         'policy=' + aPolicy + '&' +
         'name=' + aName;
}

function createTestPage(aHead, aImgPolicy, aName) {
  var _createTestUrl = createTestUrl.bind(null, aImgPolicy, 'test', aName);

  return '<!DOCTYPE HTML>\n\
         <html>'+
            aHead +
           '<body>\n\
             <img src="' + _createTestUrl('img') + '" referrer="' + aImgPolicy + '" id="image"></img>\n\
             <script>' +

               // LOAD EVENT (of the test)
               // fires when the img resource for the page is loaded
               'window.addEventListener("load", function() {\n\
                 parent.postMessage("childLoadComplete", "http://mochi.test:8888");\n\
               }.bind(window), false);' +

             '</script>\n\
           </body>\n\
         </html>';
}

// Creates the following test cases for the specified referrer
// policy combination:
//   <img> with referrer
function createTest(aPolicy, aImgPolicy, aName) {
  var headString = '<head>';
  if (aPolicy) {
    headString += '<meta name="referrer" content="' + aPolicy + '">';
  }

  headString += '<script></script>';

  return createTestPage(headString, aImgPolicy, aName);
}

// testing regular load img with referrer policy
// speculative parser should not kick in here
function createTest2(aImgPolicy, name) {
  return createTestPage('', aImgPolicy, name);
}

function createTest3(aImgPolicy1, aImgPolicy2, aImgPolicy3, aName) {
  return '<!DOCTYPE HTML>\n\
         <html>\n\
           <body>\n\
             <img src="' + createTestUrl(aImgPolicy1, 'test', aName + aImgPolicy1) + '" referrer="' + aImgPolicy1 + '" id="image"></img>\n\
             <img src="' + createTestUrl(aImgPolicy2, 'test', aName + aImgPolicy2) + '" referrer="' + aImgPolicy2 + '" id="image"></img>\n\
             <img src="' + createTestUrl(aImgPolicy3, 'test', aName + aImgPolicy3) + '" referrer="' + aImgPolicy3 + '" id="image"></img>\n\
             <script>\n\
               var _numLoads = 0;' +

               // LOAD EVENT (of the test)
               // fires when the img resource for the page is loaded
               'window.addEventListener("load", function() {\n\
                  parent.postMessage("childLoadComplete", "http://mochi.test:8888");\n\
               }.bind(window), false);' +

             '</script>\n\
           </body>\n\
         </html>';
}

function createTestPage2(aHead, aPolicy, aName) {
  return '<!DOCTYPE HTML>\n\
         <html>'+
            aHead +
           '<body>\n\
             <img src="' + createTestUrl(aPolicy, "test", aName) + '" id="image"></img>\n\
             <script>' +

               // LOAD EVENT (of the test)
               // fires when the img resource for the page is loaded
               'window.addEventListener("load", function() {\n\
                 parent.postMessage("childLoadComplete", "http://mochi.test:8888");\n\
               }.bind(window), false);' +

             '</script>\n\
           </body>\n\
         </html>';
}

function createTest4(aPolicy, aName) {
  var headString = '<head>';
  headString += '<meta name="referrer" content="' + aPolicy + '">';
  headString += '<script></script>';

  return createTestPage2(headString, aPolicy, aName);
}

function createTest5(aPolicy, aName) {
  var headString = '<head>';
  headString += '<meta name="referrer" content="' + aPolicy + '">';

  return createTestPage2(headString, aPolicy, aName);
}

function handleRequest(request, response) {
  var sharedKey = 'img_referrer_testserver.sjs';
  var params = request.queryString.split('&');
  var action = params[0].split('=')[1];

  response.setHeader('Cache-Control', 'no-cache', false);
  response.setHeader('Content-Type', 'text/html; charset=utf-8', false);

  if (action === 'resetState') {
    var state = getSharedState(sharedKey);
    state = {};
    setSharedState(sharedKey, JSON.stringify(state));
    response.write("");
    return;
  }
  if (action === 'test') {
    // ?action=test&policy=origin&name=name
    var policy = params[1].split('=')[1];
    var name = params[2].split('=')[1];
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
      if (referrer.indexOf("img_referrer_testserver") > 0) {
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
    return;
  }
  if (action === 'get-test-results') {
    // ?action=get-result
    response.write(getSharedState(sharedKey));
    return;
  }
  if (action === 'generate-img-policy-test') {
    // ?action=generate-img-policy-test&imgPolicy=b64-encoded-string&name=name&policy=b64-encoded-string
    var imgPolicy = unescape(params[1].split('=')[1]);
    var name = unescape(params[2].split('=')[1]);
    var metaPolicy = '';
    if (params[3]) {
      metaPolicy = params[3].split('=')[1];
    }

    response.write(createTest(metaPolicy, imgPolicy, name));
    return;
  }
  if (action === 'generate-img-policy-test2') {
    // ?action=generate-img-policy-test2&imgPolicy=b64-encoded-string&name=name
    var imgPolicy = unescape(params[1].split('=')[1]);
    var name = unescape(params[2].split('=')[1]);

    response.write(createTest2(imgPolicy, name));
    return;
  }
  if (action === 'generate-img-policy-test3') {
    // ?action=generate-img-policy-test3&imgPolicy1=b64-encoded-string&imgPolicy2=b64-encoded-string&imgPolicy3=b64-encoded-string&name=name
    var imgPolicy1 = unescape(params[1].split('=')[1]);
    var imgPolicy2 = unescape(params[2].split('=')[1]);
    var imgPolicy3 = unescape(params[3].split('=')[1]);
    var name = unescape(params[4].split('=')[1]);

    response.write(createTest3(imgPolicy1, imgPolicy2, imgPolicy3, name));
    return;
  }
  if (action === 'generate-img-policy-test4') {
    // ?action=generate-img-policy-test4&imgPolicy=b64-encoded-string&name=name
    var policy = unescape(params[1].split('=')[1]);
    var name = unescape(params[2].split('=')[1]);

    response.write(createTest4(policy, name));
    return;
  }
  if (action === 'generate-img-policy-test5') {
    // ?action=generate-img-policy-test5&policy=b64-encoded-string&name=name
    var policy = unescape(params[1].split('=')[1]);
    var name = unescape(params[2].split('=')[1]);

    response.write(createTest5(policy, name));
    return;
  }

  response.write("I don't know action "+action);
  return;
}
