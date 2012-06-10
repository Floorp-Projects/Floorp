
do_load_httpd_js();

var httpserver = new nsHttpServer();
var testpath = "/simple";
var httpbody = "<?xml version='1.0' ?><root>0123456789</root>";

var cu = Components.utils;
var sb = cu.Sandbox(["http://www.example.com", 
	             "http://localhost:4444/simple"],
	             {wantXHRConstructor: true});

function createXHR(async)
{
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "http://localhost:4444/simple", async);
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

  // Test sync XHR sending
  cu.evalInSandbox('var createXHR = ' + createXHR.toString(), sb);
  var res = cu.evalInSandbox('var sync = createXHR(); sync.send(null); sync', sb);
  checkResults(res);

  // Test async XHR sending
  var async = cu.evalInSandbox('var async = createXHR(true); async', sb);
  async.addEventListener("readystatechange", function(event) {
    if (checkResults(async))
      httpserver.stop(do_test_finished);
  }, false);
  async.send(null);
  do_test_pending();
}

function serverHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/xml", false);
  response.bodyOutputStream.write(httpbody, httpbody.length);
}
