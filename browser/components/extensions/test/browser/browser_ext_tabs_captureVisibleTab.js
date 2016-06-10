/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function* runTest(options) {
  options.neutral = [0xaa, 0xaa, 0xaa];

  let html = `
    <!DOCTYPE html>
    <html lang="en">
    <head><meta charset="UTF-8"></head>
    <body style="background-color: rgb(${options.color})">
      <!-- Fill most of the image with a neutral color to test edge-to-edge scaling. -->
      <div style="position: absolute;
                  left: 2px;
                  right: 2px;
                  top: 2px;
                  bottom: 2px;
                  background: rgb(${options.neutral});"></div>
    </body>
    </html>
  `;

  let url = `data:text/html,${encodeURIComponent(html)}`;
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, url, true);

  tab.linkedBrowser.fullZoom = options.fullZoom;

  function background(options) {
    // Wrap API methods in promise-based variants.
    let promiseTabs = {};
    Object.keys(browser.tabs).forEach(method => {
      promiseTabs[method] = (...args) => {
        return new Promise(resolve => {
          browser.tabs[method](...args, resolve);
        });
      };
    });

    browser.test.log(`Test color ${options.color} at fullZoom=${options.fullZoom}`);

    promiseTabs.query({currentWindow: true, active: true}).then(([tab]) => {
      return Promise.all([
        promiseTabs.captureVisibleTab(tab.windowId, {format: "jpeg", quality: 95}),
        promiseTabs.captureVisibleTab(tab.windowId, {format: "png", quality: 95}),
        promiseTabs.captureVisibleTab(tab.windowId, {quality: 95}),
        promiseTabs.captureVisibleTab(tab.windowId),
      ]).then(([jpeg, png, ...pngs]) => {
        browser.test.assertTrue(pngs.every(url => url == png), "All PNGs are identical");

        browser.test.assertTrue(jpeg.startsWith("data:image/jpeg;base64,"), "jpeg is JPEG");
        browser.test.assertTrue(png.startsWith("data:image/png;base64,"), "png is PNG");

        let promises = [jpeg, png].map(url => new Promise(resolve => {
          let img = new Image();
          img.src = url;
          img.onload = () => resolve(img);
        }));
        return Promise.all(promises);
      }).then(([jpeg, png]) => {
        let tabDims = `${tab.width}\u00d7${tab.height}`;

        let images = {jpeg, png};
        for (let format of Object.keys(images)) {
          let img = images[format];

          let dims = `${img.width}\u00d7${img.height}`;
          browser.test.assertEq(tabDims, dims, `${format} dimensions are correct`);

          let canvas = document.createElement("canvas");
          canvas.width = img.width;
          canvas.height = img.height;
          canvas.mozOpaque = true;

          let ctx = canvas.getContext("2d");
          ctx.drawImage(img, 0, 0);

          // Check the colors of the first and last pixels of the image, to make
          // sure we capture the entire frame, and scale it correctly.
          let coords = [
            {x: 0, y: 0,
             color: options.color},
            {x: img.width - 1,
             y: img.height - 1,
             color: options.color},
            {x: img.width / 2 | 0,
             y: img.height / 2 | 0,
             color: options.neutral},
          ];

          for (let {x, y, color} of coords) {
            let imageData = ctx.getImageData(x, y, 1, 1).data;

            if (format == "png") {
              browser.test.assertEq(`rgba(${color},255)`, `rgba(${[...imageData]})`, `${format} image color is correct at (${x}, ${y})`);
            } else {
              // Allow for some deviation in JPEG version due to lossy compression.
              const SLOP = 2;

              browser.test.log(`Testing ${format} image color at (${x}, ${y}), have rgba(${[...imageData]}), expecting approx. rgba(${color},255)`);

              browser.test.assertTrue(Math.abs(color[0] - imageData[0]) <= SLOP, `${format} image color.red is correct at (${x}, ${y})`);
              browser.test.assertTrue(Math.abs(color[1] - imageData[1]) <= SLOP, `${format} image color.green is correct at (${x}, ${y})`);
              browser.test.assertTrue(Math.abs(color[2] - imageData[2]) <= SLOP, `${format} image color.blue is correct at (${x}, ${y})`);
              browser.test.assertEq(255, imageData[3], `${format} image color.alpha is correct at (${x}, ${y})`);
            }
          }
        }

        browser.test.notifyPass("captureVisibleTab");
      });
    }).catch(e => {
      browser.test.fail(`Error: ${e} :: ${e.stack}`);
      browser.test.notifyFail("captureVisibleTab");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["<all_urls>"],
    },

    background: `(${background})(${JSON.stringify(options)})`,
  });

  yield extension.startup();

  yield extension.awaitFinish("captureVisibleTab");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab);
}

add_task(function* testCaptureVisibleTab() {
  yield runTest({color: [0, 0, 0], fullZoom: 1});

  yield runTest({color: [0, 0, 0], fullZoom: 2});

  yield runTest({color: [0, 0, 0], fullZoom: 0.5});

  yield runTest({color: [255, 255, 255], fullZoom: 1});
});

add_task(function* testCaptureVisibleTabPermissions() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background() {
      browser.test.assertFalse("captureVisibleTab" in browser.tabs,
                               'Extension without "<all_tabs>" permission should not have access to captureVisibleTab');
      browser.test.notifyPass("captureVisibleTabPermissions");
    },
  });

  yield extension.startup();

  yield extension.awaitFinish("captureVisibleTabPermissions");

  yield extension.unload();
});
