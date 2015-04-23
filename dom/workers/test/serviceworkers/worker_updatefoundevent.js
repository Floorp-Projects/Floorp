/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

onactivate = function(e) {
  e.waitUntil(new Promise(function(resolve, reject) {
    registration.onupdatefound = function(e) {
      clients.matchAll().then(function(clients) {
        if (!clients.length) {
          reject("No clients found");
        }

        if (registration.scope.match(/updatefoundevent\.html$/)) {
          clients[0].postMessage("finish");
          resolve();
        } else {
          dump("Scope did not match");
        }
      }, reject);
    }
  }));
}
