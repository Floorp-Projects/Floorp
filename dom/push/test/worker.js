// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

this.onpush = handlePush;

function handlePush(event) {

  self.clients.matchAll().then(function(result) {
    // FIXME(nsm): Bug 1149195 will fix data exposure.
    if (event instanceof PushEvent && !('data' in event)) {
      result[0].postMessage({type: "finished", okay: "yes"});
      return;
    }
    result[0].postMessage({type: "finished", okay: "no"});
  });
}
