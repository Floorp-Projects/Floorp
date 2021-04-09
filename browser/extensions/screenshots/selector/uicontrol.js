/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals log, catcher, util, ui, slides, global */
/* globals shooter, callBackground, selectorLoader, assertIsTrusted, buildSettings, selection */

"use strict";

this.uicontrol = (function() {
  const exports = {};

  /** ********************************************************
   * selection
   */

  /* States:

  "crosshairs":
    Nothing has happened, and the crosshairs will follow the movement of the mouse
  "draggingReady":
    The user has pressed the mouse button, but hasn't moved enough to create a selection
  "dragging":
    The user has pressed down a mouse button, and is dragging out an area far enough to show a selection
  "selected":
    The user has selected an area
  "resizing":
    The user is resizing the selection
  "cancelled":
    Everything has been cancelled
  "previewing":
    The user is previewing the full-screen/visible image

  A mousedown goes from crosshairs to dragging.
  A mouseup goes from dragging to selected
  A click outside of the selection goes from selected to crosshairs
  A click on one of the draggers goes from selected to resizing

  State variables:

  state (string, one of the above)
  mousedownPos (object with x/y during draggingReady, shows where the selection started)
  selectedPos (object with x/y/h/w during selected or dragging, gives the entire selection)
  resizeDirection (string: top, topLeft, etc, during resizing)
  resizeStartPos (x/y position where resizing started)
  mouseupNoAutoselect (true if a mouseup in draggingReady should not trigger autoselect)

  */

  const { watchFunction, watchPromise } = catcher;

  const MAX_PAGE_HEIGHT = buildSettings.maxImageHeight;
  const MAX_PAGE_WIDTH = buildSettings.maxImageWidth;
  // An autoselection smaller than these will be ignored entirely:
  const MIN_DETECT_ABSOLUTE_HEIGHT = 10;
  const MIN_DETECT_ABSOLUTE_WIDTH = 30;
  // An autoselection smaller than these will not be preferred:
  const MIN_DETECT_HEIGHT = 30;
  const MIN_DETECT_WIDTH = 100;
  // An autoselection bigger than either of these will be ignored:
  const MAX_DETECT_HEIGHT = Math.max(window.innerHeight + 100, 700);
  const MAX_DETECT_WIDTH = Math.max(window.innerWidth + 100, 1000);
  // This is how close (in pixels) you can get to the edge of the window and then
  // it will scroll:
  const SCROLL_BY_EDGE = 20;
  // This is how wide the inboard scrollbars are, generally 0 except on Mac
  const SCROLLBAR_WIDTH = window.navigator.platform.match(/Mac/i) ? 17 : 0;

  const { Selection } = selection;
  const { sendEvent } = shooter;
  const log = global.log;

  function round10(n) {
    return Math.floor(n / 10) * 10;
  }

  function eventOptionsForBox(box) {
    return {
      cd1: round10(Math.abs(box.bottom - box.top)),
      cd2: round10(Math.abs(box.right - box.left)),
    };
  }

  function eventOptionsForResize(boxStart, boxEnd) {
    return {
      cd1: round10(
        boxEnd.bottom - boxEnd.top - (boxStart.bottom - boxStart.top)
      ),
      cd2: round10(
        boxEnd.right - boxEnd.left - (boxStart.right - boxStart.left)
      ),
    };
  }

  function eventOptionsForMove(posStart, posEnd) {
    return {
      cd1: round10(posEnd.y - posStart.y),
      cd2: round10(posEnd.x - posStart.x),
    };
  }

  function downloadShot() {
    const previewDataUrl = captureType === "fullPageTruncated" ? null : dataUrl;
    // Downloaded shots don't have dimension limits
    removeDimensionLimitsOnFullPageShot();
    shooter.downloadShot(selectedPos, previewDataUrl, captureType);
  }

  function copyShot() {
    const previewDataUrl = captureType === "fullPageTruncated" ? null : dataUrl;
    // Copied shots don't have dimension limits
    removeDimensionLimitsOnFullPageShot();
    shooter.copyShot(selectedPos, previewDataUrl, captureType);
  }

  /** *********************************************
   * State and stateHandlers infrastructure
   */

  // This enumerates all the anchors on the selection, and what part of the
  // selection they move:
  const movements = {
    topLeft: ["x1", "y1"],
    top: [null, "y1"],
    topRight: ["x2", "y1"],
    left: ["x1", null],
    right: ["x2", null],
    bottomLeft: ["x1", "y2"],
    bottom: [null, "y2"],
    bottomRight: ["x2", "y2"],
    move: ["*", "*"],
  };

  const doNotAutoselectTags = {
    H1: true,
    H2: true,
    H3: true,
    H4: true,
    H5: true,
    H6: true,
  };

  let captureType;

  function removeDimensionLimitsOnFullPageShot() {
    if (captureType === "fullPageTruncated") {
      captureType = "fullPage";
      selectedPos = new Selection(
        0,
        0,
        getDocumentWidth(),
        getDocumentHeight()
      );
    }
  }

  const standardDisplayCallbacks = {
    cancel: () => {
      sendEvent("cancel-shot", "overlay-cancel-button");
      exports.deactivate();
    },
    download: () => {
      sendEvent("download-shot", "overlay-download-button");
      downloadShot();
    },
    copy: () => {
      sendEvent("copy-shot", "overlay-copy-button");
      copyShot();
    },
  };

  const standardOverlayCallbacks = {
    cancel: () => {
      sendEvent("cancel-shot", "cancel-preview-button");
      exports.deactivate();
    },
    onClickCancel: e => {
      sendEvent("cancel-shot", "cancel-selection-button");
      e.preventDefault();
      e.stopPropagation();
      exports.deactivate();
    },
    onOpenMyShots: () => {
      sendEvent("goto-myshots", "selection-button");
      callBackground("openMyShots")
        .then(() => exports.deactivate())
        .catch(() => {
          // Handled in communication.js
        });
    },
    onClickVisible: () => {
      sendEvent("capture-visible", "selection-button");
      selectedPos = new Selection(
        window.scrollX,
        window.scrollY,
        window.scrollX + document.documentElement.clientWidth,
        window.scrollY + window.innerHeight
      );
      captureType = "visible";
      setState("previewing");
    },
    onClickFullPage: () => {
      sendEvent("capture-full-page", "selection-button");
      captureType = "fullPage";
      const width = getDocumentWidth();
      if (width > MAX_PAGE_WIDTH) {
        captureType = "fullPageTruncated";
      }
      const height = getDocumentHeight();
      if (height > MAX_PAGE_HEIGHT) {
        captureType = "fullPageTruncated";
      }
      selectedPos = new Selection(0, 0, width, height);
      setState("previewing");
    },
    onDownloadPreview: () => {
      sendEvent(
        `download-${captureType
          .replace(/([a-z])([A-Z])/g, "$1-$2")
          .toLowerCase()}`,
        "download-preview-button"
      );
      downloadShot();
    },
    onCopyPreview: () => {
      sendEvent(
        `copy-${captureType.replace(/([a-z])([A-Z])/g, "$1-$2").toLowerCase()}`,
        "copy-preview-button"
      );
      copyShot();
    },
  };

  /** Holds all the objects that handle events for each state: */
  const stateHandlers = {};

  function getState() {
    return getState.state;
  }
  getState.state = "cancel";

  function setState(s) {
    if (!stateHandlers[s]) {
      throw new Error("Unknown state: " + s);
    }
    const cur = getState.state;
    const handler = stateHandlers[cur];
    if (handler.end) {
      handler.end();
    }
    getState.state = s;
    if (stateHandlers[s].start) {
      stateHandlers[s].start();
    }
  }

  /** Various values that the states use: */
  let mousedownPos;
  let selectedPos;
  let resizeDirection;
  let resizeStartPos;
  let resizeStartSelected;
  let resizeHasMoved;
  let mouseupNoAutoselect = false;
  let autoDetectRect;

  /** Represents a single x/y point, typically for a mouse click that doesn't have a drag: */
  class Pos {
    constructor(x, y) {
      this.x = x;
      this.y = y;
    }

    elementFromPoint() {
      return ui.iframe.getElementFromPoint(
        this.x - window.pageXOffset,
        this.y - window.pageYOffset
      );
    }

    distanceTo(x, y) {
      return Math.sqrt(Math.pow(this.x - x, 2) + Math.pow(this.y - y, 2));
    }
  }

  /** *********************************************
   * all stateHandlers
   */

  let dataUrl;

  stateHandlers.previewing = {
    start() {
      shooter.preview(selectedPos, captureType);
    },
  };

  stateHandlers.crosshairs = {
    cachedEl: null,

    start() {
      selectedPos = mousedownPos = null;
      this.cachedEl = null;
      watchPromise(
        ui.iframe
          .display(installHandlersOnDocument, standardOverlayCallbacks)
          .then(() => {
            ui.iframe.usePreSelection();
            ui.Box.remove();
          })
      );
    },

    mousemove(event) {
      ui.PixelDimensions.display(
        event.pageX,
        event.pageY,
        event.pageX,
        event.pageY
      );
      if (
        event.target.classList &&
        !event.target.classList.contains("preview-overlay")
      ) {
        // User is hovering over a toolbar button or control
        autoDetectRect = null;
        if (this.cachedEl) {
          this.cachedEl = null;
        }
        ui.HoverBox.hide();
        return;
      }
      let el;
      if (
        event.target.classList &&
        event.target.classList.contains("preview-overlay")
      ) {
        // The hover is on the overlay, so we need to figure out the real element
        el = ui.iframe.getElementFromPoint(
          event.pageX + window.scrollX - window.pageXOffset,
          event.pageY + window.scrollY - window.pageYOffset
        );
        const xpos = Math.floor(
          (10 * (event.pageX - window.innerWidth / 2)) / window.innerWidth
        );
        const ypos = Math.floor(
          (10 * (event.pageY - window.innerHeight / 2)) / window.innerHeight
        );

        for (let i = 0; i < 2; i++) {
          const move = `translate(${xpos}px, ${ypos}px)`;
          event.target.getElementsByClassName("eyeball")[
            i
          ].style.transform = move;
        }
      } else {
        // The hover is on the element we care about, so we use that
        el = event.target;
      }
      if (this.cachedEl && this.cachedEl === el) {
        // Still hovering over the same element
        return;
      }
      this.cachedEl = el;
      this.setAutodetectBasedOnElement(el);
    },

    setAutodetectBasedOnElement(el) {
      let lastRect;
      let lastNode;
      let rect;
      let attemptExtend = false;
      let node = el;
      while (node) {
        rect = Selection.getBoundingClientRect(node);
        if (!rect) {
          rect = lastRect;
          break;
        }
        if (rect.width < MIN_DETECT_WIDTH || rect.height < MIN_DETECT_HEIGHT) {
          // Avoid infinite loop for elements with zero or nearly zero height,
          // like non-clearfixed float parents with or without borders.
          break;
        }
        if (rect.width > MAX_DETECT_WIDTH || rect.height > MAX_DETECT_HEIGHT) {
          // Then the last rectangle is better
          rect = lastRect;
          attemptExtend = true;
          break;
        }
        if (
          rect.width >= MIN_DETECT_WIDTH &&
          rect.height >= MIN_DETECT_HEIGHT
        ) {
          if (!doNotAutoselectTags[node.tagName]) {
            break;
          }
        }
        lastRect = rect;
        lastNode = node;
        node = node.parentNode;
      }
      if (rect && node) {
        const evenBetter = this.evenBetterElement(node, rect);
        if (evenBetter) {
          node = lastNode = evenBetter;
          rect = Selection.getBoundingClientRect(evenBetter);
          attemptExtend = false;
        }
      }
      if (rect && attemptExtend) {
        let extendNode = lastNode.nextSibling;
        while (extendNode) {
          if (extendNode.nodeType === document.ELEMENT_NODE) {
            break;
          }
          extendNode = extendNode.nextSibling;
          if (!extendNode) {
            const parent = lastNode.parentNode;
            for (let i = 0; i < parent.childNodes.length; i++) {
              if (parent.childNodes[i] === lastNode) {
                extendNode = parent.childNodes[i + 1];
              }
            }
          }
        }
        if (extendNode) {
          const extendSelection = Selection.getBoundingClientRect(extendNode);
          const extendRect = rect.union(extendSelection);
          if (
            extendRect.width <= MAX_DETECT_WIDTH &&
            extendRect.height <= MAX_DETECT_HEIGHT
          ) {
            rect = extendRect;
          }
        }
      }

      if (
        rect &&
        (rect.width < MIN_DETECT_ABSOLUTE_WIDTH ||
          rect.height < MIN_DETECT_ABSOLUTE_HEIGHT)
      ) {
        rect = null;
      }
      if (!rect) {
        ui.HoverBox.hide();
      } else {
        ui.HoverBox.display(rect);
      }
      autoDetectRect = rect;
    },

    /** When we find an element, maybe there's one that's just a little bit better... */
    evenBetterElement(node, origRect) {
      let el = node.parentNode;
      const ELEMENT_NODE = document.ELEMENT_NODE;
      while (el && el.nodeType === ELEMENT_NODE) {
        if (!el.getAttribute) {
          return null;
        }
        const role = el.getAttribute("role");
        if (
          role === "article" ||
          (el.className &&
            typeof el.className === "string" &&
            el.className.search("tweet ") !== -1)
        ) {
          const rect = Selection.getBoundingClientRect(el);
          if (!rect) {
            return null;
          }
          if (
            rect.width <= MAX_DETECT_WIDTH &&
            rect.height <= MAX_DETECT_HEIGHT
          ) {
            return el;
          }
          return null;
        }
        el = el.parentNode;
      }
      return null;
    },

    mousedown(event) {
      // FIXME: this is happening but we don't know why, we'll track it now
      // but avoid popping up messages:
      if (typeof ui === "undefined") {
        const exc = new Error("Undefined ui in mousedown");
        exc.unloadTime = unloadTime;
        exc.nowTime = Date.now();
        exc.noPopup = true;
        exc.noReport = true;
        throw exc;
      }
      if (ui.isHeader(event.target)) {
        return undefined;
      }
      // If the pageX is greater than this, then probably it's an attempt to get
      // to the scrollbar, or an actual scroll, and not an attempt to start the
      // selection:
      const maxX = window.innerWidth - SCROLLBAR_WIDTH;
      if (event.pageX >= maxX) {
        event.stopPropagation();
        event.preventDefault();
        return false;
      }

      mousedownPos = new Pos(
        event.pageX + window.scrollX,
        event.pageY + window.scrollY
      );
      setState("draggingReady");
      event.stopPropagation();
      event.preventDefault();
      return false;
    },

    end() {
      ui.HoverBox.remove();
      ui.PixelDimensions.remove();
    },
  };

  stateHandlers.draggingReady = {
    minMove: 40, // px
    minAutoImageWidth: 40,
    minAutoImageHeight: 40,
    maxAutoElementWidth: 800,
    maxAutoElementHeight: 600,

    start() {
      ui.iframe.usePreSelection();
      ui.Box.remove();
    },

    mousemove(event) {
      if (mousedownPos.distanceTo(event.pageX, event.pageY) > this.minMove) {
        selectedPos = new Selection(
          mousedownPos.x,
          mousedownPos.y,
          event.pageX + window.scrollX,
          event.pageY + window.scrollY
        );
        mousedownPos = null;
        setState("dragging");
      }
    },

    mouseup(event) {
      // If we don't get into "dragging" then we attempt an autoselect
      if (mouseupNoAutoselect) {
        sendEvent("cancel-selection", "selection-background-mousedown");
        setState("crosshairs");
        return false;
      }
      if (autoDetectRect) {
        selectedPos = autoDetectRect;
        selectedPos.x1 += window.scrollX;
        selectedPos.y1 += window.scrollY;
        selectedPos.x2 += window.scrollX;
        selectedPos.y2 += window.scrollY;
        autoDetectRect = null;
        mousedownPos = null;
        ui.iframe.useSelection();
        ui.Box.display(selectedPos, standardDisplayCallbacks);
        sendEvent(
          "make-selection",
          "selection-click",
          eventOptionsForBox(selectedPos)
        );
        setState("selected");
        sendEvent("autoselect");
      } else {
        sendEvent("no-selection", "no-element-found");
        setState("crosshairs");
      }
      return undefined;
    },

    click(event) {
      this.mouseup(event);
    },

    findGoodEl() {
      let el = mousedownPos.elementFromPoint();
      if (!el) {
        return null;
      }
      const isGoodEl = element => {
        if (element.nodeType !== document.ELEMENT_NODE) {
          return false;
        }
        if (element.tagName === "IMG") {
          const rect = element.getBoundingClientRect();
          return (
            rect.width >= this.minAutoImageWidth &&
            rect.height >= this.minAutoImageHeight
          );
        }
        const display = window.getComputedStyle(element).display;
        if (["block", "inline-block", "table"].includes(display)) {
          return true;
          // FIXME: not sure if this is useful:
          // let rect = el.getBoundingClientRect();
          // return rect.width <= this.maxAutoElementWidth && rect.height <= this.maxAutoElementHeight;
        }
        return false;
      };
      while (el) {
        if (isGoodEl(el)) {
          return el;
        }
        el = el.parentNode;
      }
      return null;
    },

    end() {
      mouseupNoAutoselect = false;
    },
  };

  stateHandlers.dragging = {
    start() {
      ui.iframe.useSelection();
      ui.Box.display(selectedPos);
    },

    mousemove(event) {
      selectedPos.x2 = util.truncateX(event.pageX);
      selectedPos.y2 = util.truncateY(event.pageY);
      scrollIfByEdge(event.pageX, event.pageY);
      ui.Box.display(selectedPos);
      ui.PixelDimensions.display(
        event.pageX,
        event.pageY,
        selectedPos.width,
        selectedPos.height
      );
    },

    mouseup(event) {
      selectedPos.x2 = util.truncateX(event.pageX);
      selectedPos.y2 = util.truncateY(event.pageY);
      ui.Box.display(selectedPos, standardDisplayCallbacks);
      sendEvent(
        "make-selection",
        "selection-drag",
        eventOptionsForBox({
          top: selectedPos.y1,
          bottom: selectedPos.y2,
          left: selectedPos.x1,
          right: selectedPos.x2,
        })
      );
      setState("selected");
    },

    end() {
      ui.PixelDimensions.remove();
    },
  };

  stateHandlers.selected = {
    start() {
      ui.iframe.useSelection();
    },

    mousedown(event) {
      const target = event.target;
      if (target.tagName === "HTML") {
        // This happens when you click on the scrollbar
        return undefined;
      }
      const direction = ui.Box.draggerDirection(target);
      if (direction) {
        sendEvent("start-resize-selection", "handle");
        stateHandlers.resizing.startResize(event, direction);
      } else if (ui.Box.isSelection(target)) {
        sendEvent("start-move-selection", "selection");
        stateHandlers.resizing.startResize(event, "move");
      } else if (!ui.Box.isControl(target)) {
        mousedownPos = new Pos(event.pageX, event.pageY);
        setState("crosshairs");
      }
      event.preventDefault();
      return false;
    },
  };

  stateHandlers.resizing = {
    start() {
      ui.iframe.useSelection();
      selectedPos.sortCoords();
    },

    startResize(event, direction) {
      selectedPos.sortCoords();
      resizeDirection = direction;
      resizeStartPos = new Pos(event.pageX, event.pageY);
      resizeStartSelected = selectedPos.clone();
      resizeHasMoved = false;
      setState("resizing");
    },

    mousemove(event) {
      this._resize(event);
      if (resizeDirection !== "move") {
        ui.PixelDimensions.display(
          event.pageX,
          event.pageY,
          selectedPos.width,
          selectedPos.height
        );
      }
      return false;
    },

    mouseup(event) {
      this._resize(event);
      sendEvent("selection-resized");
      ui.Box.display(selectedPos, standardDisplayCallbacks);
      if (resizeHasMoved) {
        if (resizeDirection === "move") {
          const startPos = new Pos(
            resizeStartSelected.left,
            resizeStartSelected.top
          );
          const endPos = new Pos(selectedPos.left, selectedPos.top);
          sendEvent(
            "move-selection",
            "mouseup",
            eventOptionsForMove(startPos, endPos)
          );
        } else {
          sendEvent(
            "resize-selection",
            "mouseup",
            eventOptionsForResize(resizeStartSelected, selectedPos)
          );
        }
      } else if (resizeDirection === "move") {
        sendEvent("keep-resize-selection", "mouseup");
      } else {
        sendEvent("keep-move-selection", "mouseup");
      }
      setState("selected");
    },

    _resize(event) {
      const diffX = event.pageX - resizeStartPos.x;
      const diffY = event.pageY - resizeStartPos.y;
      const movement = movements[resizeDirection];
      if (movement[0]) {
        let moveX = movement[0];
        moveX = moveX === "*" ? ["x1", "x2"] : [moveX];
        for (const moveDir of moveX) {
          selectedPos[moveDir] = util.truncateX(
            resizeStartSelected[moveDir] + diffX
          );
        }
      }
      if (movement[1]) {
        let moveY = movement[1];
        moveY = moveY === "*" ? ["y1", "y2"] : [moveY];
        for (const moveDir of moveY) {
          selectedPos[moveDir] = util.truncateY(
            resizeStartSelected[moveDir] + diffY
          );
        }
      }
      if (diffX || diffY) {
        resizeHasMoved = true;
      }
      scrollIfByEdge(event.pageX, event.pageY);
      ui.Box.display(selectedPos);
    },

    end() {
      resizeDirection = resizeStartPos = resizeStartSelected = null;
      selectedPos.sortCoords();
      ui.PixelDimensions.remove();
    },
  };

  stateHandlers.cancel = {
    start() {
      ui.iframe.hide();
      ui.Box.remove();
    },
  };

  function getDocumentWidth() {
    return Math.max(
      document.body && document.body.clientWidth,
      document.documentElement.clientWidth,
      document.body && document.body.scrollWidth,
      document.documentElement.scrollWidth
    );
  }
  function getDocumentHeight() {
    return Math.max(
      document.body && document.body.clientHeight,
      document.documentElement.clientHeight,
      document.body && document.body.scrollHeight,
      document.documentElement.scrollHeight
    );
  }

  function scrollIfByEdge(pageX, pageY) {
    const top = window.scrollY;
    const bottom = top + window.innerHeight;
    const left = window.scrollX;
    const right = left + window.innerWidth;
    if (pageY + SCROLL_BY_EDGE >= bottom && bottom < getDocumentHeight()) {
      window.scrollBy(0, SCROLL_BY_EDGE);
    } else if (pageY - SCROLL_BY_EDGE <= top) {
      window.scrollBy(0, -SCROLL_BY_EDGE);
    }
    if (pageX + SCROLL_BY_EDGE >= right && right < getDocumentWidth()) {
      window.scrollBy(SCROLL_BY_EDGE, 0);
    } else if (pageX - SCROLL_BY_EDGE <= left) {
      window.scrollBy(-SCROLL_BY_EDGE, 0);
    }
  }

  /** *********************************************
   * Selection communication
   */

  exports.activate = function() {
    if (!document.body) {
      callBackground("abortStartShot");
      const tagName = String(document.documentElement.tagName || "").replace(
        /[^a-z0-9]/gi,
        ""
      );
      sendEvent("abort-start-shot", `document-is-${tagName}`);
      selectorLoader.unloadModules();
      return;
    }
    if (isFrameset()) {
      callBackground("abortStartShot");
      sendEvent("abort-start-shot", "frame-page");
      selectorLoader.unloadModules();
      return;
    }
    addHandlers();
    setState("crosshairs");
  };

  function isFrameset() {
    return document.body.tagName === "FRAMESET";
  }

  exports.deactivate = function() {
    try {
      sendEvent("internal", "deactivate");
      setState("cancel");
      callBackground("closeSelector");
      selectorLoader.unloadModules();
    } catch (e) {
      log.error("Error in deactivate", e);
      // Sometimes this fires so late that the document isn't available
      // We don't care about the exception, so we swallow it here
    }
  };

  let unloadTime = 0;

  exports.unload = function() {
    // Note that ui.unload() will be called on its own
    unloadTime = Date.now();
    removeHandlers();
  };

  /** *********************************************
   * Event handlers
   */

  const primedDocumentHandlers = new Map();
  let registeredDocumentHandlers = [];

  function addHandlers() {
    ["mouseup", "mousedown", "mousemove", "click"].forEach(eventName => {
      const fn = watchFunction(
        assertIsTrusted(function(event) {
          if (typeof event.button === "number" && event.button !== 0) {
            // Not a left click
            return undefined;
          }
          if (
            event.ctrlKey ||
            event.shiftKey ||
            event.altKey ||
            event.metaKey
          ) {
            // Modified click of key
            return undefined;
          }
          const state = getState();
          const handler = stateHandlers[state];
          if (handler[event.type]) {
            return handler[event.type](event);
          }
          return undefined;
        })
      );
      primedDocumentHandlers.set(eventName, fn);
    });
    primedDocumentHandlers.set(
      "keyup",
      watchFunction(assertIsTrusted(keyupHandler))
    );
    primedDocumentHandlers.set(
      "keydown",
      watchFunction(assertIsTrusted(keydownHandler))
    );
    window.document.addEventListener(
      "visibilitychange",
      visibilityChangeHandler
    );
    window.addEventListener("beforeunload", beforeunloadHandler);
  }

  let mousedownSetOnDocument = false;

  function installHandlersOnDocument(docObj) {
    for (const [eventName, handler] of primedDocumentHandlers) {
      const watchHandler = watchFunction(handler);
      const useCapture = eventName !== "keyup";
      docObj.addEventListener(eventName, watchHandler, useCapture);
      registeredDocumentHandlers.push({
        name: eventName,
        doc: docObj,
        handler: watchHandler,
        useCapture,
      });
    }
    if (!mousedownSetOnDocument) {
      const mousedownHandler = primedDocumentHandlers.get("mousedown");
      document.addEventListener("mousedown", mousedownHandler, true);
      registeredDocumentHandlers.push({
        name: "mousedown",
        doc: document,
        handler: mousedownHandler,
        useCapture: true,
      });
      mousedownSetOnDocument = true;
    }
  }

  function beforeunloadHandler() {
    sendEvent("cancel-shot", "tab-load");
    exports.deactivate();
  }

  function keydownHandler(event) {
    // In MacOS, the keyup event for 'c' is not fired when performing cmd+c.
    if (
      event.code === "KeyC" &&
      (event.ctrlKey || event.metaKey) &&
      ["previewing", "selected"].includes(getState.state)
    ) {
      catcher.watchPromise(
        callBackground("getPlatformOs").then(os => {
          if (
            (event.ctrlKey && os !== "mac") ||
            (event.metaKey && os === "mac")
          ) {
            sendEvent("copy-shot", "keyboard-copy");
            copyShot();
          }
        })
      );
    }
  }

  function keyupHandler(event) {
    if (event.shiftKey || event.altKey || event.ctrlKey || event.metaKey) {
      // unused modifier keys
      return;
    }
    if ((event.key || event.code) === "Escape") {
      sendEvent("cancel-shot", "keyboard-escape");
      exports.deactivate();
    }
    // Enter to trigger Save or Download by default. But if the user tabbed to
    // select another button, then we do not want this.
    if (
      (event.key || event.code) === "Enter" &&
      getState.state === "selected" &&
      ui.iframe.document().activeElement.tagName === "BODY"
    ) {
      sendEvent("download-shot", "keyboard-enter");
      downloadShot();
    }
  }

  function visibilityChangeHandler(event) {
    // The document is the event target
    if (event.target.hidden) {
      sendEvent("internal", "document-hidden");
    }
  }

  function removeHandlers() {
    window.removeEventListener("beforeunload", beforeunloadHandler);
    window.document.removeEventListener(
      "visibilitychange",
      visibilityChangeHandler
    );
    for (const {
      name,
      doc,
      handler,
      useCapture,
    } of registeredDocumentHandlers) {
      doc.removeEventListener(name, handler, !!useCapture);
    }
    registeredDocumentHandlers = [];
  }

  catcher.watchFunction(exports.activate)();

  return exports;
})();

null;
