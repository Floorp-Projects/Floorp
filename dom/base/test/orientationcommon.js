function specialPowersLock(orientation) {
  return SpecialPowers.pushPrefEnv({
    'set': [ ["dom.screenorientation.testing.non_fullscreen_lock_allow", true] ]
  }).then(function() => {
    var p = screen.orientation.lock(orientation);
  });
}

function specialPowersUnlock() {
  return SpecialPowers.pushPrefEnv({
    'set': [ ["dom.screenorientation.testing.non_fullscreen_lock_allow", true] ]
  }).then(function() {
    screen.orientation.unlock();
    resolve();
  });
}
