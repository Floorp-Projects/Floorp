var BASE_URL = 'example.com/tests/dom/base/test/img_referrer_testserver.sjs';
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
  "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");

function createTestUrl(aPolicy, aAction, aName, aContent) {
  var content = aContent || 'text';
  return 'http://' + BASE_URL + '?' +
         'action=' + aAction + '&' +
         'policy=' + aPolicy + '&' +
         'name=' + aName + '&' +
         'content=' + content;
}

function createTestPage(aHead, aImgPolicy, aName) {
  var _createTestUrl = createTestUrl.bind(null, aImgPolicy, 'test', aName);

  return '<!DOCTYPE HTML>\n\
         <html>'+
            aHead +
           '<body>\n\
             <img src="' + _createTestUrl('img') + '" referrerpolicy="' + aImgPolicy + '" id="image"></img>\n\
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
             <img src="' + createTestUrl(aImgPolicy1, 'test', aName + aImgPolicy1) + '" referrerpolicy="' + aImgPolicy1 + '" id="image"></img>\n\
             <img src="' + createTestUrl(aImgPolicy2, 'test', aName + aImgPolicy2) + '" referrerpolicy="' + aImgPolicy2 + '" id="image"></img>\n\
             <img src="' + createTestUrl(aImgPolicy3, 'test', aName + aImgPolicy3) + '" referrerpolicy="' + aImgPolicy3 + '" id="image"></img>\n\
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

function createTestPage3(aHead, aPolicy, aName) {
  return '<!DOCTYPE HTML>\n\
         <html>'+
            aHead +
           '<body>\n\
             <script>' +
               'var image = new Image();\n\
               image.src = "' + createTestUrl(aPolicy, "test", aName, "image") + '";\n\
               image.referrerPolicy = "' + aPolicy + '";\n\
               image.onload = function() {\n\
                 window.parent.postMessage("childLoadComplete", "http://mochi.test:8888");\n\
               }\n\
               document.body.appendChild(image);' +

             '</script>\n\
           </body>\n\
         </html>';
}

function createTestPage4(aHead, aPolicy, aName) {
  return '<!DOCTYPE HTML>\n\
         <html>'+
            aHead +
           '<body>\n\
             <script>' +
               'var image = new Image();\n\
               image.referrerPolicy = "' + aPolicy + '";\n\
               image.src = "' + createTestUrl(aPolicy, "test", aName, "image") + '";\n\
               image.onload = function() {\n\
                 window.parent.postMessage("childLoadComplete", "http://mochi.test:8888");\n\
               }\n\
               document.body.appendChild(image);' +

             '</script>\n\
           </body>\n\
         </html>';
}

function createSetAttributeTest1(aPolicy, aImgPolicy, aName) {
  var headString = '<head>';
  headString += '<meta name="referrer" content="' + aPolicy + '">';
  headString += '<script></script>';

  return createTestPage3(headString, aImgPolicy, aName);
}

function createSetAttributeTest2(aPolicy, aImgPolicy, aName) {
  var headString = '<head>';
  headString += '<meta name="referrer" content="' + aPolicy + '">';
  headString += '<script></script>';

  return createTestPage4(headString, aImgPolicy, aName);
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
    // ?action=test&policy=origin&name=name&content=content
    var policy = params[1].split('=')[1];
    var name = params[2].split('=')[1];
    var content = params[3].split('=')[1];
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
      } else if (referrer == "http://mochi.test:8888/") {
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

    if (content === 'image') {
      response.setHeader("Content-Type", "image/png");
      response.write(IMG_BYTES);
    }
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

  if (action === 'generate-setAttribute-test1') {
    // ?action=generate-setAttribute-test1&policy=b64-encoded-string&name=name
    var imgPolicy = unescape(params[1].split('=')[1]);
    var policy = unescape(params[2].split('=')[1]);
    var name = unescape(params[3].split('=')[1]);

    response.write(createSetAttributeTest1(policy, imgPolicy, name));
    return;
  }

  if (action === 'generate-setAttribute-test2') {
    // ?action=generate-setAttribute-test2&policy=b64-encoded-string&name=name
    var imgPolicy = unescape(params[1].split('=')[1]);
    var policy = unescape(params[2].split('=')[1]);
    var name = unescape(params[3].split('=')[1]);

    response.write(createSetAttributeTest2(policy, imgPolicy, name));
    return;
  }

  response.write("I don't know action "+action);
  return;
}
