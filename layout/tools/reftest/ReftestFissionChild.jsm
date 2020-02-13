var EXPORTED_SYMBOLS = ["ReftestFissionChild"];

class ReftestFissionChild extends JSWindowActorChild {

  receiveMessage(msg) {
    switch (msg.name) {
      case "UpdateLayerTree":
      {
        let errorString = null;
        try {
          if (this.manager.isProcessRoot) {
            this.contentWindow.windowUtils.updateLayerTree();
          }
        } catch (e) {
          errorString = "updateLayerTree failed: " + e;
        }
        return Promise.resolve({errorString});
      }
      case "FlushRendering":
      {
        let errorStrings = [];
        let warningStrings = [];
        let infoStrings = [];

        try {
          let ignoreThrottledAnimations = msg.data.ignoreThrottledAnimations;

          if (this.manager.isProcessRoot) {
            var anyPendingPaintsGeneratedInDescendants = false;

            function flushWindow(win) {
              var utils = win.windowUtils;
              var afterPaintWasPending = utils.isMozAfterPaintPending;

              var root = win.document.documentElement;
              if (root && !root.classList.contains("reftest-no-flush")) {
                try {
                  if (ignoreThrottledAnimations) {
                    utils.flushLayoutWithoutThrottledAnimations();
                  } else {
                    root.getBoundingClientRect();
                  }
                } catch (e) {
                  warningStrings.push("flushWindow failed: " + e + "\n");
                }
              }

              if (!afterPaintWasPending && utils.isMozAfterPaintPending) {
                infoStrings.push("FlushRendering generated paint for window " + win.location.href);
                anyPendingPaintsGeneratedInDescendants = true;
              }

              for (let i = 0; i < win.frames.length; ++i) {
                try {
                  if (!Cu.isRemoteProxy(win.frames[i])) {
                    flushWindow(win.frames[i]);
                  }
                } catch (e) {
                  Cu.reportError(e);
                }
              }
            }

            flushWindow(this.contentWindow);

            if (anyPendingPaintsGeneratedInDescendants &&
                !this.contentWindow.windowUtils.isMozAfterPaintPending) {
              warningStrings.push("Internal error: descendant frame generated a MozAfterPaint event, but the root document doesn't have one!");
            }

          }
        } catch (e) {
          errorStrings.push("flushWindow failed: " + e);
        }
        return Promise.resolve({errorStrings, warningStrings, infoStrings});
      }
    }
  }
}
