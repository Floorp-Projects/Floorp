var accumulatedRect = null;
var onpaint = function() {};
var debug = false;
var CI = Components.interfaces;
var utils = window.QueryInterface(CI.nsIInterfaceRequestor)
            .getInterface(CI.nsIDOMWindowUtils);

function paintListener(event) {
  if (event.target != window)
    return;
  if (debug) {
    dump("got MozAfterPaint: " + event.boundingClientRect.left + "," + event.boundingClientRect.top + "," +
         event.boundingClientRect.right + "," + event.boundingClientRect.bottom + "\n");
  }
  if (accumulatedRect) {
    accumulatedRect[0] = Math.min(accumulatedRect[0], event.boundingClientRect.left);
    accumulatedRect[1] = Math.min(accumulatedRect[1], event.boundingClientRect.top);
    accumulatedRect[2] = Math.max(accumulatedRect[2], event.boundingClientRect.right);
    accumulatedRect[3] = Math.max(accumulatedRect[3], event.boundingClientRect.bottom);
  } else {
    accumulatedRect = [event.boundingClientRect.left, event.boundingClientRect.top,
                       event.boundingClientRect.right, event.boundingClientRect.bottom];
  }
  onpaint();
}
window.addEventListener("MozAfterPaint", paintListener, false);

function waitForAllPaintsFlushed(callback, subdoc) {
  document.documentElement.getBoundingClientRect();
  if (subdoc) {
    subdoc.documentElement.getBoundingClientRect();
  }
  if (!utils.isMozAfterPaintPending) {
    if (debug) {
      dump("done...\n");
    }
    var result = accumulatedRect;
    accumulatedRect = null;
    onpaint = function() {};
    if (!result) {
      result = [0,0,0,0];
    }
    callback(result[0], result[1], result[2], result[3]);
    return;
  }
  if (debug) {
    dump("waiting for paint...\n");
  }
  onpaint = function() { waitForAllPaintsFlushed(callback, subdoc); };
}

var notPaintedList = new Array();
var paintedList = new Array();

function ensureNotPainted(element)
{
  utils.checkAndClearPaintedState(element);
  notPaintedList.push(element);
}

function ensurePainted(element)
{
  utils.checkAndClearPaintedState(element);
  paintedList.push(element);
}

function checkInvalidation()
{
  for (var i = 0; i < notPaintedList.length; i++) {
    ok(!utils.checkAndClearPaintedState(notPaintedList[i]), "Should not have repainted element!");
  }
  notPaintedList = new Array();
  for (var i = 0; i < paintedList.length; i++) {
    ok(utils.checkAndClearPaintedState(notPaintedList[i]), "Should have repainted element!");
  }
  paintedList = new Array();
}

function testInvalidation(callback)
{
  waitForAllPaintsFlushed(function() { checkInvalidation(); callback(); }); 
}

