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
  if (!sb) {
    return "?";
  }
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

function AppendTrack(test, ms, track, token)
{
  return new Promise(function(resolve, reject) {
    var sb;
    var curFragment = 0;
    var resolved = false;
    var fragmentFile;

    var appendBufferTimer = false;

    var addNextFragment = function() {
      if (curFragment >= track.fragments.length) {
        Log(token, track.name + ": end of track");
        resolved = true;
        sb.removeEventListener("updateend", handleUpdateEnd);
        resolve();
        return;
      }

      fragmentFile = MaybeCrossOriginURI(test, track.fragments[curFragment++]);

      var req = new XMLHttpRequest();
      req.open("GET", fragmentFile);
      req.responseType = "arraybuffer";
      req.timeout = 10 * 1000; // 10s should be plenty of time to get small fragments!

      req.addEventListener("load", function() {
        Log(token, track.name + ": fetch of " + fragmentFile + " complete, appending");
        appendBufferTimer = setTimeout(function () {
          reject("Timeout appendBuffer with fragment '" + fragmentFile + "'");
        }, 10 * 1000);
        sb.appendBuffer(new Uint8Array(req.response));
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

    if (!ms._test_sourceBuffers || !ms._test_sourceBuffers[track.name]) {
      Log(token, track.name + ": addSourceBuffer(" + track.type + ")");
      sb = ms.addSourceBuffer(track.type);
      ms._test_sourceBuffers[track.name] = sb;
    } else {
      Log(token, track.name + ": reusing SourceBuffer(" + track.type + ")");
      sb = ms._test_sourceBuffers[track.name];
    }

    var handleUpdateEnd = function() {
      if (appendBufferTimer) { clearTimeout(appendBufferTimer); appendBufferTimer = false; }
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
          sb.removeEventListener("updateend", handleUpdateEnd);
          resolve();
        }
        return;
      }
      Log(token, track.name + ": updateend for " + fragmentFile + ", " + SourceBufferToString(sb));
      addNextFragment();
    }

    sb.addEventListener("updateend", handleUpdateEnd);

    addNextFragment();
  });
}

var LOAD_TEST_ALL = 0;
var LOAD_TEST_INIT = 1;
var LOAD_TEST_DATA = 2;
// Returns a promise that is resolved when the media element is ready to have
// its play() function called; when it's loaded MSE fragments.
function LoadTest(test, elem, token, loadWhat = LOAD_TEST_ALL)
{
  if (!test.tracks) {
    ok(false, token + " test does not have a tracks list");
    return Promise.reject();
  }

  return new Promise(function (resolve, reject) {
    var ms;

    function AppendTracks() {
      return Promise.all(test.tracks.map(function(track) {
        if (loadWhat === LOAD_TEST_INIT) {
          track = { name:track.name,
                    type:track.type,
                    fragments:track.fragments.filter(x => x.indexOf("init") >= 0) };
          Log(token, track.name + ": Load init: " + track.fragments);
        } else if (loadWhat === LOAD_TEST_DATA) {
          track = { name:track.name,
                    type:track.type,
                    fragments:track.fragments.filter(x => x.indexOf("init") < 0) };
          Log(token, track.name + ": Load data: " + track.fragments);
        } else {
          Log(token, track.name + ": Load everything: " + track.fragments);
        }
        return AppendTrack(test, ms, track, token);
      })).then(function(){
        if (loadWhat !== LOAD_TEST_INIT) {
          Log(token, "end of stream");
          ms.endOfStream();
        } else {
          Log(token, "all init segments loaded");
        }
        resolve();
      });
    }

    if (!elem._test_MediaSource) {
      Log(token, "Create MediaSource");
      ms = new MediaSource();
      ms._test_sourceBuffers = {};
      elem._test_MediaSource = ms;
      elem.src = URL.createObjectURL(ms);
      var firstOpen = true;
      ms.addEventListener("sourceopen", function() {
        if (!firstOpen) {
          Log(token, "sourceopen again?");
          return;
        }
        firstOpen = false;
        Log(token, "sourceopen");
        AppendTracks();
      });
    } else {
      Log(token, "Reuse MediaSource");
      ms = elem._test_MediaSource;
      AppendTracks();
    }
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
      if (params && params.onsessionupdated) {
        params.onsessionupdated(null);
      }
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
      Log(token, "session[" + session.sessionId + "].generateRequest succeeded");
      if (params && params.onsessionupdated) {
        params.onsessionupdated(session);
      }
    });
  });
  return v;
}

// Setup an EME test, return the media element.
function SetupControlledEME(test, token, params)
{
  var v = document.createElement("video");
  v._test_test = test;
  v._test_token = token;
  v._test_params = params || {};

  v.crossOrigin = test.crossOrigin || false;

  // Log events dispatched to make debugging easier...
  [ "canplay", "canplaythrough", "ended", "loadeddata",
    "loadedmetadata", "loadstart", "pause", "play", "playing", "progress",
    "stalled", "suspend", "waiting",
  ].forEach(function (e) {
    v.addEventListener(e, function(event) {
      Log(token, "" + e);
    }, false);
  });
  v.addEventListener("error", function(ev) {
    Log(token, "error: " + MediaErrorCodeToString(ev.target.error.code));
  }, false);

  return v;
}

// Return a promise that is resolved when the media element is ready to have
// its play() function called; i.e., when it's loaded MSE fragments.
function LoadControlledEME(v)
{
  // Resolve promise when 'loadedmetadata' has been received.
  var promiseLoadedMetadata = new Promise(function (resolve, reject) {
    function HandleLoadedMetadataAgain(ev)
    {
      bail(v._test_token + " Got metadataloaded again!")(ev);
    }
    function HandleLoadedMetadata(ev)
    {
      v.removeEventListener("loadedmetadata", HandleLoadedMetadata);
      v.addEventListener("loadedmetadata", HandleLoadedMetadataAgain);
      resolve(ev);
    }
    v.addEventListener("loadedmetadata", HandleLoadedMetadata);
  });

  // Resolve promise when all 'encrypted' events have been fully handled,
  // including setting up sessions and updating keys.
  var promiseEncrypted = new Promise(function (resolve, reject) {
    var encrypteds = 0;
    var updated = 0;
    function HandleEncryptedAgain(ev)
    {
      bail(v._test_token + " Got too many 'encrypted' events!")(ev);
    }
    function HandleEncrypted(ev)
    {
      var sessionType = v._test_test.sessionType || "temporary";
      PromiseUpdatedSession(v, ev, v._test_test.keys, sessionType)
      .then(function () {
        updated += 1;
        if (updated === v._test_test.sessionCount) {
          resolve();
        }
      });
      encrypteds += 1;
      if (encrypteds === v._test_test.sessionCount) {
        v.removeEventListener("encrypted", HandleEncrypted);
        v.addEventListener("encrypted", HandleEncryptedAgain);
      }
    }
    v.addEventListener("encrypted", HandleEncrypted);
  });

  // Prepare MediaSource and SourceBuffers.
  return Promise.all(v._test_test.tracks.map(function (track) {
    return PromiseSourceBuffer(v, track.name, track.type);
  }))

  // Append init fragments for all tracks.
  .then(function () {
    Log(v._test_token, "MediaSource and all SourceBuffers ready, load init fragments");
    return Promise.all(v._test_test.tracks.map(function (track) {
      var fragments = track.fragments.filter(x => x.indexOf("init") >= 0);
      return PromiseAppendFragments(v, track.name, fragments, track.type);
    }));
  })

  // Wait for loadedmetadata and all up-to-date sessions.
  .then(function () {
    Log(v._test_token, "Init fragments loaded, wait for loadedmetadata and all session updates");
    return Promise.all([promiseLoadedMetadata, promiseEncrypted]);
  })

  // Append data fragments for all tracks.
  .then(function () {
    Log(v._test_token, "All sessions updated, load data fragments");
    return Promise.all(v._test_test.tracks.map(function (track) {
      var fragments = track.fragments.filter(x => x.indexOf("init") < 0);
      return PromiseAppendFragments(v, track.name, fragments, track.type);
    }));
  })

  .then(function() {
    Log(v._test_token, "Fragments loaded, endOfStream");
    return PromiseEndOfStream(v);
  })
}

// Return a promise that resolves into a MediaSource attached to the 'v' element.
function PromiseMediaSource(v)
{
  if (!v._test_mediaSourcePromise) {
    v._test_mediaSourcePromise = new Promise(function (resolve, reject) {
      Log(v._test_token, "Create MediaSource");
      var ms = new MediaSource();
      v.src = URL.createObjectURL(ms);
      ms._test_sourceBufferPromises = {};
      var firstOpen = true;
      ms.addEventListener("sourceopen", function() {
        if (!firstOpen) {
          Log(v._test_token, "sourceopen again?");
          return;
        }
        firstOpen = false;
        Log(v._test_token, "sourceopen");
        resolve(ms);
      });
    });
  }
  return v._test_mediaSourcePromise;
}

// Return a promise that resolves into a SourceBuffer.
function PromiseSourceBuffer(v, track, type)
{
  return PromiseMediaSource(v)
  .then(function(ms) {
    if (!ms._test_sourceBufferPromises[track]) {
      if (!type) {
        for (var i in v._test_test.tracks) {
          if (v._test_test.tracks[i].name === track) {
            type = v._test_test.tracks[i].type;
            break;
          }
        }
        if (!type) {
          return Promise.reject("No type provided for track '" + track + "'");
        }
      }
      ms._test_sourceBufferPromises[track] = new Promise(function(resolve, reject) {
        Log(v._test_token, track + ": addSourceBuffer(" + type + ")");
        var sb = ms.addSourceBuffer(type);
        resolve(sb);
      });
    }
    return ms._test_sourceBufferPromises[track];
  });
}

function PromiseXHRGet(v, fragmentFile)
{
  return new Promise(function (resolve, reject) {
    var req = new XMLHttpRequest();
    req.open("GET", fragmentFile);
    req.responseType = "arraybuffer";
    req.timeout = 10 * 1000; // 10s should be plenty of time to get small fragments!

    req.addEventListener("load", function() {
      if (req.status === 200) {
        Log(v._test_token, "completed fetching " + fragmentFile + ": " + req.statusText);
        return resolve(req);
      } else if (req.status === 206 && v._test_test.crossOrigin) {
        // When testing CORS, allowed.sjs returns 206.
        Log(v._test_token, "completed fetching " + fragmentFile + ": " + req.statusText);
        return resolve(req);
      } else {
        Log(v._test_token, "problem fetching " + fragmentFile + ", status='" + req.statusText + "'");
        return reject(req);
      }
    });

    ["error", "abort", "timeout"
    ].forEach(function(issue) {
      req.addEventListener(issue, function() {
        Log(v._test_token, issue + " fetching " + fragmentFile + ", status='" + req.statusText + "'");
        req.issue = issue;
        return reject(req);
      });
    });

    Log(v._test_token, "Fetching fragment " + fragmentFile);
    req.send(null);
  });
}

// Return a promise that resolves after fragments have been added to a SourceBuffer.
function PromiseAppendFragments(v, track, fragments, type)
{
  return PromiseSourceBuffer(v, track, type)
  .then(function (sb) {
    return new Promise(function(resolve, reject) {
      var curFragment = 0;
      var fragmentFile;

      var appendBufferTimer = false;

      var addNextFragment = function() {
        if (curFragment >= fragments.length) {
          sb.removeEventListener("updateend", handleUpdateEnd);
          return resolve();
        }

        fragmentFile = MaybeCrossOriginURI(v._test_test, fragments[curFragment++]);

        PromiseXHRGet(v, fragmentFile)
        .then(function(req) {
          Log(v._test_token, track + ": fetch of " + fragmentFile + " complete, appending");
          appendBufferTimer = setTimeout(function () {
            reject("Timeout appendBuffer with fragment '" + fragmentFile + "'");
          }, 10 * 1000);
          sb.appendBuffer(new Uint8Array(req.response));
        }, function(req) {
          if (appendBufferTimer) { clearTimeout(appendBufferTimer); appendBufferTimer = false; }
          sb.removeEventListener("updateend", handleUpdateEnd);
          return reject("Cannot load fragment '" + fragmentFile + "'");
        });
      }

      var handleUpdateEnd = function() {
        if (appendBufferTimer) { clearTimeout(appendBufferTimer); appendBufferTimer = false; }
        Log(v._test_token, track + ": updateend for " + fragmentFile + ", " + SourceBufferToString(sb));
        addNextFragment();
      }

      sb.addEventListener("updateend", handleUpdateEnd);

      addNextFragment();
    });
  });
}

function PromiseEndOfStream(v)
{
  return PromiseMediaSource(v)
  .then(function (ms) {
    Log(v._test_token, "end of stream");
    ms.endOfStream();
  });
}

// Return a promise that resolves into MediaKeys that have been attached to v.
function PromiseMediaKeys(v, initDataType, videoType, audioType)
{
  if (!v._test_mediaKeysPromise) {
    var createdMediaKeys;
    v._test_mediaKeysPromise = new Promise(function(resolve, reject) {
      Log(v._test_token, "Creating MediaKeys - 1. requestMediaKeySystemAccess("
          + "initDataType=" + initDataType
          + ", videoType=" + videoType
          + ", audioType=" + audioType + ")");
      // TODO: Revisit when bug 1134066 is fixed.
      var options = [ {
        initDataType: initDataType,
        videoType: v._test_test.type,
        audioType: v._test_test.type,
      } ];
      navigator.requestMediaKeySystemAccess(KEYSYSTEM_TYPE, options)
      .catch(function (reason) {
        bail(v._test_token + " Failed to request key system access.")(reason);
        return reject();
      })

      .then(function(keySystemAccess) {
        Log(v._test_token, "Creating MediaKeys - 2. createMediaKeys");
        return keySystemAccess.createMediaKeys();
      })
      .catch(function (reason) {
        bail(v._test_token +  " Failed to create MediaKeys object")(reason);
        return reject();
      })

      .then(function(mediaKeys) {
        Log(v._test_token, "Creating MediaKeys - 3. setMediaKeys on media element");
        createdMediaKeys = mediaKeys;
        mediaKeys.sessions = [];
        return v.setMediaKeys(mediaKeys);
      })
      .catch(function (reason) {
        if (v._test_params && v._test_params.onSetKeysFail) {
          v._test_params.onSetKeysFail();
        } else {
          bail(v._test_token + " Failed to set MediaKeys on <video> element");
        }
        return reject();
      })

      .then(function() {
        Log(v._test_token, "Creating MediaKeys - 4. Completed");
        resolve(createdMediaKeys);
      });
    });
  }
  return v._test_mediaKeysPromise;
}

// Return a promise when a session has been created and updated with the appropriate keys.
function PromiseUpdatedSession(v, ev, keys, sessionType)
{
  // TODO: Revisit when bug 1134066 is fixed.
  var videoType = "";
  var audioType = "";
  v._test_test.tracks.map(function (track) {
    if (track.name === "video") { videoType = track.type; }
    if (track.name === "audio") { audioType = track.type; }
  });

  return PromiseMediaKeys(v, ev.initDataType, videoType, audioType)
  .then(function (mk) {
    sessionType = sessionType || "temporary";
    var session = mk.createSession(sessionType);
    Log(v._test_token, "Created session[" + session.sessionId + "] type=" + sessionType);

    session._test_sessionType = sessionType;
    if (v._test_params && v._test_params.onsessioncreated) {
      v._test_params.onsessioncreated(session);
    }

    function UpdateSessionFunc(resolve, reject) {
      return function(ev) {
        var msgStr = ArrayBufferToString(ev.message);
        var msg = JSON.parse(msgStr);

        is(ev.target, session, TimeStamp(v._test_token) + " message target should be previously-created session");
        Log(v._test_token, "Session[" + session.sessionId + "], got message from CDM: " + msgStr);
        is(msg.type, session._test_sessionType, TimeStamp(v._test_token) + " key session type should match");
        ok(msg.kids, TimeStamp(v._test_token) + " message event should contain key ID array");

        var outKeys = [];

        for (var i = 0; i < msg.kids.length; i++) {
          var id64 = msg.kids[i];
          var idHex = Base64ToHex(msg.kids[i]).toLowerCase();
          var key = v._test_test.keys[idHex];

          if (key) {
            Log(v._test_token, "found key " + key + " for key id " + idHex);
            outKeys.push({
              "kty":"oct",
              "alg":"A128KW",
              "kid":id64,
              "k":HexToBase64(key)
            });
          } else {
            bail(v._test_token + " couldn't find key for key id " + idHex);
          }
        }

        var update = JSON.stringify({
          "keys" : outKeys,
          "type" : msg.type
        });
        Log(v._test_token, "Session[" + session.sessionId + "], sending update message to CDM: " + update);

        session.update(StringToArrayBuffer(update)).then(function() {
          Log(v._test_token, "Session[" + session.sessionId + "] update ok");
          if (v._test_params && v._test_params.onsessionupdated) {
            v._test_params.onsessionupdated(session);
          }
          return resolve(session);
        }).catch(function(reason) {
          bail(v._test_token + " Session[" + session.sessionId + "] update failed")(reason);
          return reject();
        });
      }
    }

    return new Promise(function (resolve, reject) {
      session.addEventListener("message", UpdateSessionFunc(resolve, reject));
      Log(v._test_token, "session[" + session.sessionId + "].generateRequest(" + ev.initDataType + ")");
      session.generateRequest(ev.initDataType, ev.initData)
      .catch(function(reason) {
        // Reject the promise if generateRequest() failed. Otherwise it will
        // be resolve in UpdateSessionFunc().
        bail(v.test_token + ": session[" + session.sessionId + "].generateRequest failed")(reason);
        return reject();
      });
    });
  });
}
