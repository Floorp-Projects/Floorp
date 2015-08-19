function specialPowersLock(orientation) {
  return new Promise(function(resolve, reject) {
    SpecialPowers.pushPrefEnv({
      'set': [ ["dom.screenorientation.testing.non_fullscreen_lock_allow", true] ]
    }, function() {
      var p = screen.orientation.lock(orientation);
      resolve(p);
    });
  });
}

function specialPowersUnlock() {
  return new Promise(function(resolve, reject) {
    SpecialPowers.pushPrefEnv({
      'set': [ ["dom.screenorientation.testing.non_fullscreen_lock_allow", true] ]
    }, function() {
      screen.orientation.unlock();
      resolve();
    });
  });
}
