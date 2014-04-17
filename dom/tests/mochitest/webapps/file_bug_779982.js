SimpleTest.waitForExplicitFinish();
SpecialPowers.setAllAppsLaunchable(true);

var fileTestOnCurrentOrigin = 'http://example.org/tests/dom/tests/mochitest/webapps/file_bug_779982.html';

var gData = [
  // APP 1
  {
    app: 'http://example.org/manifest.webapp',
    action: 'getSelf',
    isnull: false,
    src: fileTestOnCurrentOrigin,
    message: 'getSelf() for app should return something'
  },
  {
    app: 'http://example.org/manifest.webapp',
    action: 'checkInstalled',
    isnull: false,
    src: fileTestOnCurrentOrigin,
    message: 'checkInstalled() for app should return true'
  },
  {
    app: 'http://example.org/manifest.webapp',
    action: 'checkInstalledWrong',
    isnull: true,
    src: fileTestOnCurrentOrigin,
    message: 'checkInstalled() for browser should return true'
  },
  // Browser
  {
    browser: true,
    action: 'getSelf',
    isnull: true,
    src: fileTestOnCurrentOrigin,
    message: 'getSelf() for browser should return null'
  },
  {
    browser: true,
    action: 'checkInstalled',
    isnull: false,
    src: fileTestOnCurrentOrigin,
    message: 'checkInstalled() for browser should return true'
  },
  {
    browser: true,
    action: 'checkInstalledWrong',
    isnull: true,
    src: fileTestOnCurrentOrigin,
    message: 'checkInstalled() for browser should return true'
  },
];

function runTest() {
  for (var i in gData) {
    var iframe = document.createElement('iframe');
    var data = gData[i];

    if (data.app) {
      iframe.setAttribute('mozbrowser', '');
      iframe.setAttribute('mozapp', data.app);
    } else if (data.browser) {
      iframe.setAttribute('mozbrowser', '');
    }

    if (data.app || data.browser) {
      iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
        is(e.detail.message, 'success', data.message);

        i++;
        if (i >= gData.length) {
          SimpleTest.finish();
        } else {
          gTestRunner.next();
        }
      });
    }

    iframe.src = data.src + '?' + data.action + '&' + data.isnull;
    document.getElementById('content').appendChild(iframe);

    yield undefined;
  }
}

var gTestRunner = runTest();

SpecialPowers.pushPrefEnv({"set": [["dom.mozBrowserFramesEnabled", true]]}, function() {
  SpecialPowers.pushPermissions([{'type': 'browser', 'allow': true, 'context': document}, {'type': 'embed-apps', 'allow': true, 'context': document}], function() {gTestRunner.next() });
});
