/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

// stolen from file_blocked_script.sjs
function setGlobalState(data, key)
{
  x = { data: data, QueryInterface: function(iid) { return this } };
  x.wrappedJSObject = x;
  setObjectState(key, x);
}

function getGlobalState(key)
{
  var data;
  getObjectState(key, function(x) {
    data = x && x.wrappedJSObject.data;
  });
  return data;
}

/*
 * We want to let the sw_download_canceled.js service worker know when the
 * stream was canceled.  To this end, we let it issue a monitor request which we
 * fulfill when the stream has been canceled.  In order to coordinate between
 * multiple requests, we use the getObjectState/setObjectState mechanism that
 * httpd.js exposes to let data be shared and/or persist between requests.  We
 * handle both possible orderings of the requests because we currently don't
 * try and impose an ordering between the two requests as issued by the SW, and
 * file_blocked_script.sjs encourages us to do this, but we probably could order
 * them.
 */
const MONITOR_KEY = "stream-monitor";
function completeMonitorResponse(response, data) {
  response.write(JSON.stringify(data));
  response.finish();
}
function handleMonitorRequest(request, response) {
  response.setHeader("Content-Type", "application/json");
  response.setStatusLine(request.httpVersion, 200, "Found");

  response.processAsync();
  // Necessary to cause the headers to be flushed; that or touching the
  // bodyOutputStream getter.
  response.write("");
  dump("server-stream-download.js: monitor headers issued\n");

  const alreadyCompleted = getGlobalState(MONITOR_KEY);
  if (alreadyCompleted) {
    completeMonitorResponse(response, alreadyCompleted);
    setGlobalState(null, MONITOR_KEY);
  } else {
    setGlobalState(response, MONITOR_KEY);
  }
}

const MAX_TICK_COUNT = 3000;
const TICK_INTERVAL = 2;
function handleStreamRequest(request, response) {
  const name = "server-stream-download";

  // Create some payload to send.
  let strChunk =
    'Static routes are the future of ServiceWorkers! So say we all!\n';
  while (strChunk.length < 1024) {
    strChunk += strChunk;
  }

  response.setHeader("Content-Disposition", `attachment; filename="${name}"`);
  response.setHeader("Content-Type", `application/octet-stream; name="${name}"`);
  response.setHeader("Content-Length", `${strChunk.length * MAX_TICK_COUNT}`);
  response.setStatusLine(request.httpVersion, 200, "Found");

  response.processAsync();
  response.write(strChunk);
  dump("server-stream-download.js: stream headers + first payload issued\n");

  let count = 0;
  let intervalId;
  function closeStream(why, message) {
    dump("server-stream-download.js: closing stream: " + why + "\n");
    clearInterval(intervalId);
    response.finish();

    const data = { why, message };
    const monitorResponse = getGlobalState(MONITOR_KEY);
    if (monitorResponse) {
      completeMonitorResponse(monitorResponse, data);
      setGlobalState(null, MONITOR_KEY);
    } else {
      setGlobalState(data, MONITOR_KEY);
    }
  }
  function tick() {
    try {
      // bound worst-case behavior.
      if (count++ > MAX_TICK_COUNT) {
        closeStream("timeout", "timeout");
        return;
      }
      response.write(strChunk);
    } catch(e) {
      closeStream("canceled", e.message);
    }
  }
  intervalId = setInterval(tick, TICK_INTERVAL);
}

Components.utils.importGlobalProperties(["URLSearchParams"]);
function handleRequest(request, response) {
  dump("server-stream-download.js: processing request for " + request.path +
       "?" + request.queryString + "\n");
  const query = new URLSearchParams(request.queryString);
  if (query.has("monitor")) {
    handleMonitorRequest(request, response);
  } else {
    handleStreamRequest(request, response);
  }
}