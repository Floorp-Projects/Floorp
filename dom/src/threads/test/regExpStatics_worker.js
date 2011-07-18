var runCount = 0;
var timeout;

onmessage = function() {
  run();
  timeout = setTimeout(run, 0);
  timeout = setTimeout(run, 5000);
};

function run() {
  if (RegExp.$1) {
    throw "RegExp.$1 already set!";
    cancelTimeout(timeout);
  }

  var match = /a(sd)f/.exec("asdf");
  if (!RegExp.$1) {
    throw "RegExp.$1 didn't get set!";
    cancelTimeout(timeout);
  }

  if (++runCount == 3) {
    postMessage("done");
  }
}

