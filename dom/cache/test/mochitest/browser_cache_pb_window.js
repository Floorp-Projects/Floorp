var name = 'pb-window-cache';

function testMatch(browser) {
  return ContentTask.spawn(browser, name, function(name) {
    return new Promise((resolve, reject) => {
      content.caches.match('http://foo.com').then(function(response) {
        ok(false, 'caches.match() should not return success');
        reject();
      }).catch(function(err) {
        is('SecurityError', err.name, 'caches.match() should throw SecurityError');
        resolve();
      });
    });
  });
}

function testHas(browser) {
  return ContentTask.spawn(browser, name, function(name) {
    return new Promise(function(resolve, reject) {
      content.caches.has(name).then(function(result) {
        ok(false, 'caches.has() should not return success');
        reject();
      }).catch(function(err) {
        is('SecurityError', err.name, 'caches.has() should throw SecurityError');
        resolve();
      });
    });
  });
}

function testOpen(browser) {
  return ContentTask.spawn(browser, name, function(name) {
    return new Promise(function(resolve, reject) {
      content.caches.open(name).then(function(c) {
        ok(false, 'caches.open() should not return success');
        reject();
      }).catch(function(err) {
        is('SecurityError', err.name, 'caches.open() should throw SecurityError');
        resolve();
      });
    });
  });
}

function testDelete(browser) {
  return ContentTask.spawn(browser, name, function(name) {
    return new Promise(function(resolve, reject) {
      content.caches.delete(name).then(function(result) {
        ok(false, 'caches.delete() should not return success');
        reject();
      }).catch(function(err) {
        is('SecurityError', err.name, 'caches.delete() should throw SecurityError');
        resolve();
      });
    });
  });
}

function testKeys(browser) {
  return ContentTask.spawn(browser, name, function(name) {
    return new Promise(function(resolve, reject) {
      content.caches.keys().then(function(names) {
        ok(false, 'caches.keys() should not return success');
        reject();
      }).catch(function(err) {
        is('SecurityError', err.name, 'caches.keys() should throw SecurityError');
        resolve();
      });
    });
  });
}

function test() {
  let privateWin, privateTab;
  waitForExplicitFinish();
  SpecialPowers.pushPrefEnv({'set': [['dom.caches.enabled', true],
                                     ['dom.caches.testing.enabled', true]]}
  ).then(() => {
    return BrowserTestUtils.openNewBrowserWindow({private: true});
  }).then(pw => {
    privateWin = pw;
    privateTab = pw.gBrowser.addTab("http://example.com/");
    return BrowserTestUtils.browserLoaded(privateTab.linkedBrowser);
  }).then(tab => {
    return Promise.all([
      testMatch(privateTab.linkedBrowser),
      testHas(privateTab.linkedBrowser),
      testOpen(privateTab.linkedBrowser),
      testDelete(privateTab.linkedBrowser),
      testKeys(privateTab.linkedBrowser),
    ]);
  }).then(() => {
    return BrowserTestUtils.closeWindow(privateWin);
  }).then(finish);
}
