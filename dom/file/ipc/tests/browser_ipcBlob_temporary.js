/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */

requestLongerTimeout(3);

const BASE_URI = "http://mochi.test:8888/browser/dom/file/ipc/tests/empty.html";

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.blob.memoryToTemporaryFile", 1], ["dom.ipc.processCount", 4]],
  });

  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser1 = gBrowser.getBrowserForTab(tab1);

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  await ContentTask.spawn(browser2, null, function() {
    content.window.testPromise = new content.window.Promise(resolve => {
      let bc = new content.window.BroadcastChannel("foobar");
      bc.onmessage = e => {
        function realTest() {
          return new content.window.Promise(resolve => {
            let count = 10;
            for (let i = 0; i < count; ++i) {
              info("FileReader at the same time: " + i);
              let fr = new content.window.FileReader();
              fr.readAsText(e.data);
              fr.onerror = () => {
                ok(false, "Something wrong happened.");
              };

              fr.onloadend = () => {
                is(fr.result.length, e.data.size, "FileReader worked fine.");
                if (!--count) {
                  resolve(true);
                }
              };
            }
          });
        }

        let promises = [];
        for (let i = 0; i < 5; ++i) {
          promises.push(realTest());
        }

        Promise.all(promises).then(() => {
          resolve(true);
        });
      };
    });
  });

  let status = await ContentTask.spawn(browser1, null, function() {
    let p = new content.window.Promise(resolve => {
      let xhr = new content.window.XMLHttpRequest();
      xhr.open("GET", "temporary.sjs", true);
      xhr.responseType = "blob";
      xhr.onload = () => {
        resolve(xhr.response);
      };
      xhr.send();
    });

    return p.then(blob => {
      function realTest() {
        return new content.window.Promise(resolve => {
          info("Let's broadcast the blob...");
          let bc = new content.window.BroadcastChannel("foobar");
          bc.postMessage(blob);

          info("Here the test...");
          let count = 10;
          for (let i = 0; i < count; ++i) {
            info("FileReader at the same time: " + i);
            let fr = new content.window.FileReader();
            fr.readAsText(blob);
            fr.onerror = () => {
              ok(false, "Something wrong happened.");
            };

            fr.onloadend = () => {
              is(fr.result.length, blob.size, "FileReader worked fine.");
              if (!--count) {
                resolve(true);
              }
            };
          }
        });
      }

      let promises = [];
      for (let i = 0; i < 5; ++i) {
        promises.push(realTest());
      }

      return Promise.all(promises);
    });
  });

  ok(status, "All good for tab1!");

  status = await ContentTask.spawn(browser2, null, function() {
    return content.window.testPromise;
  });

  ok(status, "All good for tab2!");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
