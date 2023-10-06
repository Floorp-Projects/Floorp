/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var gTimeoutId;
var gTimeoutCount = 0;
var gIntervalCount = 0;

function timeoutFunc() {
  if (++gTimeoutCount > 1) {
    throw new Error("Timeout called more than once!");
  }
  postMessage("timeoutFinished");
}

function intervalFunc() {
  if (++gIntervalCount == 2) {
    postMessage("intervalFinished");
  }
}

function messageListener(event) {
  switch (event.data) {
    case "startTimeout":
      gTimeoutId = setTimeout(timeoutFunc, 2000);
      clearTimeout(gTimeoutId);
      gTimeoutId = setTimeout(timeoutFunc, 2000);
      break;
    case "startInterval":
      gTimeoutId = setInterval(intervalFunc, 2000);
      break;
    case "cancelInterval":
      clearInterval(gTimeoutId);
      postMessage("intervalCanceled");
      break;
    case "startExpression":
      // eslint-disable-next-line no-implied-eval
      setTimeout("this.postMessage('expressionFinished');", 2000);
      break;
    default:
      throw "Bad message: " + event.data;
  }
}

addEventListener("message", messageListener, false);
