SimpleTest.waitForExplicitFinish();

var fileTestOnCurrentOrigin = (location.protocol + '//' + location.host + location.pathname)
                              .replace('test_', 'file_')
                              .replace('_inproc', '').replace('_oop', '');

var previousPrefs = {
  mozBrowserFramesEnabled: undefined,
  oop_by_default: undefined,
};

try {
  previousPrefs.mozBrowserFramesEnabled = SpecialPowers.getBoolPref('dom.mozBrowserFramesEnabled');
} catch(e)
{
}

try {
  previousPrefs.oop_by_default = SpecialPowers.getBoolPref('dom.ipc.browser_frames.oop_by_default');
} catch(e) {
}

SpecialPowers.setBoolPref('dom.mozBrowserFramesEnabled', true);
SpecialPowers.setBoolPref("dom.ipc.browser_frames.oop_by_default", location.pathname.indexOf('_inproc') == -1);

SpecialPowers.addPermission("browser", true, window.document);

var gData = [
  // APP 1
  {
    app: 'http://example.org/manifest.webapp',
    action: 'read-no',
    src: fileTestOnCurrentOrigin,
  },
  {
    app: 'http://example.org/manifest.webapp',
    action: 'write',
    src: fileTestOnCurrentOrigin,
  },
  {
    app: 'http://example.org/manifest.webapp',
    action: 'read-yes',
    src: fileTestOnCurrentOrigin,
  },
  // APP 2
  {
    app: 'https://example.com/manifest.webapp',
    action: 'read-no',
    src: fileTestOnCurrentOrigin,
  },
  {
    app: 'https://example.com/manifest.webapp',
    action: 'write',
    src: fileTestOnCurrentOrigin,
  },
  {
    app: 'https://example.com/manifest.webapp',
    action: 'read-yes',
    src: fileTestOnCurrentOrigin,
  },
  // Browser
  {
    browser: true,
    action: 'read-no',
    src: fileTestOnCurrentOrigin,
  },
  {
    browser: true,
    action: 'write',
    src: fileTestOnCurrentOrigin,
  },
  {
    browser: true,
    action: 'read-yes',
    src: fileTestOnCurrentOrigin,
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
        is(e.detail.message, 'success', 'test number ' + i);

//        document.getElementById('content').removeChild(iframe);

        i++;
        if (i >= gData.length) {
          if (previousPrefs.mozBrowserFramesEnabled !== undefined) {
            SpecialPowers.setBoolPref('dom.mozBrowserFramesEnabled', previousPrefs.mozBrowserFramesEnabled);
          }
          if (previousPrefs.oop_by_default !== undefined) {
            SpecialPowers.setBoolPref("dom.ipc.browser_frames.oop_by_default", previousPrefs.oop_by_default);
          }

          SpecialPowers.removePermission("browser", window.document);

          indexedDB.deleteDatabase('AppIsolationTest').onsuccess = function() {
            SimpleTest.finish();
          };
        } else {
          gTestRunner.next();
        }
      });
    }

    iframe.src = data.src + '?' + data.action;

    document.getElementById('content').appendChild(iframe);

    yield undefined;
  }
}

var gTestRunner = runTest();

function startTest() {
  var request = window.indexedDB.open('AppIsolationTest');
  var created = false;

  request.onupgradeneeded = function(event) {
    created = true;
    var db = event.target.result;
    var data = [
      { id: "0", name: "foo" },
    ];
    var objectStore = db.createObjectStore("test", { keyPath: "id" });
    for (var i in data) {
      objectStore.add(data[i]);
    }
  }

  request.onsuccess = function(event) {
    var db = event.target.result;
    is(created, true, "we should have created the db");

    db.transaction("test").objectStore("test").get("0").onsuccess = function(event) {
      is(event.target.result.name, 'foo', 'data have been written');
      db.close();

      gTestRunner.next();
    };
  }
}

// TODO: remove unsetting network.disable.ipc.security as part of bug 820712
SpecialPowers.pushPrefEnv({
  "set": [
    ["network.disable.ipc.security", true],
  ]
}, startTest);
