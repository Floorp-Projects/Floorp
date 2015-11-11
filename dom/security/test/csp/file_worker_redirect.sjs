// testserver customized for the needs of:
// Bug 949706 - CSP: Correct handling of web workers importing scripts that get redirected

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  var query = request.queryString;

  if (query === "stage_0_script_loads_worker") {
    var newWorker =
      "var myWorker = new Worker(\"file_worker_redirect.sjs?stage_1_worker_import_scripts\");" +
      "myWorker.onmessage = function (event) { parent.checkResult(\"allowed\"); };" +
      "myWorker.onerror = function (event) { parent.checkResult(\"blocked\"); };";
    response.write(newWorker);
    return;
  }

  if (query === "stage_1_worker_import_scripts") {
    response.write("importScripts(\"file_worker_redirect.sjs?stage_2_redirect_imported_script\");");
    return;
  }

  if (query === "stage_2_redirect_imported_script") {
    var newLocation =
      "http://test1.example.com/tests/dom/security/test/csp/file_worker_redirect.sjs?stage_3_target_script";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", newLocation, false);
    return;
  }

  if (query === "stage_3_target_script") {
    response.write("postMessage(\"imported script loaded\");");
    return;
  }
}
