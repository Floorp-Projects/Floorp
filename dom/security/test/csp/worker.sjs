Components.utils.importGlobalProperties(["URLSearchParams"]);

const SJS = "http://mochi.test:8888/tests/dom/security/test/csp/worker.sjs";

function createFetchWorker(url)
{
    return `fetch("${url}");`;
}

function createXHRWorker(url)
{
  return `
    try {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "${url}");
      xhr.send();
    } catch(ex) {}
  `;
}

function createImportScriptsWorker(url)
{
  return `
    try {
      importScripts("${url}");
    } catch(ex) {}
  `;
}

function createChildWorkerURL(params)
{
  let url = SJS + "?" + params.toString();
  return `new Worker("${url}");`;
}

function createChildWorkerBlob(params)
{
  let url = SJS + "?" + params.toString();
  return `
    try {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "${url}");
      xhr.responseType = "blob";
      xhr.send();
      xhr.onload = () => {
        new Worker(URL.createObjectURL(xhr.response));};
    } catch(ex) {}
  `;
}

function handleRequest(request, response)
{
  let params = new URLSearchParams(request.queryString);

  let id = params.get("id");
  let base = unescape(params.get("base"));
  let child = params.has("child") ? params.get("child") : "";

  //avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "application/javascript");

  // Deliver the CSP policy encoded in the URL
  if(params.has("csp")) {
    response.setHeader("Content-Security-Policy", unescape(params.get("csp")), false);
  }

  if (child) {
    let childCsp = params.has("childCsp") ? params.get("childCsp") : "";
    params.delete("csp");
    params.delete("child");
    params.delete("childCsp");
    params.append("csp", childCsp);

    switch (child) {
      case "blob":
        response.write(createChildWorkerBlob(params));
        break;

      case "url":
        response.write(createChildWorkerURL(params));
        break;

      default:
        response.setStatusLine(request.httpVersion, 400, "Bad request");
        break;
    }

    return;
  }

  if (params.has("action")) {
    switch (params.get("action")) {
      case "fetch":
        response.write(createFetchWorker(base + "?id=" + id));
        break;

      case "xhr":
        response.write(createXHRWorker(base + "?id=" + id));
        break;

      case "importScripts":
        response.write(createImportScriptsWorker(base + "?id=" + id));
        break;

      default:
        response.setStatusLine(request.httpVersion, 400, "Bad request");
        break;
    }

    return;
  }

  response.write("I don't know action ");
  return;
}
