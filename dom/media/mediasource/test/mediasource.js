// Helpers for Media Source Extensions tests

var gMSETestPrefs = [
  [ "media.mediasource.enabled", true ]
];

// Called before runWithMSE() to set the prefs before running MSE tests.
function addMSEPrefs(...prefs) {
  gMSETestPrefs = gMSETestPrefs.concat(prefs);
}

function runWithMSE(testFunction) {
  function bootstrapTest() {
    var ms = new MediaSource();

    var el = document.createElement("video");
    el.src = URL.createObjectURL(ms);
    el.preload = "auto";

    document.body.appendChild(el);
    SimpleTest.registerCleanupFunction(function () {
      el.remove();
      // Don't trigger load algorithm to prevent 'error' events.
      el.preload = "none";
      el.src = null;
    });

    testFunction(ms, el);
  }

  addLoadEvent(function () {
    SpecialPowers.pushPrefEnv({"set": gMSETestPrefs}, bootstrapTest);
  });
}

function fetchWithXHR(uri, onLoadFunction) {
  var p = new Promise(function(resolve, reject) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", uri, true);
    xhr.responseType = "arraybuffer";
    xhr.addEventListener("load", function () {
      is(xhr.status, 200, "fetchWithXHR load uri='" + uri + "' status=" + xhr.status);
      resolve(xhr.response);
    });
    xhr.send();
  });

  if (onLoadFunction) {
    p.then(onLoadFunction);
  }

  return p;
};

function range(start, end) {
  var rv = [];
  for (var i = start; i < end; ++i) {
    rv.push(i);
  }
  return rv;
}

function once(target, name, cb) {
  var p = new Promise(function(resolve, reject) {
    target.addEventListener(name, function onceEvent() {
      target.removeEventListener(name, onceEvent);
      resolve();
    });
  });
  if (cb) {
    p.then(cb);
  }
  return p;
}

function timeRangeToString(r) {
  var str = "TimeRanges: ";
  for (var i = 0; i < r.length; i++) {
    str += "[" + r.start(i) + ", " + r.end(i) + ")";
  }
  return str;
}

function loadSegment(sb, typedArrayOrArrayBuffer) {
  var typedArray = (typedArrayOrArrayBuffer instanceof ArrayBuffer) ? new Uint8Array(typedArrayOrArrayBuffer)
                                                                    : typedArrayOrArrayBuffer;
  info(`Loading buffer: [${typedArray.byteOffset}, ${typedArray.byteOffset + typedArray.byteLength})`);
  var beforeBuffered = timeRangeToString(sb.buffered);
  return new Promise(function(resolve, reject) {
    once(sb, 'update').then(function() {
      var afterBuffered = timeRangeToString(sb.buffered);
      info(`SourceBuffer buffered ranges grew from ${beforeBuffered} to ${afterBuffered}`);
      resolve();
    });
    sb.appendBuffer(typedArray);
  });
}

function fetchAndLoad(sb, prefix, chunks, suffix) {

  // Fetch the buffers in parallel.
  var buffers = {};
  var fetches = [];
  for (var chunk of chunks) {
    fetches.push(fetchWithXHR(prefix + chunk + suffix).then(((c, x) => buffers[c] = x).bind(null, chunk)));
  }

  // Load them in series, as required per spec.
  return Promise.all(fetches).then(function() {
    var rv = Promise.resolve();
    for (var chunk of chunks) {
      rv = rv.then(loadSegment.bind(null, sb, buffers[chunk]));
    }
    return rv;
  });
}

//Register timeout function to dump debugging logs.
SimpleTest.registerTimeoutFunction(function() {
  for (var v of document.getElementsByTagName("video")) {
    v.mozDumpDebugInfo();
  }
  for (var a of document.getElementsByTagName("audio")) {
    a.mozDumpDebugInfo();
  }
});

function waitUntilTime(target, targetTime) {
  return new Promise(function(resolve, reject) {
    target.addEventListener("waiting", function onwaiting() {
      info("Got a waiting event at " + target.currentTime);
      if (target.currentTime >= targetTime) {
        ok(true, "Reached target time of: " + targetTime);
        target.removeEventListener("waiting", onwaiting);
        resolve();
      }
    });
  });
}
