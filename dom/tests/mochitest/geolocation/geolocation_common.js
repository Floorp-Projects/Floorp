function check_geolocation(location) {

  ok(location, "Check to see if this location is non-null");

  ok(location.latitude, "Check to see if there is a latitude");
  ok(location.longitude, "Check to see if there is a longitude");
  ok(location.altitude, "Check to see if there is a altitude");
  ok(location.accuracy, "Check to see if there is a accuracy");
  ok(location.altitudeAccuracy, "Check to see if there is a alt accuracy");

  ok(location.heading, "Check to see if there is a heading");
  ok(location.velocity, "Check to see if there is a velocity");

  ok(location.timestamp, "Check to see if there is a timestamp");

}
