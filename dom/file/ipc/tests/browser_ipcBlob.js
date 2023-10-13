/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */

requestLongerTimeout(3);

const BASE_URI = "http://mochi.test:8888/browser/dom/file/ipc/tests/empty.html";

// More than 1mb memory blob childA-parent-childB.
add_task(async function test_CtoPtoC_big() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser1 = gBrowser.getBrowserForTab(tab1);

  let blob = await SpecialPowers.spawn(browser1, [], function () {
    let blob = new Blob([new Array(1024 * 1024).join("123456789ABCDEF")]);
    return blob;
  });

  ok(blob, "CtoPtoC-big: We have a blob!");
  is(
    blob.size,
    new Array(1024 * 1024).join("123456789ABCDEF").length,
    "CtoPtoC-big: The size matches"
  );

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  let status = await SpecialPowers.spawn(browser2, [blob], function (blob) {
    return new Promise(resolve => {
      let fr = new content.FileReader();
      fr.readAsText(blob);
      fr.onloadend = function () {
        resolve(fr.result == new Array(1024 * 1024).join("123456789ABCDEF"));
      };
    });
  });

  ok(status, "CtoPtoC-big: Data match!");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// Less than 1mb memory blob childA-parent-childB.
add_task(async function test_CtoPtoC_small() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser1 = gBrowser.getBrowserForTab(tab1);

  let blob = await SpecialPowers.spawn(browser1, [], function () {
    let blob = new Blob(["hello world!"]);
    return blob;
  });

  ok(blob, "CtoPtoC-small: We have a blob!");
  is(blob.size, "hello world!".length, "CtoPtoC-small: The size matches");

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  let status = await SpecialPowers.spawn(browser2, [blob], function (blob) {
    return new Promise(resolve => {
      let fr = new content.FileReader();
      fr.readAsText(blob);
      fr.onloadend = function () {
        resolve(fr.result == "hello world!");
      };
    });
  });

  ok(status, "CtoPtoC-small: Data match!");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// More than 1mb memory blob childA-parent-childB: BroadcastChannel
add_task(async function test_CtoPtoC_bc_big() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser1 = gBrowser.getBrowserForTab(tab1);

  await SpecialPowers.spawn(browser1, [], function () {
    var bc = new content.BroadcastChannel("test");
    bc.onmessage = function () {
      bc.postMessage(
        new Blob([new Array(1024 * 1024).join("123456789ABCDEF")])
      );
    };
  });

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  let status = await SpecialPowers.spawn(browser2, [], function () {
    return new Promise(resolve => {
      var bc = new content.BroadcastChannel("test");
      bc.onmessage = function (e) {
        let fr = new content.FileReader();
        fr.readAsText(e.data);
        fr.onloadend = function () {
          resolve(fr.result == new Array(1024 * 1024).join("123456789ABCDEF"));
        };
      };

      bc.postMessage("GO!");
    });
  });

  ok(status, "CtoPtoC-broadcastChannel-big: Data match!");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// Less than 1mb memory blob childA-parent-childB: BroadcastChannel
add_task(async function test_CtoPtoC_bc_small() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser1 = gBrowser.getBrowserForTab(tab1);

  await SpecialPowers.spawn(browser1, [], function () {
    var bc = new content.BroadcastChannel("test");
    bc.onmessage = function () {
      bc.postMessage(new Blob(["hello world!"]));
    };
  });

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  let status = await SpecialPowers.spawn(browser2, [], function () {
    return new Promise(resolve => {
      var bc = new content.BroadcastChannel("test");
      bc.onmessage = function (e) {
        let fr = new content.FileReader();
        fr.readAsText(e.data);
        fr.onloadend = function () {
          resolve(fr.result == "hello world!");
        };
      };

      bc.postMessage("GO!");
    });
  });

  ok(status, "CtoPtoC-broadcastChannel-small: Data match!");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// blob URL childA-parent-childB
add_task(async function test_CtoPtoC_bc_small() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser1 = gBrowser.getBrowserForTab(tab1);

  let blobURL = await SpecialPowers.spawn(browser1, [], function () {
    return content.URL.createObjectURL(new content.Blob(["hello world!"]));
  });

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  let status = await SpecialPowers.spawn(
    browser2,
    [blobURL],
    function (blobURL) {
      return new Promise(resolve => {
        var xhr = new content.XMLHttpRequest();
        xhr.open("GET", blobURL);
        xhr.onloadend = function () {
          resolve(xhr.response == "hello world!");
        };

        xhr.send();
      });
    }
  );

  ok(status, "CtoPtoC-blobURL: Data match!");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// Multipart Blob childA-parent-childB.
add_task(async function test_CtoPtoC_multipart() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser1 = gBrowser.getBrowserForTab(tab1);

  let blob = await SpecialPowers.spawn(browser1, [], function () {
    return new Blob(["!"]);
  });

  ok(blob, "CtoPtoC-multipart: We have a blob!");
  is(blob.size, "!".length, "CtoPtoC-multipart: The size matches");

  let newBlob = new Blob(["world", blob]);

  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser2 = gBrowser.getBrowserForTab(tab2);

  let status = await SpecialPowers.spawn(browser2, [newBlob], function (blob) {
    return new Promise(resolve => {
      let fr = new content.FileReader();
      fr.readAsText(new Blob(["hello ", blob]));
      fr.onloadend = function () {
        resolve(fr.result == "hello world!");
      };
    });
  });

  ok(status, "CtoPtoC-multipart: Data match!");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});

// Multipart Blob childA-parent with a max size
add_task(async function test_CtoPsize_multipart() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, BASE_URI);
  let browser = gBrowser.getBrowserForTab(tab);

  let blob = await SpecialPowers.spawn(browser, [], function () {
    let data = new Array(1024 * 512).join("A");
    let blob1 = new Blob([data]);
    let blob2 = new Blob([data]);
    let blob3 = new Blob([data]);

    return new Blob([blob1, blob2, blob3]);
  });

  ok(blob, "CtoPsize-multipart: We have a blob!");
  is(
    blob.size,
    new Array(1024 * 512).join("A").length * 3,
    "CtoPsize-multipart: The size matches"
  );

  BrowserTestUtils.removeTab(tab);
});
