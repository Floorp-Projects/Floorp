// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

addEventListener("push", function(event) {

  self.clients.matchAll().then(function(result) {
    if (event instanceof PushEvent &&
      event.data instanceof PushMessageData &&
      event.data.text().length > 0) {

      result[0].postMessage({type: "finished", okay: "yes"});
      return;
    }
    result[0].postMessage({type: "finished", okay: "no"});
  });
});
