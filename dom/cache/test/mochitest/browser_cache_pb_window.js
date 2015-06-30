var name = 'pb-window-cache';

function testMatch(win) {
  return new Promise(function(resolve, reject) {
    win.caches.match('http://foo.com').then(function(response) {
      ok(false, 'caches.match() should not return success');
      reject();
    }).catch(function(err) {
      is('SecurityError', err.name, 'caches.match() should throw SecurityError');
      resolve();
    });
  });
}

function testHas(win) {
  return new Promise(function(resolve, reject) {
    win.caches.has(name).then(function(result) {
      ok(false, 'caches.has() should not return success');
      reject();
    }).catch(function(err) {
      is('SecurityError', err.name, 'caches.has() should throw SecurityError');
      resolve();
    });
  });
}

function testOpen(win) {
  return new Promise(function(resolve, reject) {
    win.caches.open(name).then(function(c) {
      ok(false, 'caches.open() should not return success');
      reject();
    }).catch(function(err) {
      is('SecurityError', err.name, 'caches.open() should throw SecurityError');
      resolve();
    });
  });
}

function testDelete(win) {
  return new Promise(function(resolve, reject) {
    win.caches.delete(name).then(function(result) {
      ok(false, 'caches.delete() should not return success');
      reject();
    }).catch(function(err) {
      is('SecurityError', err.name, 'caches.delete() should throw SecurityError');
      resolve();
    });
  });
}

function testKeys(win) {
  return new Promise(function(resolve, reject) {
    win.caches.keys().then(function(names) {
      ok(false, 'caches.keys() should not return success');
      reject();
    }).catch(function(err) {
      is('SecurityError', err.name, 'caches.keys() should throw SecurityError');
      resolve();
    });
  });
}

function test() {
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({'set': [['dom.caches.enabled', true],
                                     ['dom.caches.testing.enabled', true]]},
                            function() {
    var privateWin = OpenBrowserWindow({private: true});
    privateWin.addEventListener('load', function() {
      Promise.all([
        testMatch(privateWin),
        testHas(privateWin),
        testOpen(privateWin),
        testDelete(privateWin),
        testKeys(privateWin)
      ]).then(function() {
        privateWin.close();
        finish();
      });
    });
  });
}
