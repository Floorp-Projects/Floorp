const CLEARKEY_KEYSYSTEM = "org.w3.clearkey";

const gCencMediaKeySystemConfig = [{
  initDataTypes: ['cenc'],
  videoCapabilities: [{ contentType: 'video/mp4' }],
  audioCapabilities: [{ contentType: 'audio/mp4' }],
}];

function IsMacOSSnowLeopardOrEarlier() {
  var re = /Mac OS X (\d+)\.(\d+)/;
  var ver = navigator.userAgent.match(re);
  if (!ver || ver.length != 3) {
    return false;
  }
  var major = ver[1] | 0;
  var minor = ver[2] | 0;
  return major == 10 && minor <= 6;
}

function bail(message)
{
  return function(err) {
    if (err) {
      message +=  "; " + String(err)
    }
    ok(false, message);
    if (err) {
      info(String(err));
    }
    SimpleTest.finish();
  }
}

function ArrayBufferToString(arr)
{
  var str = '';
  var view = new Uint8Array(arr);
  for (var i = 0; i < view.length; i++) {
    str += String.fromCharCode(view[i]);
  }
  return str;
}

function StringToArrayBuffer(str)
{
  var arr = new ArrayBuffer(str.length);
  var view = new Uint8Array(arr);
  for (var i = 0; i < str.length; i++) {
    view[i] = str.charCodeAt(i);
  }
  return arr;
}

function StringToHex(str){
  var res = "";
  for (var i = 0; i < str.length; ++i) {
      res += ("0" + str.charCodeAt(i).toString(16)).slice(-2);
  }
  return res;
}

function Base64ToHex(str)
{
  var bin = window.atob(str.replace(/-/g, "+").replace(/_/g, "/"));
  var res = "";
  for (var i = 0; i < bin.length; i++) {
    res += ("0" + bin.charCodeAt(i).toString(16)).substr(-2);
  }
  return res;
}

function HexToBase64(hex)
{
  var bin = "";
  for (var i = 0; i < hex.length; i += 2) {
    bin += String.fromCharCode(parseInt(hex.substr(i, 2), 16));
  }
  return window.btoa(bin).replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");
}

function TimeRangesToString(trs)
{
  var l = trs.length;
  if (l === 0) { return "-"; }
  var s = "";
  var i = 0;
  for (;;) {
    s += trs.start(i) + "-" + trs.end(i);
    if (++i === l) { return s; }
    s += ",";
  }
}

function SourceBufferToString(sb)
{
  return ("SourceBuffer{"
    + "AppendMode=" + (sb.AppendMode || "-")
    + ", updating=" + (sb.updating ? "true" : "false")
    + ", buffered=" + TimeRangesToString(sb.buffered)
    + ", audioTracks=" + (sb.audioTracks ? sb.audioTracks.length : "-")
    + ", videoTracks=" + (sb.videoTracks ? sb.videoTracks.length : "-")
    + "}");
}

function SourceBufferListToString(sbl)
{
  return "SourceBufferList[" + sbl.map(SourceBufferToString).join(", ") + "]";
}

function GenerateClearKeyLicense(licenseRequest, keyStore)
{
  var msgStr = ArrayBufferToString(licenseRequest);
  var msg = JSON.parse(msgStr);

  var keys = [];
  for (var i = 0; i < msg.kids.length; i++) {
    var id64 = msg.kids[i];
    var idHex = Base64ToHex(msg.kids[i]).toLowerCase();
    var key = keyStore[idHex];

    if (key) {
      keys.push({
        "kty": "oct",
        "kid": id64,
        "k": HexToBase64(key)
      });
    }
  }

  return new TextEncoder().encode(JSON.stringify({
    "keys" : keys,
    "type" : msg.type || "temporary"
  }));
}

function UpdateSessionFunc(test, token, sessionType, resolve, reject) {
  return function(ev) {
    var license = GenerateClearKeyLicense(ev.message, test.keys);
    Log(token, "sending update message to CDM: " + (new TextDecoder().decode(license)));
    ev.target.update(license).then(function() {
      Log(token, "MediaKeySession update ok!");
      resolve(ev.target);
    }).catch(function(reason) {
      reject(`${token} MediaKeySession update failed: ${reason}`);
    });
  }
}

function MaybeCrossOriginURI(test, uri)
{
  if (test.crossOrigin) {
    return "http://test2.mochi.test:8888/tests/dom/media/test/allowed.sjs?" + uri;
  } else {
    return uri;
  }
}

function AppendTrack(test, ms, track, token)
{
  return new Promise(function(resolve, reject) {
    var sb;
    var curFragment = 0;
    var resolved = false;
    var fragments = track.fragments;
    var fragmentFile;

    function addNextFragment() {
      if (curFragment >= fragments.length) {
        Log(token, track.name + ": end of track");
        resolve();
        resolved = true;
        return;
      }

      fragmentFile = MaybeCrossOriginURI(test, fragments[curFragment++]);

      var req = new XMLHttpRequest();
      req.open("GET", fragmentFile);
      req.responseType = "arraybuffer";

      req.addEventListener("load", function() {
        Log(token, track.name + ": fetch of " + fragmentFile + " complete, appending");
        sb.appendBuffer(new Uint8Array(req.response));
      });

      req.addEventListener("error", function() {
        reject(`${token} - ${track.name}: error fetching ${fragmentFile}`);
      });
      req.addEventListener("abort", function() {
        reject(`${token} - ${track.name}: aborted fetching ${fragmentFile}`);
      });

      Log(token, track.name + ": addNextFragment() fetching next fragment " + fragmentFile);
      req.send(null);
    }

    Log(token, track.name + ": addSourceBuffer(" + track.type + ")");
    sb = ms.addSourceBuffer(track.type);
    sb.addEventListener("updateend", function() {
      Log(token, track.name + ": updateend for " + fragmentFile + ", " + SourceBufferToString(sb));
      addNextFragment();
    });

    addNextFragment();
  });
}

//Returns a promise that is resolved when the media element is ready to have
//its play() function called; when it's loaded MSE fragments.
function LoadTest(test, elem, token, endOfStream = true)
{
  if (!test.tracks) {
    ok(false, token + " test does not have a tracks list");
    return Promise.reject();
  }

  var ms = new MediaSource();
  elem.src = URL.createObjectURL(ms);
  elem.crossOrigin = test.crossOrigin || false;

  return new Promise(function (resolve, reject) {
    ms.addEventListener("sourceopen", function () {
      Log(token, "sourceopen");
      Promise.all(test.tracks.map(function(track) {
        return AppendTrack(test, ms, track, token);
      })).then(function() {
        Log(token, "Tracks loaded, calling MediaSource.endOfStream()");
        if (endOfStream) {
          ms.endOfStream();
        }
        resolve();
      }).catch(reject);
    }, {once: true});
  });
}

function EMEPromise() {
  var self = this;
  self.promise = new Promise(function(resolve, reject) {
    self.resolve = resolve;
    self.reject = reject;
  });
}

/*
 * Create a new MediaKeys object.
 * Return a promise which will be resolved with a new MediaKeys object,
 * or will be rejected with a string that describes the failure.
 */
function CreateMediaKeys(v, test, token) {
  let p = new EMEPromise;

  function streamType(type) {
    var x = test.tracks.find(o => o.name == type);
    return x ? x.type : undefined;
  }

  function onencrypted(ev) {
    var options = { initDataTypes: [ev.initDataType] };
    if (streamType("video")) {
      options.videoCapabilities = [{contentType: streamType("video")}];
    }
    if (streamType("audio")) {
      options.audioCapabilities = [{contentType: streamType("audio")}];
    }
    navigator.requestMediaKeySystemAccess(CLEARKEY_KEYSYSTEM, [options])
    .then(keySystemAccess => {
      keySystemAccess.createMediaKeys().then(
        p.resolve,
        () => p.reject(`${token} Failed to create MediaKeys object.`)
      );
    }, () => p.reject(`${token} Failed to request key system access.`));
  }

  v.addEventListener("encrypted", onencrypted, {once: true});
  return p.promise;
}

/*
 * Create a new MediaKeys object and provide it to the media element.
 * Return a promise which will be resolved if succeeded, or will be rejected
 * with a string that describes the failure.
 */
function CreateAndSetMediaKeys(v, test, token) {
  let p = new EMEPromise;

  CreateMediaKeys(v, test, token).then(mediaKeys => {
    v.setMediaKeys(mediaKeys).then(
      p.resolve,
      () => p.reject(`${token} Failed to set MediaKeys on <video> element.`)
    );
  }, p.reject)

  return p.promise;
}

/*
 * Collect the init data from 'encrypted' events.
 * Return a promise which will be resolved with the init data when collection
 * is completed (specified by test.sessionCount).
 */
function LoadInitData(v, test, token) {
  let p = new EMEPromise;
  let initDataQueue = [];

  // Call SimpleTest._originalSetTimeout() to bypass the flaky timeout checker.
  let timer = SimpleTest._originalSetTimeout.call(window, () => {
    p.reject(`${token} Timed out in waiting for the init data.`);
  }, 60000);

  function onencrypted(ev) {
    initDataQueue.push(ev);
    Log(token, `got encrypted(${ev.initDataType}, ` +
        `${StringToHex(ArrayBufferToString(ev.initData))}) event.`);
    if (test.sessionCount == initDataQueue.length) {
      p.resolve(initDataQueue);
      clearTimeout(timer);
    }
  }

  v.addEventListener("encrypted", onencrypted);
  return p.promise;
}

/*
 * Generate a license request and update the session.
 * Return a promsise which will be resolved with the updated session
 * or rejected with a string that describes the failure.
 */
function MakeRequest(test, token, ev, session, sessionType) {
  sessionType = sessionType || "temporary";
  let p = new EMEPromise;
  let str = `session[${session.sessionId}].generateRequest(` +
    `${ev.initDataType}, ${StringToHex(ArrayBufferToString(ev.initData))})`;

  session.addEventListener("message",
    UpdateSessionFunc(test, token, sessionType, p.resolve, p.reject));

  Log(token, str);
  session.generateRequest(ev.initDataType, ev.initData)
  .catch(reason => {
    // Reject the promise if generateRequest() failed.
    // Otherwise it will be resolved in UpdateSessionFunc().
    p.reject(`${token}: ${str} failed; ${reason}`);
  });

  return p.promise;
}

/*
 * Process the init data by calling MakeRequest().
 * Return a promise which will be resolved with the updated sessions
 * when all init data are processed or rejected if any failure.
 */
function ProcessInitData(v, test, token, initData, sessionType) {
  return Promise.all(
    initData.map(ev => {
      let session = v.mediaKeys.createSession(sessionType);
      return MakeRequest(test, token, ev, session, sessionType);
    })
  );
}

/*
 * Clean up the |v| element.
 */
function CleanUpMedia(v) {
  v.setMediaKeys(null);
  v.remove();
  v.removeAttribute("src");
  v.load();
}

/*
 * Close all sessions and clean up the |v| element.
 */
function CloseSessions(v, sessions) {
  return Promise.all(sessions.map(s => s.close()))
  .then(CleanUpMedia(v));
}

/*
 * Set up media keys and source buffers for the media element.
 * Return a promise resolved when all key sessions are updated or rejected
 * if any failure.
 */
function SetupEME(v, test, token) {
  let p = new EMEPromise;

  v.onerror = function() {
    p.reject(`${token} got an error event.`);
  }

  Promise.all([
    LoadInitData(v, test, token),
    CreateAndSetMediaKeys(v, test, token),
    LoadTest(test, v, token)])
  .then(values => {
    let initData = values[0];
    return ProcessInitData(v, test, token, initData);
  })
  .then(p.resolve, p.reject);

  return p.promise;
}

function SetupEMEPref(callback) {
  var prefs = [
    [ "media.mediasource.enabled", true ],
    [ "media.mediasource.webm.enabled", true ],
    [ "media.eme.vp9-in-mp4.enabled", true ],
  ];

  if (SpecialPowers.Services.appinfo.name == "B2G" ||
      !manifestVideo().canPlayType("video/mp4")) {
    // XXX remove once we have mp4 PlatformDecoderModules on all platforms.
    prefs.push([ "media.use-blank-decoder", true ]);
  }

  SpecialPowers.pushPrefEnv({ "set" : prefs }, callback);
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

function once(target, name, cb) {
  var p = new Promise(function(resolve, reject) {
    target.addEventListener(name, function(arg) {
      resolve(arg);
    }, {once: true});
  });
  if (cb) {
    p.then(cb);
  }
  return p;
}
