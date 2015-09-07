add_task(function* () {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {},
      "background": {
        "page": "background.html",
      }
    },

    files: {
      "background.html": `<canvas id="canvas" width="2" height="2">
        <script src="background.js"></script>`,

      "background.js": function() {
        var canvas = document.getElementById("canvas");
        var canvasContext = canvas.getContext("2d");

        canvasContext.clearRect(0, 0, canvas.width, canvas.height);
        canvasContext.fillStyle = "green";
        canvasContext.fillRect(0, 0, 1, 1);

        var url = canvas.toDataURL("image/png");
        var imageData = canvasContext.getImageData(0, 0, canvas.width, canvas.height);
        browser.browserAction.setIcon({imageData});

        browser.test.sendMessage("imageURL", url);
      }
    },
  });

  let [_, url] = yield Promise.all([extension.startup(), extension.awaitMessage("imageURL")]);

  let widgetId = makeWidgetId(extension.id) + "-browser-action";
  let node = CustomizableUI.getWidget(widgetId).forWindow(window).node;

  let image = node.getAttribute("image");
  is(image, url, "image is correct");

  yield extension.unload();
});
