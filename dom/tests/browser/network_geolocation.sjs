function parseQueryString(str)
{
  if (str == "")
    return {};

  var paramArray = str.split("&");
  var regex = /^([^=]+)=(.*)$/;
  var params = {};
  for (var i = 0, sz = paramArray.length; i < sz; i++)
  {
    var match = regex.exec(paramArray[i]);
    if (!match)
      throw "Bad parameter in queryString!  '" + paramArray[i] + "'";
    params[decodeURIComponent(match[1])] = decodeURIComponent(match[2]);
  }

  return params;
}

function getPosition(expectedAccessToken, providedAccessToken, desiredAccessToken)
{  
  var response = {
    status: "OK",
    location: {
      lat: providedAccessToken ?
             (expectedAccessToken == providedAccessToken ? 200 : 404) : 200,
      lng: -122.08769,
    },
    accuracy: 100,
    access_token: desiredAccessToken
  };
  
  return JSON.stringify(response);
}

function handleRequest(request, response)
{
  var params = parseQueryString(request.queryString);

  response.setStatusLine("1.0", 200, "OK");
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "aplication/x-javascript", false);
  response.write(getPosition(params.expected_access_token, params.access_token,
                             params.desired_access_token));
}
