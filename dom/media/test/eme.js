const KEYSYSTEM_TYPE = "org.w3.clearkey";

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

function UpdateSessionFunc(test, token, sessionType, resolve, reject) {
  return function(ev) {
    var msgStr = ArrayBufferToString(ev.message);
    var msg = JSON.parse(msgStr);

    Log(token, "got message from CDM: " + msgStr);
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
    Log(token, "sending update message to CDM: " + update);

    ev.target.update(StringToArrayBuffer(update)).then(function() {
      Log(token, "MediaKeySession update ok!");
      resolve(ev.target);
    }).catch(function(reason) {
      bail(token + " MediaKeySession update failed")(reason);
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

function AppendTrack(test, ms, track, token, loadParams)
{
  return new Promise(function(resolve, reject) {
    var sb;
    var curFragment = 0;
    var resolved = false;
    var fragments = track.fragments;
    var fragmentFile;

    if (loadParams && loadParams.onlyLoadFirstFragments) {
      fragments = fragments.slice(0, loadParams.onlyLoadFirstFragments);
    }

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

      req.addEventListener("error", function(){info(token + " error fetching " + fragmentFile);});
      req.addEventListener("abort", function(){info(token + " aborted fetching " + fragmentFile);});

      Log(token, track.name + ": addNextFragment() fetching next fragment " + fragmentFile);
      req.send(null);
    }

    Log(token, track.name + ": addSourceBuffer(" + track.type + ")");
    sb = ms.addSourceBuffer(track.type);
    sb.addEventListener("updateend", function() {
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
          resolve();
          resolved = true;
        }
        return;
      }
      Log(token, track.name + ": updateend for " + fragmentFile + ", " + SourceBufferToString(sb));
      addNextFragment();
    });

    addNextFragment();
  });
}

//Returns a promise that is resolved when the media element is ready to have
//its play() function called; when it's loaded MSE fragments.
function LoadTest(test, elem, token, loadParams)
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
        return AppendTrack(test, ms, track, token, loadParams);
      })).then(function(){
        if (loadParams && loadParams.noEndOfStream) {
          Log(token, "Tracks loaded");
        } else {
          Log(token, "Tracks loaded, calling MediaSource.endOfStream()");
          ms.endOfStream();
        }
        resolve();
      });
    })
  });
}

// Same as LoadTest, but manage a token+"_load" start&finished.
// Also finish main token if loading fails.
function LoadTestWithManagedLoadToken(test, elem, manager, token, loadParams)
{
  manager.started(token + "_load");
  return LoadTest(test, elem, token, loadParams)
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
  v.sessions = [];

  v.closeSessions = function() {
    return Promise.all(v.sessions.map(s => s.close().then(() => s.closed))).then(
      () => {
        v.setMediaKeys(null);
        if (v.parentNode) {
          v.parentNode.removeChild(v);
        }
        v.onerror = null;
        v.src = null;
      });
  };

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

  // null: No session management in progress, just go ahead and update the session.
  // [...]: Session management in progress, add [initDataType, initData] to
  //        this queue to get it processed when possible.
  var initDataQueue = [];
  function processInitDataQueue()
  {
    if (initDataQueue === null) { return; }
    if (initDataQueue.length === 0) { initDataQueue = null; return; }
    var ev = initDataQueue.shift();

    var sessionType = (params && params.sessionType) ? params.sessionType : "temporary";
    Log(token, "createSession(" + sessionType + ") for (" + ev.initDataType + ", " + StringToHex(ArrayBufferToString(ev.initData)) + ")");
    var session = v.mediaKeys.createSession(sessionType);
    if (params && params.onsessioncreated) {
      params.onsessioncreated(session);
    }
    v.sessions.push(session);

    return new Promise(function (resolve, reject) {
      session.addEventListener("message", UpdateSessionFunc(test, token, sessionType, resolve, reject));
      Log(token, "session[" + session.sessionId + "].generateRequest(" + ev.initDataType + ", " + StringToHex(ArrayBufferToString(ev.initData)) + ")");
      session.generateRequest(ev.initDataType, ev.initData).catch(function(reason) {
        // Reject the promise if generateRequest() failed. Otherwise it will
        // be resolve in UpdateSessionFunc().
        bail(token + ": session[" + session.sessionId + "].generateRequest(" + ev.initDataType + ", " + StringToHex(ArrayBufferToString(ev.initData)) + ") failed")(reason);
        reject();
      });
    })

    .then(function(aSession) {
      Log(token, "session[" + session.sessionId + "].generateRequest(" + ev.initDataType + ", " + StringToHex(ArrayBufferToString(ev.initData)) + ") succeeded");
      if (params && params.onsessionupdated) {
        params.onsessionupdated(aSession);
      }
      processInitDataQueue();
    });
  }

  // All 'initDataType's should be the same.
  // null indicates no 'encrypted' event received yet.
  var initDataType = null;
  v.addEventListener("encrypted", function(ev) {
    if (initDataType === null) {
      Log(token, "got first encrypted(" + ev.initDataType + ", " + StringToHex(ArrayBufferToString(ev.initData)) + "), setup session");
      initDataType = ev.initDataType;
      initDataQueue.push(ev);

      function chain(promise, onReject) {
        return promise.then(function(value) {
          return Promise.resolve(value);
        }).catch(function(reason) {
          onReject(reason);
          return Promise.reject();
        })
      }

      var options = [
         {
           initDataType: ev.initDataType,
           videoType: test.type,
           audioType: test.type,
         }
       ];
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
        processInitDataQueue();
      })
    } else {
      if (ev.initDataType !== initDataType) {
        return bail(token + ": encrypted(" + ev.initDataType + ", " +
                    StringToHex(ArrayBufferToString(ev.initData)) + ")")
                   ("expected " + initDataType);
      }
      if (initDataQueue !== null) {
        Log(token, "got encrypted(" + ev.initDataType + ", " + StringToHex(ArrayBufferToString(ev.initData)) + ") event, queue it for later session update");
        initDataQueue.push(ev);
      } else {
        Log(token, "got encrypted(" + ev.initDataType + ", " + StringToHex(ArrayBufferToString(ev.initData)) + ") event, update session now");
        initDataQueue = [ev];
        processInitDataQueue();
      }
    }
  });
  return v;
}

function SetupEMEPref(callback) {
  var prefs = [
    [ "media.mediasource.enabled", true ],
    [ "media.fragmented-mp4.exposed", true ],
    [ "media.eme.apiVisible", true ],
  ];

  if (/Linux/.test(manifestNavigator().userAgent)) {
    prefs.push([ "media.fragmented-mp4.ffmpeg.enabled", true ]);
  } else if (SpecialPowers.Services.appinfo.name == "B2G" ||
             !manifestVideo().canPlayType("video/mp4")) {
   // XXX remove once we have mp4 PlatformDecoderModules on all platforms.
   prefs.push([ "media.fragmented-mp4.use-blank-decoder", true ]);
  }

  SpecialPowers.pushPrefEnv({ "set" : prefs }, callback);
}
