function parseQueryString(str) {
  if (str == "") {
    return {};
  }

  var paramArray = str.split("&");
  var regex = /^([^=]+)=(.*)$/;
  var params = {};
  for (var i = 0, sz = paramArray.length; i < sz; i++) {
    var match = regex.exec(paramArray[i]);
    if (!match) {
      throw new Error("Bad parameter in queryString!  '" + paramArray[i] + "'");
    }
    params[decodeURIComponent(match[1])] = decodeURIComponent(match[2]);
  }

  return params;
}

function getPosition(params) {
  var response = {
    status: "OK",
    accuracy: 100,
    location: {
      lat: params.lat,
      lng: params.lng,
    },
  };

  return JSON.stringify(response);
}

function handleRequest(request, response) {
  let params = parseQueryString(request.queryString);
  response.setStatusLine("1.0", 200, "OK");
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "application/x-javascript", false);
  response.write(getPosition(params));
}
