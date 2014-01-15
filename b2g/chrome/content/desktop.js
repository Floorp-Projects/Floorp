
window.addEventListener("ContentStart", function(evt) {
  // Enable touch event shim on desktop that translates mouse events
  // into touch ones
  let require = Cu.import("resource://gre/modules/devtools/Loader.jsm", {})
                  .devtools.require;
  let { TouchEventHandler } = require("devtools/touch-events");
  let touchEventHandler = new TouchEventHandler(shell.contentBrowser);
  touchEventHandler.start();
});
