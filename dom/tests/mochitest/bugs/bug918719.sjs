// Keep track of one in-flight XHR by allowing secondary ones to
// tell us when the client has received a Progress event and wants
// more data or to stop the in-flight XHR.

const STATE_NAME = "bug918719_loading_event_test";
const CHUNK_DATA = "chunk";

function setReq(req) {
  setObjectState(STATE_NAME, req);
}

function getReq() {
  let req;
  getObjectState(STATE_NAME, function(v) {
    req = v;
  });
  return req;
}

function handleRequest(request, response)
{
  var pairs = request.queryString.split("&");
  var command = pairs.shift();

  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "no-cache", false);

  switch(command) {
    case "more":
      getReq().write(CHUNK_DATA);
      break;

    case "done":
      getReq().finish();
      setReq(null);
      break;

    default:
      response.processAsync();
      response.write(CHUNK_DATA);
      setReq(response);
      return;
  }

  response.setHeader("Content-Length", "2", false);
  response.write("ok");
}
