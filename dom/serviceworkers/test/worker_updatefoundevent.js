/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

registration.onupdatefound = function (e) {
  clients.matchAll().then(function (clients) {
    if (!clients.length) {
      // We don't control any clients when the first update event is fired
      // because we haven't reached the 'activated' state.
      return;
    }

    if (registration.scope.match(/updatefoundevent\.html$/)) {
      clients[0].postMessage("finish");
    } else {
      dump("Scope did not match");
    }
  });
};
