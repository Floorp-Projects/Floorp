/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view remains responsive when faced with
 * huge ammounts of data.
 */

const TAB_URL = EXAMPLE_URL + "doc_large-array-buffer.html";

let gTab, gDebuggee, gPanel, gDebugger;
let gVariables;

function test() {
  // This is a very, very stressful test.
  // Thankfully, after bug 830344 none of this will be necessary anymore.
  requestLongerTimeout(10);

  initDebugger(TAB_URL).then(([aTab, aDebuggee, aPanel]) => {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVariables = gDebugger.DebuggerView.Variables;

    gDebugger.DebuggerView.Variables.lazyAppend = true;

    waitForSourceAndCaretAndScopes(gPanel, ".html", 18)
      .then(() => performTest())
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .then(null, aError => {
        ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
      });

    EventUtils.sendMouseEvent({ type: "click" },
      gDebuggee.document.querySelector("button"),
      gDebuggee);
  });
}

function performTest() {
  let deferred = promise.defer();

  let localScope = gVariables.getScopeAtIndex(0);
  is(localScope.expanded, true,
    "The local scope should be expanded by default.");

  let localEnums = localScope.target.querySelector(".variables-view-element-details.enum").childNodes;
  let localNonEnums = localScope.target.querySelector(".variables-view-element-details.nonenum").childNodes;

  is(localEnums.length, 5,
    "The local scope should contain all the created enumerable elements.");
  is(localNonEnums.length, 0,
    "The local scope should contain all the created non-enumerable elements.");

  let bufferVar = localScope.get("buffer");
  let zVar = localScope.get("z");

  is(bufferVar.target.querySelector(".name").getAttribute("value"), "buffer",
    "Should have the right property name for 'buffer'.");
  is(bufferVar.target.querySelector(".value").getAttribute("value"), "ArrayBuffer",
    "Should have the right property value for 'buffer'.");
  ok(bufferVar.target.querySelector(".value").className.contains("token-other"),
    "Should have the right token class for 'buffer'.");

  is(zVar.target.querySelector(".name").getAttribute("value"), "z",
    "Should have the right property name for 'z'.");
  is(zVar.target.querySelector(".value").getAttribute("value"), "Int8Array",
    "Should have the right property value for 'z'.");
  ok(zVar.target.querySelector(".value").className.contains("token-other"),
    "Should have the right token class for 'z'.");

  EventUtils.sendMouseEvent({ type: "mousedown" },
    bufferVar.target.querySelector(".arrow"),
    gDebugger);

  EventUtils.sendMouseEvent({ type: "mousedown" },
    zVar.target.querySelector(".arrow"),
    gDebugger);

  // Need to wait for 0 enumerable and 2 non-enumerable properties in bufferVar,
  // and 10000 enumerable and 5 non-enumerable properties in zVar.
  let total = 0 + 2 + 10000 + 5;
  let loaded = 0;
  let paints = 0;

  // Make sure the variables view doesn't scroll while adding the properties.
  let [oldX, oldY] = getScroll();
  info("Initial scroll position: " + oldX + ", " + oldY);

  waitForProperties(total, {
    onLoading: function(aLoaded) {
      ok(aLoaded >= loaded,
        "Should have loaded more properties.");

      let [newX, newY] = getScroll();
      info("Current scroll position: " + newX + " " + newY);
      is(oldX, newX, "The variables view hasn't scrolled horizontally.");
      is(oldY, newY, "The variables view hasn't scrolled vertically.");

      info("Displayed " + aLoaded + " properties, not finished yet.");

      loaded = aLoaded;
      paints++;
    },
    onFinished: function(aLoaded) {
      ok(aLoaded == total,
        "Displayed all the properties.");
      isnot(paints, 0,
        "Debugger was unresponsive, sad panda.");

      let [newX, newY] = getScroll();
      info("Current scroll position: " + newX + ", " + newY);
      is(oldX, newX, "The variables view hasn't scrolled horizontally.");
      is(oldY, newY, "The variables view hasn't scrolled vertically.");

      is(bufferVar._enum.childNodes.length, 0,
        "The bufferVar should contain all the created enumerable elements.");
      is(bufferVar._nonenum.childNodes.length, 2,
        "The bufferVar should contain all the created non-enumerable elements.");

      let bufferVarByteLengthProp = bufferVar.get("byteLength");
      let bufferVarProtoProp = bufferVar.get("__proto__");

      is(bufferVarByteLengthProp.target.querySelector(".name").getAttribute("value"), "byteLength",
        "Should have the right property name for 'byteLength'.");
      is(bufferVarByteLengthProp.target.querySelector(".value").getAttribute("value"), "10000",
        "Should have the right property value for 'byteLength'.");
      ok(bufferVarByteLengthProp.target.querySelector(".value").className.contains("token-number"),
        "Should have the right token class for 'byteLength'.");

      is(bufferVarProtoProp.target.querySelector(".name").getAttribute("value"), "__proto__",
        "Should have the right property name for '__proto__'.");
      is(bufferVarProtoProp.target.querySelector(".value").getAttribute("value"), "ArrayBufferPrototype",
        "Should have the right property value for '__proto__'.");
      ok(bufferVarProtoProp.target.querySelector(".value").className.contains("token-other"),
        "Should have the right token class for '__proto__'.");

      is(zVar._enum.childNodes.length, 10000,
        "The zVar should contain all the created enumerable elements.");
      is(zVar._nonenum.childNodes.length, 5,
        "The zVar should contain all the created non-enumerable elements.");

      let zVarByteLengthProp = zVar.get("byteLength");
      let zVarByteOffsetProp = zVar.get("byteOffset");
      let zVarProtoProp = zVar.get("__proto__");

      is(zVarByteLengthProp.target.querySelector(".name").getAttribute("value"), "byteLength",
        "Should have the right property name for 'byteLength'.");
      is(zVarByteLengthProp.target.querySelector(".value").getAttribute("value"), "10000",
        "Should have the right property value for 'byteLength'.");
      ok(zVarByteLengthProp.target.querySelector(".value").className.contains("token-number"),
        "Should have the right token class for 'byteLength'.");

      is(zVarByteOffsetProp.target.querySelector(".name").getAttribute("value"), "byteOffset",
        "Should have the right property name for 'byteOffset'.");
      is(zVarByteOffsetProp.target.querySelector(".value").getAttribute("value"), "0",
        "Should have the right property value for 'byteOffset'.");
      ok(zVarByteOffsetProp.target.querySelector(".value").className.contains("token-number"),
        "Should have the right token class for 'byteOffset'.");

      is(zVarProtoProp.target.querySelector(".name").getAttribute("value"), "__proto__",
        "Should have the right property name for '__proto__'.");
      is(zVarProtoProp.target.querySelector(".value").getAttribute("value"), "Int8ArrayPrototype",
        "Should have the right property value for '__proto__'.");
      ok(zVarProtoProp.target.querySelector(".value").className.contains("token-other"),
        "Should have the right token class for '__proto__'.");

      let arrayElements = zVar._enum.childNodes;
      for (let i = 0, len = arrayElements.length; i < len; i++) {
        let node = arrayElements[i];
        let name = node.querySelector(".name").getAttribute("value");
        let value = node.querySelector(".value").getAttribute("value");
        if (name !== i + "" || value !== "0") {
          ok(false, "The array items aren't in the correct order.");
        }
      }

      deferred.resolve();
    },
    onTimeout: function() {
      ok(false, "Timed out while polling for the properties.");
      deferred.resolve();
    }
  });

  function getScroll() {
    let scrollX = {};
    let scrollY = {};
    gVariables._boxObject.getPosition(scrollX, scrollY);
    return [scrollX.value, scrollY.value];
  }

  return deferred.promise;
}

function waitForProperties(aTotal, aCallbacks, aInterval = 10) {
  let localScope = gVariables.getScopeAtIndex(0);
  let bufferEnum = localScope.get("buffer")._enum.childNodes;
  let bufferNonEnum = localScope.get("buffer")._nonenum.childNodes;
  let zEnum = localScope.get("z")._enum.childNodes;
  let zNonEnum = localScope.get("z")._nonenum.childNodes;

  // Poll every few milliseconds until the properties are retrieved.
  let count = 0;
  let intervalId = window.setInterval(() => {
    // Make sure we don't wait for too long.
    if (++count > 1000) {
      window.clearInterval(intervalId);
      aCallbacks.onTimeout();
      return;
    }
    // Check if we need to wait for a few more properties to be fetched.
    let loaded = bufferEnum.length + bufferNonEnum.length + zEnum.length + zNonEnum.length;
    if (loaded < aTotal) {
      aCallbacks.onLoading(loaded);
      return;
    }
    // We got all the properties, it's safe to callback.
    window.clearInterval(intervalId);
    aCallbacks.onFinished(loaded);
  }, aInterval);
}

registerCleanupFunction(function() {
  gTab = null;
  gDebuggee = null;
  gPanel = null;
  gDebugger = null;
  gVariables = null;
});
