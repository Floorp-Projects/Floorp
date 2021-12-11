Cu.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
var httpserver2 = new HttpServer();
var httpserver3 = new HttpServer();
var testpath = "/simple";
var redirectpath = "/redirect";
var negativetestpath = "/negative";
var httpbody = "<?xml version='1.0' ?><root>0123456789</root>";

var sb = Cu.Sandbox(["http://www.example.com",
                     "http://localhost:4444/redirect",
                     "http://localhost:4444/simple",
                     "http://localhost:4446/redirect"],
                     { wantGlobalProperties: ["XMLHttpRequest"] });

function createXHR(loc, async)
{
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "http://localhost:" + loc, async);
  return xhr;
}

function checkResults(xhr)
{
  if (xhr.readyState != 4)
    return false;

  equal(xhr.status, 200);
  equal(xhr.responseText, httpbody);

  var root_node = xhr.responseXML.getElementsByTagName('root').item(0);
  equal(root_node.firstChild.data, "0123456789");
  return true;
}

var httpServersClosed = 0;
function finishIfDone()
{
  if (++httpServersClosed == 3)
    do_test_finished();
}

function run_test()
{
  do_get_profile();
  do_test_pending();

  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.registerPathHandler(redirectpath, redirectHandler1);
  httpserver.start(4444);

  httpserver2.registerPathHandler(negativetestpath, serverHandler);
  httpserver2.start(4445);

  httpserver3.registerPathHandler(redirectpath, redirectHandler2);
  httpserver3.start(4446);

  // Test sync XHR sending
  Cu.evalInSandbox('var createXHR = ' + createXHR.toString(), sb);
  var res = Cu.evalInSandbox('var sync = createXHR("4444/simple"); sync.send(null); sync', sb);
  Assert.ok(checkResults(res));

  var principal = res.responseXML.nodePrincipal;
  Assert.ok(principal.isContentPrincipal);
  var requestURL = "http://localhost:4444/simple";
  Assert.equal(principal.spec, requestURL);

  // negative test sync XHR sending (to ensure that the xhr do not have chrome caps, see bug 779821)
  try {
    Cu.evalInSandbox('var createXHR = ' + createXHR.toString(), sb);
    var res = Cu.evalInSandbox('var sync = createXHR("4445/negative"); sync.send(null); sync', sb);
    Assert.equal(false, true, "XHR created from sandbox should not have chrome caps");
  } catch (e) {
    Assert.ok(true);
  }

  // Test redirect handling.
  // This request bounces to server 2 and then back to server 1.  Neither of
  // these servers support CORS, but if the expanded principal is used as the
  // triggering principal, this should work.
  Cu.evalInSandbox('var createXHR = ' + createXHR.toString(), sb);
  var res = Cu.evalInSandbox('var sync = createXHR("4444/redirect"); sync.send(null); sync', sb);
  Assert.ok(checkResults(res));

  var principal = res.responseXML.nodePrincipal;
  Assert.ok(principal.isContentPrincipal);
  var requestURL = "http://localhost:4444/simple";
  Assert.equal(principal.spec, requestURL);

  httpserver2.stop(finishIfDone);
  httpserver3.stop(finishIfDone);

  // Test async XHR sending
  sb.finish = function(){
    httpserver.stop(finishIfDone);
  }

  // We want to execute checkResults from the scope of the sandbox as well to
  // make sure that there are no permission errors related to nsEP. For that
  // we need to clone the function into the sandbox and make a few things
  // available for it.
  Cu.evalInSandbox('var checkResults = ' + checkResults.toSource(), sb);
  sb.equal = equal;
  sb.httpbody = httpbody;

  function changeListener(event) {
    if (checkResults(async))
      finish();
  }

  var async = Cu.evalInSandbox('var async = createXHR("4444/simple", true);' +
                               'async.addEventListener("readystatechange", ' +
                                                       changeListener.toString() + ', false);' +
                               'async', sb);
  async.send(null);
}

function serverHandler(request, response)
{
  response.setHeader("Content-Type", "text/xml", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}

function redirectHandler1(request, response)
{
  response.setStatusLine(request.httpVersion, 302, "Found");
  response.setHeader("Location", "http://localhost:4446/redirect", false);
}

function redirectHandler2(request, response)
{
  response.setStatusLine(request.httpVersion, 302, "Found");
  response.setHeader("Location", "http://localhost:4444/simple", false);
}
