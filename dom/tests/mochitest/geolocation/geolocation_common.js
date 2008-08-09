function check_geolocation(location) {

  ok(location, "Check to see if this location is non-null");

  ok(location.latitude, "Check to see if there is a latitude");
  ok(location.longitude, "Check to see if there is a longitude");
  ok(location.altitude, "Check to see if there is a altitude");
  ok(location.horizontalAccuracy, "Check to see if there is a horizontalAccuracy");
  ok(location.verticalAccuracy, "Check to see if there is a verticalAccuracy");
  ok(location.timestamp, "Check to see if there is a timestamp");

}
