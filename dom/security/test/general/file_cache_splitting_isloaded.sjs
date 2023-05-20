/*
  Helper Server - 
  Send a Request with ?queryResult - response will be the 
  queryString of the next request.

*/
// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // save the object state of the initial request, which returns
  // async once the server has processed the img request.
  if (request.queryString.includes("wait")) {
    response.processAsync();
    setObjectState("wait", response);
    return;
  }

  response.write(IMG_BYTES);

  // return the result
  getObjectState("wait", function (queryResponse) {
    if (!queryResponse) {
      return;
    }
    queryResponse.write("1");
    queryResponse.finish();
  });
}
