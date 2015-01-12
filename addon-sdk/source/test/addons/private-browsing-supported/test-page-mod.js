const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { PageMod } = require("sdk/page-mod");
const { getActiveTab, setTabURL, openTab, closeTab } = require('sdk/tabs/utils');
const xulApp = require('sdk/system/xul-app');
const windowHelpers = require('sdk/window/helpers');
const { defer } = require("sdk/core/promise");
const { isPrivate } = require('sdk/private-browsing');
const { isTabPBSupported, isWindowPBSupported } = require('sdk/private-browsing/utils');

function openWebpage(url, enablePrivate) {
  let { promise, resolve, reject } = defer();

  if (xulApp.is("Fennec")) {
    let chromeWindow = getMostRecentBrowserWindow();
    let rawTab = openTab(chromeWindow, url, {
      isPrivate: enablePrivate
    });

    resolve(function() {
      closeTab(rawTab)
    });

    return promise;
  }
  else {
    windowHelpers.open("", {
      features: {
        toolbar: true,
        private: enablePrivate
      }
    }).then(function(chromeWindow) {
      if (isPrivate(chromeWindow) !== !!enablePrivate)
        reject(new Error("Window should have Private set to " + !!enablePrivate));

      let tab = getActiveTab(chromeWindow);
      setTabURL(tab, url);

      resolve(function(){
        windowHelpers.close(chromeWindow);
      });
    });

    return promise;
  }
}

exports["test page-mod on private tab"] = function (assert, done) {
  // Only set private mode when explicitely supported.
  // (fennec 19 has some intermediate PB support where isTabPBSupported
  // will be false, but isPrivate(worker.tab) will be true if we open a private
  // tab)
  let setPrivate = isTabPBSupported || isWindowPBSupported;

  let id = Date.now().toString(36);
  let frameUri = "data:text/html;charset=utf-8," + id;
  let uri = "data:text/html;charset=utf-8," +
            encodeURIComponent(id + "<iframe src='" + frameUri + "'><iframe>");

  let count = 0;

  openWebpage(uri, setPrivate).then(function(close) {
    PageMod({
      include: [uri, frameUri],

      onAttach: function(worker) {
        assert.ok(worker.tab.url == uri || worker.tab.url == frameUri,
                  "Got a worker attached to the private window tab");

        if (setPrivate) {
          assert.ok(isPrivate(worker), "The worker is really private");
          assert.ok(isPrivate(worker.tab), "The document is really private");
        }
        else {
          assert.ok(!isPrivate(worker),
                    "private browsing isn't supported, " +
                    "so that the worker isn't private");
          assert.ok(!isPrivate(worker.tab),
                    "private browsing isn't supported, " +
                    "so that the document isn't private");
        }

        if (++count == 2) {
          this.destroy();
          close();
          done();
        }
      }
    });
  }).then(null, assert.fail);
};

exports["test page-mod on non-private tab"] = function (assert, done) {
  let id = Date.now().toString(36);
  let url = "data:text/html;charset=utf-8," + encodeURIComponent(id);

  openWebpage(url, false).then(function(close){
    PageMod({
      include: url,
      onAttach: function(worker) {
        assert.equal(worker.tab.url, url,
                     "Got a worker attached to the private window tab");
        assert.ok(!isPrivate(worker), "The worker is really non-private");
        assert.ok(!isPrivate(worker.tab), "The document is really non-private");

        this.destroy();
        close();
        done();
      }
    });
  }).then(null, assert.fail);
}
