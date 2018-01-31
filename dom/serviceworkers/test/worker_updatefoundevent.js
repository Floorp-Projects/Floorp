/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

registration.onupdatefound = function(e) {
  clients.matchAll().then(function(clients) {
    if (!clients.length) {
      reject("No clients found");
    }

    if (registration.scope.match(/updatefoundevent\.html$/)) {
      clients[0].postMessage("finish");
    } else {
      dump("Scope did not match");
    }
  });
}
