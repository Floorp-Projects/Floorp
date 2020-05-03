var EXPORTED_SYMBOLS = ["ReftestFissionChild"];

class ReftestFissionChild extends JSWindowActorChild {

  forwardAfterPaintEventToParent(rects, originalTargetUri, dispatchToSelfAsWell) {
    if (dispatchToSelfAsWell) {
      let event = new this.contentWindow.CustomEvent("Reftest:MozAfterPaintFromChild",
        {bubbles: true, detail: {rects, originalTargetUri}});
      this.contentWindow.dispatchEvent(event);
    }

    let parentContext = this.browsingContext.parent;
    if (parentContext) {
      try {
        this.sendAsyncMessage("ForwardAfterPaintEvent",
          {toBrowsingContext: parentContext, fromBrowsingContext: this.browsingContext,
           rects, originalTargetUri});
      } catch (e) {
        // |this| can be destroyed here and unable to send messages, which is
        // not a problem, the reftest harness probably torn down the page and
        // moved on to the next test.
        Cu.reportError(e);
      }
    }
  }

  handleEvent(evt) {
    switch (evt.type) {
      case "MozAfterPaint":
        // We want to forward any after paint events to our parent document so that
        // that it reaches the root content document where the main reftest harness
        // code (reftest-content.js) will process it and update the canvas.
        var rects = [];
        for (let r of evt.clientRects) {
            rects.push({ left: r.left, top: r.top, right: r.right, bottom: r.bottom });
        }
        this.forwardAfterPaintEventToParent(rects, this.document.documentURI, /* dispatchToSelfAsWell */ false);
        break;
    }
  }

  transformRect(transform, rect) {
    let p1 = transform.transformPoint({x: rect.left, y: rect.top});
    let p2 = transform.transformPoint({x: rect.right, y: rect.top});
    let p3 = transform.transformPoint({x: rect.left, y: rect.bottom});
    let p4 = transform.transformPoint({x: rect.right, y: rect.bottom});
    let quad = new DOMQuad(p1, p2, p3, p4);
    return quad.getBounds();
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "ForwardAfterPaintEventToSelfAndParent":
      {
        // The embedderElement can be null if the child we got this from was removed.
        // Not much we can do to transform the rects, but it doesn't matter, the rects
        // won't reach reftest-content.js.
        if (msg.data.fromBrowsingContext.embedderElement == null) {
          this.forwardAfterPaintEventToParent(msg.data.rects, msg.data.originalTargetUri,
            /* dispatchToSelfAsWell */ true);
          return;
        }

        // Transform the rects from fromBrowsingContext to us.
        // We first translate from the content rect to the border rect of the iframe.
        let style = this.contentWindow.getComputedStyle(msg.data.fromBrowsingContext.embedderElement);
        let translate = new DOMMatrixReadOnly().translate(
          parseFloat(style.paddingLeft) + parseFloat(style.borderLeftWidth),
          parseFloat(style.paddingTop) + parseFloat(style.borderTopWidth));

        // Then we transform from the iframe to our root frame.
        // We are guaranteed to be the process with the embedderElement for fromBrowsingContext.
        let transform = msg.data.fromBrowsingContext.embedderElement.getTransformToViewport();
        let combined = translate.multiply(transform);

        let newrects = msg.data.rects.map(r => this.transformRect(combined, r))

        this.forwardAfterPaintEventToParent(newrects, msg.data.originalTargetUri, /* dispatchToSelfAsWell */ true);
        break;
      }

      case "EmptyMessage":
        return undefined;
      case "UpdateLayerTree":
      {
        let errorStrings = [];
        try {
          if (this.manager.isProcessRoot) {
            this.contentWindow.windowUtils.updateLayerTree();
          }
        } catch (e) {
          errorStrings.push("updateLayerTree failed: " + e);
        }
        return Promise.resolve({errorStrings});
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
