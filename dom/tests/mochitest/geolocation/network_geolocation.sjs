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


function controlResponse(params)
{
  if (getState("sendresponse") == "") {
    setState("sendresponse", "true");
  }

  if (params.action == "stop-responding") {
    setState("sendresponse", "false");
  }

  if (params.action == "start-responding") {
    setState("sendresponse", "true");
  }

  if (params.action == "start-garbage") {
    setState("garbage", "true");
  }

  if (params.action == "stop-garbage") {
    setState("garbage", "false");
  }

}

function setPosition(params)
{
  var address = {
      street_number: "street_number",
      street: "street",
      premises: "premises",
      city: "city",
      county: "county",
      region: "region",
      country: "country",
      country_code: "country_code",
      postal_code: "postal_code",
  };

  // this isnt' the w3c data structure, it is the network location provider structure.

  var coords = {
    latitude: 37.41857,
    longitude: -122.08769,

    altitude: 42,
    accuracy: 42,
    altitude_accuracy: 42,
  };
  
  var geoposition = {
    location: coords,
  };

  if (getState("coords") == "") {
    setState("coords", JSON.stringify(geoposition));
  }

  var position = JSON.parse(getState("coords"));

  if (params.latitude != null) {
    position.location.latitude = params.latitude;
  }
  if (params.longitude != null) {
    position.location.longitude = params.longitude;
  }
  if (params.altitude != null) {
    position.location.altitude = params.altitude;
  }
  if (params.accuracy != null) {
    position.location.accuracy = params.accuracy;
  }
  if (params.altitudeAccuracy != null) {
    position.location.altitudeAccuracy = params.altitudeAccuracy;
  }
  if (params.heading != null) {
    position.location.heading = params.heading;
  }
  if (params.speed != null) {
    position.location.speed = params.speed;
  }

  setState("coords", JSON.stringify(position));
}

function handleRequest(request, response)
{
  var params = parseQueryString(request.queryString);

  controlResponse(params);
  setPosition(params);

  var sendresponse = getState("sendresponse");
  var position = getState("coords");

  if (getState("garbage") == "true") {
     // better way?
    var chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXTZabcdefghiklmnopqrstuvwxyz";
    position = "";
    var len = Math.floor(Math.random() * 5000);

    for (var i=0; i< len; i++) {
        var c = Math.floor(Math.random() * chars.length);
        position += chars.substring(c, c+1);
    }
  }


  if (sendresponse == "true") {
    response.setStatusLine("1.0", 200, "OK");
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "aplication/x-javascript", false);
    response.write(position);
  }
}

