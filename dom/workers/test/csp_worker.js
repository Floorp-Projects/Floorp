/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onmessage = function (event) {
  if (event.data.do == "eval") {
    var res;
    try {
      // eslint-disable-next-line no-eval
      res = eval("40+2");
    } catch (ex) {
      res = ex + "";
    }
    postMessage(res);
  } else if (event.data.do == "nest") {
    var worker = new Worker(event.data.uri);
    if (--event.data.level) {
      worker.postMessage(event.data);
    } else {
      worker.postMessage({ do: "eval" });
    }
    worker.onmessage = e => {
      postMessage(e.data);
    };
  }
};
