
function handleRequest(request, response)
{

  var coords = {
    latitude: 37.41857,
    longitude: -122.08769,

    altitude: 42,
    accuracy: 42,
    altitudeAccuracy: 42,
    heading: 42,
    speed: 42,
  };

  var geoposition = {
    location: coords,
  };

  response.setStatusLine("1.0", 200, "OK");
  response.setHeader("Content-Type", "aplication/x-javascript", false);
  response.write(JSON.stringify(geoposition));
}

