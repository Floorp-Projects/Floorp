
var cu = Components.utils;
cu.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
var httpserver2 = new HttpServer();
var testpath = "/simple";
var negativetestpath = "/negative";
var httpbody = "<?xml version='1.0' ?><root>0123456789</root>";

var sb = cu.Sandbox(["http://www.example.com",
                     "http://localhost:4444/simple"],
                     {wantXHRConstructor: true});

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

  do_check_eq(xhr.status, 200);
  do_check_eq(xhr.responseText, httpbody);

  var root_node = xhr.responseXML.getElementsByTagName('root').item(0);
  do_check_eq(root_node.firstChild.data, "0123456789");
  return true;
}

function run_test()
{
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(4444);

  httpserver2.registerPathHandler(negativetestpath, serverHandler);
  httpserver2.start(4445);

  // Test sync XHR sending
  cu.evalInSandbox('var createXHR = ' + createXHR.toString(), sb);
  var res = cu.evalInSandbox('var sync = createXHR("4444/simple"); sync.send(null); sync', sb);
  checkResults(res);

  // Test async XHR sending
  var async = cu.evalInSandbox('var async = createXHR("4444/simple", true); async', sb);
  async.addEventListener("readystatechange", function(event) {
    if (checkResults(async))
      httpserver.stop(do_test_finished);
  }, false);
  async.send(null);

  // negative test sync XHR sending (to ensure that the xhr do not have chrome caps, see bug 779821)
  try {
    cu.evalInSandbox('var createXHR = ' + createXHR.toString(), sb);
    var res = cu.evalInSandbox('var sync = createXHR("4445/negative"); sync.send(null); sync', sb);
    do_check_false(true, "XHR created from sandbox should not have chrome caps");
  } catch (e) {
    do_check_true(true);
  }
  
  do_test_pending();
}

function serverHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/xml", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}
