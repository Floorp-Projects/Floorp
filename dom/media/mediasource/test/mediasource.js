// Helpers for Media Source Extensions tests

let gMSETestPrefs = [
  ["media.mediasource.enabled", true],
  ["media.audio-max-decode-error", 0],
  ["media.video-max-decode-error", 0],
];

// Called before runWithMSE() to set the prefs before running MSE tests.
function addMSEPrefs(...prefs) {
  gMSETestPrefs = gMSETestPrefs.concat(prefs);
}

async function runWithMSE(testFunction) {
  await once(window, "load");
  await SpecialPowers.pushPrefEnv({"set": gMSETestPrefs});

  const ms = new MediaSource();

  const el = document.createElement("video");
  el.src = URL.createObjectURL(ms);
  el.preload = "auto";

  document.body.appendChild(el);
  SimpleTest.registerCleanupFunction(() => {
    el.remove();
    el.removeAttribute("src");
    el.load();
  });
  try {
    await testFunction(ms, el);
  } catch (e) {
    ok(false, `${testFunction.name} failed with error ${e.name}`);
    throw e;
  }
}

async function fetchWithXHR(uri, onLoadFunction) {
  let result = await new Promise(resolve => {
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
    result = await onLoadFunction(result);
  }
  return result;
}

function range(start, end) {
  const rv = [];
  for (let i = start; i < end; ++i) {
    rv.push(i);
  }
  return rv;
}

function must_throw(f, msg, error = true) {
  try {
    f();
    ok(!error, msg);
  } catch (e) {
    ok(error, msg);
    if (error === true) {
      ok(false, `Please provide name of expected error! Got ${e.name}: ${e.message}.`);
    } else if (e.name != error) {
      throw e;
    }
  }
}

async function must_reject(f, msg, error = true) {
  try {
    await f();
    ok(!error, msg);
  } catch (e) {
    ok(error, msg);
    if (error === true) {
      ok(false, `Please provide name of expected error! Got ${e.name}: ${e.message}.`);
    } else if (e.name != error) {
      throw e;
    }
  }
}

const must_not_throw = (f, msg) => must_throw(f, msg, false);
const must_not_reject = (f, msg) => must_reject(f, msg, false);

async function once(target, name, cb) {
  let result = await new Promise(r => target.addEventListener(name, r, {once: true}));
  if (cb) {
    result = await cb();
  }
  return result;
}

function timeRangeToString(r) {
  let str = "TimeRanges: ";
  for (let i = 0; i < r.length; i++) {
    str += "[" + r.start(i) + ", " + r.end(i) + ")";
  }
  return str;
}

async function loadSegment(sb, typedArrayOrArrayBuffer) {
  const typedArray = (typedArrayOrArrayBuffer instanceof ArrayBuffer) ? new Uint8Array(typedArrayOrArrayBuffer)
                                                                      : typedArrayOrArrayBuffer;
  info(`Loading buffer: [${typedArray.byteOffset}, ${typedArray.byteOffset + typedArray.byteLength})`);
  const beforeBuffered = timeRangeToString(sb.buffered);
  const p = once(sb, "update");
  sb.appendBuffer(typedArray);
  await p;
  const afterBuffered = timeRangeToString(sb.buffered);
  info(`SourceBuffer buffered ranges grew from ${beforeBuffered} to ${afterBuffered}`);
}

async function fetchAndLoad(sb, prefix, chunks, suffix) {

  // Fetch the buffers in parallel.
  const buffers = await Promise.all(chunks.map(c => fetchWithXHR(prefix + c + suffix)));

  // Load them in series, as required per spec.
  for (const buffer of buffers) {
    await loadSegment(sb, buffer);
  }
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

async function waitUntilTime(target, targetTime) {
  await new Promise(resolve => {
    target.addEventListener("waiting", function onwaiting() {
      info("Got a waiting event at " + target.currentTime);
      if (target.currentTime >= targetTime) {
        target.removeEventListener("waiting", onwaiting);
        resolve();
      }
    });
  });
  ok(true, "Reached target time of: " + targetTime);
}

// Log events for debugging.

function logEvents(el) {
  ["suspend", "play", "canplay", "canplaythrough", "loadstart", "loadedmetadata",
   "loadeddata", "playing", "ended", "error", "stalled", "emptied", "abort",
   "waiting", "pause", "durationchange", "seeking",
   "seeked"].forEach(type => el.addEventListener(type, e => info(`got ${e.type} event`)));
}
