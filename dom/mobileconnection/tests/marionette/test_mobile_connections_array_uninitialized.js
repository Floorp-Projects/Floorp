/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 1000;

SpecialPowers.addPermission("mobileconnection", true, document);

// Permission changes can't change existing Navigator.prototype
// objects, so grab our objects from a new Navigator
let ifr = document.createElement("iframe");
let connections;

ifr.onload = function() {
  connections = ifr.contentWindow.navigator.mozMobileConnections;

  // mozMobileConnections hasn't been initialized yet.
  ok(connections);
  is(connections.length, 1);

  ifr.parentNode.removeChild(ifr);
  ifr = null;
  connections = null;

  SpecialPowers.gc();
  cleanUp();
};
document.body.appendChild(ifr);

function cleanUp() {
  SpecialPowers.removePermission("mobileconnection", document);
  finish();
}
