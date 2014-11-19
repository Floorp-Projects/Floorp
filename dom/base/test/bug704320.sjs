var BASE_URL = 'example.com/tests/dom/base/test/bug704320.sjs';

function createTestUrl(schemeFrom, schemeTo, policy, action, type) {
  return schemeTo + '://' + BASE_URL + '?' +
         'action=' + action + '&' +
         'scheme=' + schemeFrom + '-to-' + schemeTo + '&' +
         'policy=' + policy + '&' +
         'type=' + type;
}

function create2ndLevelIframeUrl(schemeFrom, schemeTo, policy, type) {
  return schemeFrom + '://' + BASE_URL + '?' +
         'action=create-2nd-level-iframe&' +
         'scheme-from=' + schemeFrom + '&' +
         'scheme-to=' + schemeTo + '&' +
         'policy=' + policy + '&' +
         'type=' + type;
}

// Creates the following test cases for the specified scheme and referrer
// policy combination:
//   <link>
//   @import
//   font-face
//   bg-url
//   <script>
//   <img>
//   <iframe>
//   <audio>
//   <video>
//   <object type="bogus"> 
//   <object type="image/svg+xml">
//   <a>
//   <a ping>
//   <form>
//   window.location
//   window.open
//   XMLHttpRequest
//   EventSource
//   TODO: XSLT?
//
// This returns a page that loads all of the above resources and contains a
// script that clicks a link after all resources are (hopefully)
// loaded. The click triggers a redirection to file_bug704320_redirect.html,
// which in turn notifies the main window that it's time to check the test
// results.
function createTest(schemeFrom, schemeTo, policy) {
  var _createTestUrl = createTestUrl.bind(
      null, schemeFrom, schemeTo, policy, 'test');

  var _create2ndLevelIframeUrl = create2ndLevelIframeUrl.bind(
      null, schemeFrom, schemeTo, policy);

  return '<!DOCTYPE HTML>\n\
         <html>\n\
         <head>\n\
           <meta name="referrer" content="' + policy + '">\n\
           <link rel="stylesheet" type="text/css" href="' + _createTestUrl('stylesheet') + '">\n\
           <style type="text/css">\n\
             @import "' + _createTestUrl('import-css') + '";\n\
             @font-face {\n\
               font-family: "Fake Serif Bold";\n\
               src: url("' + _createTestUrl('font-face') + '");\n\
             }\n\
             body {\n\
               font-family: "Fake Serif Bold", serif;\n\
               background: url("' + _createTestUrl('bg-url') + '");\n\
             }\n\
           </style>\n\
         </head>\n\
         <body>\n\
           <script src="' + _createTestUrl('script') + '"></script>\n\
           <img src="' + _createTestUrl('img') + '"></img>\n\
           <iframe src="' + _createTestUrl('iframe') + '"></iframe>\n\
           <audio src="' + _createTestUrl('audio') + '"></audio>\n\
           <video src="' + _createTestUrl('video') + '"></video>\n\
           <object type="bogus" data="' + _createTestUrl('object') + '"></object>\n\
           <object type="image/svg+xml" data="' + _createTestUrl('object-svg') + '"></object>\n\
           <a id="link" href="' + _createTestUrl('link') + '" ping="' + _createTestUrl('link-ping') + '"></a>\n\
           <iframe src="' + _create2ndLevelIframeUrl('form') + '"></iframe>\n\
           <iframe src="' + _create2ndLevelIframeUrl('window.location') + '"></iframe>\n\
           <script>\n\
             (function() {\n\
               var x = new XMLHttpRequest();\n\
               x.open("GET", "' + _createTestUrl('xmlhttprequest') + '");\n\
               x.send();\n\
             })();\n\
             (function() {\n\
               var eventSource = new EventSource("' + _createTestUrl('eventsource') + '");\n\
             })();' +

             // LOAD EVENT (most of the tests)
             // fires when the resources for the page are loaded
             'var _isLoaded = false;\n\
             window.addEventListener("load", function() {\n\
               this._isLoaded = true;\n\
               this.checkForFinish();\n\
             }.bind(window), false);' +

             // WINDOW.OPEN test
             // listen for incoming status from window.open, close the window
             // and check if we're done.
             'var _openedWindowLoaded = false;\n\
             window.addEventListener("message", function(message) {\n\
               if (message.data == "window.open") {\n\
                 this._openedWindowLoaded = true;\n\
                 this.win.close();\n\
                 this.checkForFinish();\n\
               }\n\
             }.bind(window), false);\n\
             var win = window.open("' + _createTestUrl('window.open') + '", "");' +

             // called by the two things that must complete: window.open page
             // and the window load event.  When both are complete, this
             // "finishes" the iframe subtest by clicking the link.
             'function checkForFinish() {\n\
               if (window._isLoaded && window._openedWindowLoaded) {\n\
                 document.getElementById("link").click();\n\
               }\n\
             }\n\
           </script>\n\
         </body>\n\
         </html>';
}

function createIframedFormTest(schemeFrom, schemeTo, policy) {
  var actionUrl = schemeTo + '://' + BASE_URL;

  return '<!DOCTYPE HTML>\n\
         <html>\n\
         <head>\n\
           <meta name="referrer" content="' + policy + '">\n\
         </head>\n\
         <body>\n\
           <form id="form" action="' + actionUrl + '">\n\
             <input type="hidden" name="action" value="test">\n\
             <input type="hidden" name="scheme" value="' + schemeFrom + '-to-' + schemeTo + '">\n\
             <input type="hidden" name="policy" value="' + policy + '">\n\
             <input type="hidden" name="type" value="form">\n\
           </form>\n\
           <script>\n\
             document.getElementById("form").submit();\n\
           </script>\n\
         </body>\n\
         </html>';
}

function createIframedWindowLocationTest(schemeFrom, schemeTo, policy) {
  var url = createTestUrl(
      schemeFrom, schemeTo, policy, 'test', 'window.location');

  return '<!DOCTYPE HTML>\n\
         <html>\n\
         <head>\n\
           <meta name="referrer" content="' + policy + '">\n\
         </head>\n\
         <body>\n\
           <script>\n\
            window.location = "' + url + '";\n\
           </script>\n\
         </body>\n\
         </html>';
}

function createPolicyTest(refpol) {
  return '<!DOCTYPE HTML>\n\
          <html>\n\
          <head>\n\
            <meta name="referrer" content="' + refpol + '">\n\
            <script type="text/javascript" src="/tests/dom/base/test/file_bug704320_preload_common.js"></script>\n\
          </head>\n\
          <body>\n\
            <img src="/tests/dom/base/test/bug704320_counter.sjs?type=img"\n\
                    onload="incrementLoad2(\'img\', 2);">\n\
            <img src="http://example.com/tests/dom/base/test/bug704320_counter.sjs?type=img"\n\
                    onload="incrementLoad2(\'img\', 2);">\n\
          </body>\n\
          </html>';
}

function handleRequest(request, response) {
  var sharedKey = 'bug704320.sjs';
  var params = request.queryString.split('&');
  var action = params[0].split('=')[1];

  if (action === 'create-1st-level-iframe') {
    // ?action=create-1st-level-iframe&scheme-from=http&scheme-to=https&policy=origin
    var schemeFrom = params[1].split('=')[1];
    var schemeTo = params[2].split('=')[1];
    var policy = params[3].split('=')[1];

    response.setHeader('Content-Type', 'text/html; charset=utf-8', false);
    response.setHeader('Cache-Control', 'no-cache', false);
    response.write(createTest(schemeFrom, schemeTo, policy));
  }
  else if (action === 'create-2nd-level-iframe') {
    // ?action=create-2nd-level-iframe&scheme-from=http&scheme-to=https&policy=origin&type=form"
    var schemeFrom = params[1].split('=')[1];
    var schemeTo = params[2].split('=')[1];
    var policy = params[3].split('=')[1];
    var type = params[4].split('=')[1];

    response.setHeader('Content-Type', 'text/html; charset=utf-8', false);
    response.setHeader('Cache-Control', 'no-cache', false);

    if (type === 'form') {
      response.write(createIframedFormTest(schemeFrom, schemeTo, policy));
    } else if (type === 'window.location') {
      response.write(createIframedWindowLocationTest(
            schemeFrom, schemeTo, policy));
    }
  }
  else if (action === 'test') {
    // ?action=test&scheme=http-to-https&policy=origin&type=img
    var scheme = params[1].split('=')[1];
    var policy = params[2].split('=')[1];
    var type = params[3].split('=')[1];
    var result = getSharedState(sharedKey);

    if (result === '') {
      result = {};
    } else {
      result = JSON.parse(result);
    }

    if (!result[type]) {
      result[type] = {};
    }

    if (!result[type][scheme]) {
      result[type][scheme] = {};
    }

    if (request.hasHeader('Referer')) {
      result[type][scheme][policy] = request.getHeader('Referer');
    } else {
      result[type][scheme][policy] = '';
    }

    setSharedState(sharedKey, JSON.stringify(result));

    if (type === 'link') {
      var loc = 'https://example.com/tests/dom/base/test/file_bug704320_redirect.html';
      response.setStatusLine('1.1', 302, 'Found');
      response.setHeader('Location', loc, false);
    }

    if (type === 'window.open') {
      response.setHeader('Cache-Control', 'no-cache', false);
      response.setHeader('Content-Type', 'text/html', false);
      response.write('<html><body><script>'
                   + 'window.opener.postMessage("window.open", "*");'
                   + '</script></body></html>');
    }
  }
  else if (action === 'get-test-results') {
    // ?action=get-result
    response.setHeader('Cache-Control', 'no-cache', false);
    response.setHeader('Content-Type', 'text/plain', false);
    response.write(getSharedState(sharedKey));
  }
  else if (action === 'generate-policy-test') {
    // ?action=generate-policy-test&policy=b64-encoded-string
    response.setHeader('Cache-Control', 'no-cache', false);
    response.setHeader('Content-Type', 'text/html', false);
    var refpol = unescape(params[1].split('=')[1]);
    response.write(createPolicyTest(refpol));
  }
}
