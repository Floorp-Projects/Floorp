// Helpers for Media Source Extensions tests

let gMSETestPrefs = [
  [ "media.mediasource.enabled", true ],
  [ "media.audio-max-decode-error", 0 ],
  [ "media.video-max-decode-error", 0 ],
];

// Called before runWithMSE() to set the prefs before running MSE tests.
function addMSEPrefs(...prefs) {
  gMSETestPrefs = gMSETestPrefs.concat(prefs);
}

function runWithMSE(testFunction) {
  function bootstrapTest() {
    const ms = new MediaSource();

    const el = document.createElement("video");
    el.src = URL.createObjectURL(ms);
    el.preload = "auto";

    document.body.appendChild(el);
    SimpleTest.registerCleanupFunction(function() {
      el.remove();
      el.removeAttribute("src");
      el.load();
    });

    testFunction(ms, el);
  }

  addLoadEvent(function() {
    SpecialPowers.pushPrefEnv({"set": gMSETestPrefs}, bootstrapTest);
  });
}

function fetchWithXHR(uri, onLoadFunction) {
  const p = new Promise(function(resolve, reject) {
    const xhr = new XMLHttpRequest();
    xhr.open("GET", uri, true);
    xhr.responseType = "arraybuffer";
    xhr.addEventListener("load", function() {
      is(xhr.status, 200, "fetchWithXHR load uri='" + uri + "' status=" + xhr.status);
      resolve(xhr.response);
    });
    xhr.send();
  });

  if (onLoadFunction) {
    p.then(onLoadFunction);
  }

  return p;
}

function range(start, end) {
  const rv = [];
  for (let i = start; i < end; ++i) {
    rv.push(i);
  }
  return rv;
}

function once(target, name, cb) {
  const p = new Promise(function(resolve, reject) {
    target.addEventListener(name, function() {
      resolve();
    }, {once: true});
  });
  if (cb) {
    p.then(cb);
  }
  return p;
}

function timeRangeToString(r) {
  let str = "TimeRanges: ";
  for (let i = 0; i < r.length; i++) {
    str += "[" + r.start(i) + ", " + r.end(i) + ")";
  }
  return str;
}

function loadSegment(sb, typedArrayOrArrayBuffer) {
  const typedArray = (typedArrayOrArrayBuffer instanceof ArrayBuffer) ? new Uint8Array(typedArrayOrArrayBuffer)
                                                                      : typedArrayOrArrayBuffer;
  info(`Loading buffer: [${typedArray.byteOffset}, ${typedArray.byteOffset + typedArray.byteLength})`);
  const beforeBuffered = timeRangeToString(sb.buffered);
  return new Promise(function(resolve, reject) {
    once(sb, "update").then(function() {
      const afterBuffered = timeRangeToString(sb.buffered);
      info(`SourceBuffer buffered ranges grew from ${beforeBuffered} to ${afterBuffered}`);
      resolve();
    });
    sb.appendBuffer(typedArray);
  });
}

function fetchAndLoad(sb, prefix, chunks, suffix) {

  // Fetch the buffers in parallel.
  const buffers = {};
  const fetches = [];
  for (const chunk of chunks) {
    fetches.push(fetchWithXHR(prefix + chunk + suffix).then(((c, x) => buffers[c] = x).bind(null, chunk)));
  }

  // Load them in series, as required per spec.
  return Promise.all(fetches).then(function() {
    let rv = Promise.resolve();
    for (const chunk of chunks) {
      rv = rv.then(loadSegment.bind(null, sb, buffers[chunk]));
    }
    return rv;
  });
}

function loadSegmentAsync(sb, typedArrayOrArrayBuffer) {
  const typedArray = (typedArrayOrArrayBuffer instanceof ArrayBuffer) ? new Uint8Array(typedArrayOrArrayBuffer)
                                                                      : typedArrayOrArrayBuffer;
  info(`Loading buffer2: [${typedArray.byteOffset}, ${typedArray.byteOffset + typedArray.byteLength})`);
  const beforeBuffered = timeRangeToString(sb.buffered);
  return sb.appendBufferAsync(typedArray).then(() => {
    const afterBuffered = timeRangeToString(sb.buffered);
    info(`SourceBuffer buffered ranges grew from ${beforeBuffered} to ${afterBuffered}`);
  });
}

function fetchAndLoadAsync(sb, prefix, chunks, suffix) {

  // Fetch the buffers in parallel.
  const buffers = {};
  const fetches = [];
  for (const chunk of chunks) {
    fetches.push(fetchWithXHR(prefix + chunk + suffix).then(((c, x) => buffers[c] = x).bind(null, chunk)));
  }

  // Load them in series, as required per spec.
  return Promise.all(fetches).then(function() {
    let rv = Promise.resolve();
    for (const chunk of chunks) {
      rv = rv.then(loadSegmentAsync.bind(null, sb, buffers[chunk]));
    }
    return rv;
  });
}

// Register timeout function to dump debugging logs.
SimpleTest.registerTimeoutFunction(function() {
  for (const v of document.getElementsByTagName("video")) {
    v.mozDumpDebugInfo();
  }
  for (const a of document.getElementsByTagName("audio")) {
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
