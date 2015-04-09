const KEYSYSTEM_TYPE = "org.w3.clearkey";

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

function TimeStamp(token) {
  function pad(x) {
    return (x < 10) ? "0" + x : x;
  }
  var now = new Date();
  var ms = now.getMilliseconds();
  var time = "[" +
             pad(now.getHours()) + ":" +
             pad(now.getMinutes()) + ":" +
             pad(now.getSeconds()) + "." +
             ms +
             "]" +
             (ms < 10 ? "  " : (ms < 100 ? " " : ""));
  return token ? (time + " " + token) : time;
}

function Log(token, msg) {
  info(TimeStamp(token) + " " + msg);
}

function MediaErrorCodeToString(code)
{
  switch (code) {
  case MediaError.MEDIA_ERROR_ABORTED         : return "MEDIA_ERROR_ABORTED";
  case MediaError.MEDIA_ERR_NETWORK           : return "MEDIA_ERR_NETWORK";
  case MediaError.MEDIA_ERR_DECODE            : return "MEDIA_ERR_DECODE";
  case MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED : return "MEDIA_ERR_SRC_NOT_SUPPORTED";
  default: return String(code);
  }
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
    + (sb
       ? ((sb.updating ? " updating," : "")
          + " buffered="
          + (sb.buffered ? TimeRangesToString(sb.buffered) : "?"))
       : " ?")
    + " }");
}

function SourceBufferListToString(sbl)
{
  return "SourceBufferList[" + sbl.map(SourceBufferToString).join(", ") + "]";
}

function UpdateSessionFunc(test, token, sessionType, resolve, reject) {
  return function(ev) {
    var msgStr = ArrayBufferToString(ev.message);
    var msg = JSON.parse(msgStr);

    Log(token, "Session[" + ev.target.sessionId + "], got message from CDM: " + msgStr);
    is(msg.type, sessionType, TimeStamp(token) + " key session type should match");
    ok(msg.kids, TimeStamp(token) + " message event should contain key ID array");

    var outKeys = [];

    for (var i = 0; i < msg.kids.length; i++) {
      var id64 = msg.kids[i];
      var idHex = Base64ToHex(msg.kids[i]).toLowerCase();
      var key = test.keys[idHex];

      if (key) {
        Log(token, "found key " + key + " for key id " + idHex);
        outKeys.push({
          "kty":"oct",
          "alg":"A128KW",
          "kid":id64,
          "k":HexToBase64(key)
        });
      } else {
        bail(token + " couldn't find key for key id " + idHex);
      }
    }

    var update = JSON.stringify({
      "keys" : outKeys,
      "type" : msg.type
    });
    Log(token, "Session[" + ev.target.sessionId + "], sending update message to CDM: " + update);

    ev.target.update(StringToArrayBuffer(update)).then(function() {
      Log(token, "Session[" + ev.target.sessionId + "] update ok!");
      resolve(ev.target);
    }).catch(function(reason) {
      bail(token + " Session[" + ev.target.sessionId + "] update failed")(reason);
      reject();
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

function BSD16(a)
{
  var c = 0x1234;
  var i = 0;
  var l = a.length;
  for (i = 0; i < l; i++) {
    c = (((((c >>> 1) + ((c & 1) << 15)) | 0) + (a[i] & 0xff)) & 0xffff) | 0;
  }
  return c;
};

function AppendTrack(test, ms, track, token)
{
  return new Promise(function(resolve, reject) {
    var sb;
    var curFragment = 0;
    var resolved = false;
    var fragmentFile;
    var appendBufferTimer = null;

    function addNextFragment() {
      if (curFragment >= track.fragments.length) {
        Log(token, track.name + ": end of track");
        resolved = true;
        sb.removeEventListener("updateend", handleUpdateEnd);
        resolve();
        return;
      }

      var fragment = track.fragments[curFragment++];
      if (typeof fragment === "string") {
        fragment = { file:fragment, size:-1, bsd16:-1 };
      }

      fragmentFile = MaybeCrossOriginURI(test, fragment.file);

      var req = new XMLHttpRequest();
      req.open("GET", fragmentFile);
      req.responseType = "arraybuffer";
      req.timeout = 10 * 1000; // 10s should be plenty of time to get small fragments!

      req.addEventListener("load", function() {
        var u8array = new Uint8Array(req.response);
        if (fragment.size !== undefined && fragment.size >= 0) {
          is(u8array.length, fragment.size,
             token + " fragment '" + fragmentFile + "' size: expected "
             + fragment.size + ", got " + u8array.length);
        }
        if (fragment.bsd16 !== undefined && fragment.bsd16 >= 0) {
          is(BSD16(u8array), fragment.bsd16,
             token + " fragment '" + fragmentFile + "' checksum: expected "
             + fragment.bsd16 + ", got " + BSD16(u8array));
        }
        Log(token, track.name + ": fetch of " + fragmentFile + " complete, appending");
        appendBufferTimer = setTimeout(function () {
          if (!appendBufferTimer) { return; }
          sb.removeEventListener("updateend", handleUpdateEnd);
          reject("Timeout appendBuffer with fragment '" + fragmentFile + "'");
        }, 10 * 1000);
        sb.appendBuffer(u8array);
      });

      ["error", "abort", "timeout"
      ].forEach(function(issue) {
        req.addEventListener(issue, function() {
          info(token + " " + issue + " fetching " + fragmentFile + ", status='" + req.statusText + "'");
          resolved = true;
          reject(issue + " fetching " + fragmentFile);
        });
      });

      Log(token, track.name + ": addNextFragment() fetching next fragment " + fragmentFile);
      req.send(null);
    }

    function handleUpdateEnd() {
      if (appendBufferTimer) {
        clearTimeout(appendBufferTimer);
        appendBufferTimer = null;
      }
      if (ms.readyState == "ended") {
        /* We can get another updateevent as a result of calling ms.endOfStream() if
           the highest end time of our source buffers is different from that of the
           media source duration. Due to bug 1065207 this can happen because of
           inaccuracies in the frame duration calculations. Check if we are already
           "ended" and ignore the update event */
        Log(token, track.name + ": updateend when readyState already 'ended'");
        if (!resolved) {
          // Needed if decoder knows this was the last fragment and ended by itself.
          Log(token, track.name + ": but promise not resolved yet -> end of track");
          resolved = true;
          resolve();
        }
        return;
      }
      Log(token, track.name + ": updateend for " + fragmentFile + ", " + SourceBufferToString(sb));
      addNextFragment();
    }

    Log(token, track.name + ": addSourceBuffer(" + track.type + ")");
    sb = ms.addSourceBuffer(track.type);

    sb.addEventListener("updateend", handleUpdateEnd);
    addNextFragment();
  });
}

//Returns a promise that is resolved when the media element is ready to have
//its play() function called; when it's loaded MSE fragments.
function LoadTest(test, elem, token)
{
  if (!test.tracks) {
    ok(false, token + " test does not have a tracks list");
    return Promise.reject();
  }

  var ms = new MediaSource();
  elem.src = URL.createObjectURL(ms);

  return new Promise(function (resolve, reject) {
    var firstOpen = true;
    ms.addEventListener("sourceopen", function () {
      if (!firstOpen) {
        Log(token, "sourceopen again?");
        return;
      }

      firstOpen = false;
      Log(token, "sourceopen");
      return Promise.all(test.tracks.map(function(track) {
        return AppendTrack(test, ms, track, token);
      })).then(function(){
        Log(token, "end of stream");
        ms.endOfStream();
        resolve();
      });
    })
  });
}

// Same as LoadTest, but manage a token+"_load" start&finished.
// Also finish main token if loading fails.
function LoadTestWithManagedLoadToken(test, elem, manager, token)
{
  manager.started(token + "_load");
  return LoadTest(test, elem, token)
  .catch(function (reason) {
    ok(false, TimeStamp(token) + " - Error during load: " + reason);
    manager.finished(token + "_load");
    manager.finished(token);
  })
  .then(function () {
    manager.finished(token + "_load");
  });
}

function SetupEME(test, token, params)
{
  var v = document.createElement("video");
  v.crossOrigin = test.crossOrigin || false;

  // Log events dispatched to make debugging easier...
  [ "canplay", "canplaythrough", "ended", "error", "loadeddata",
    "loadedmetadata", "loadstart", "pause", "play", "playing", "progress",
    "stalled", "suspend", "waiting",
  ].forEach(function (e) {
    v.addEventListener(e, function(event) {
      Log(token, "" + e);
    }, false);
  });

  // Finish the test when error is encountered.
  v.onerror = bail(token + " got error event");

  var onSetKeysFail = (params && params.onSetKeysFail)
    ? params.onSetKeysFail
    : bail(token + " Failed to set MediaKeys on <video> element");

  var firstEncrypted = true;

  v.addEventListener("encrypted", function(ev) {
    if (!firstEncrypted) {
      // TODO: Better way to handle 'encrypted'?
      //       Maybe wait for metadataloaded and all expected 'encrypted's?
      Log(token, "got encrypted event again, initDataType=" + ev.initDataType);
      return;
    }
    firstEncrypted = false;

    Log(token, "got encrypted event, initDataType=" + ev.initDataType);
    var options = [
      {
        initDataType: ev.initDataType,
        videoType: test.type,
        audioType: test.type,
      }
    ];

    function chain(promise, onReject) {
      return promise.then(function(value) {
        return Promise.resolve(value);
      }).catch(function(reason) {
        onReject(reason);
        return Promise.reject();
      })
    }

    var p = navigator.requestMediaKeySystemAccess(KEYSYSTEM_TYPE, options);
    var r = bail(token + " Failed to request key system access.");
    chain(p, r)
    .then(function(keySystemAccess) {
      var p = keySystemAccess.createMediaKeys();
      var r = bail(token +  " Failed to create MediaKeys object");
      return chain(p, r);
    })

    .then(function(mediaKeys) {
      Log(token, "created MediaKeys object ok");
      mediaKeys.sessions = [];
      var p = v.setMediaKeys(mediaKeys);
      return chain(p, onSetKeysFail);
    })

    .then(function() {
      Log(token, "set MediaKeys on <video> element ok");
      var sessionType = (params && params.sessionType) ? params.sessionType : "temporary";
      var session = v.mediaKeys.createSession(sessionType);
      if (params && params.onsessioncreated) {
        params.onsessioncreated(session);
      }

      return new Promise(function (resolve, reject) {
        session.addEventListener("message", UpdateSessionFunc(test, token, sessionType, resolve, reject));
        session.generateRequest(ev.initDataType, ev.initData).catch(function(reason) {
          // Reject the promise if generateRequest() failed. Otherwise it will
          // be resolve in UpdateSessionFunc().
          bail(token + ": session.generateRequest failed")(reason);
          reject();
        });
      });
    })

    .then(function(session) {
      Log(token, ": session.generateRequest succeeded");
      if (params && params.onsessionupdated) {
        params.onsessionupdated(session);
      }
    });
  });
  return v;
}
