const KEYSYSTEM_TYPE = "org.w3.clearkey";

function bail(message)
{
  return function(err) {
    ok(false, message);
    if (err) {
      info(err);
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

function UpdateSessionFunc(test) {
  return function(ev) {
    var msgStr = ArrayBufferToString(ev.message);
    var msg = JSON.parse(msgStr);

    info("got message from CDM: " + msgStr);
    is(msg.type, test.sessionType, "Key session type should match");
    ok(msg.kids, "message event should contain key ID array");

    var outKeys = [];

    for (var i = 0; i < msg.kids.length; i++) {
      var id64 = msg.kids[i];
      var idHex = Base64ToHex(msg.kids[i]).toLowerCase();
      var key = test.keys[idHex];

      if (key) {
        info("found key " + key + " for key id " + idHex);
        outKeys.push({
          "kty":"oct",
          "alg":"A128KW",
          "kid":id64,
          "k":HexToBase64(key)
        });
      } else {
        bail("Couldn't find key for key id " + idHex);
      }
    }

    var update = JSON.stringify({
      "keys" : outKeys,
      "type" : msg.type
    });
    info("sending update message to CDM: " + update);

    ev.target.update(StringToArrayBuffer(update)).then(function() {
      info("MediaKeySession update ok!");
    }, bail("MediaKeySession update failed"));
  }
}

function PlayFragmented(test, elem)
{
  return new Promise(function(resolve, reject) {
    var ms = new MediaSource();
    elem.src = URL.createObjectURL(ms);

    var sb;
    var curFragment = 0;

    function addNextFragment() {
      if (curFragment >= test.fragments.length) {
        ms.endOfStream();
        resolve();
        return;
      }

      var fragmentFile = test.fragments[curFragment++];

      var req = new XMLHttpRequest();
      req.open("GET", fragmentFile);
      req.responseType = "arraybuffer";

      req.addEventListener("load", function() {
        sb.appendBuffer(new Uint8Array(req.response));
      });

      info("fetching resource " + fragmentFile);
      req.send(null);
    }

    ms.addEventListener("sourceopen", function () {
      sb = ms.addSourceBuffer(test.type);
      sb.addEventListener("updateend", addNextFragment);

      addNextFragment();
    })
  });
}

// Returns a promise that is resovled when the media element is ready to have
// its play() function called; when it's loaded MSE fragments, or once the load
// has started for non-MSE video.
function LoadTest(test, elem)
{
  if (test.fragments) {
    return PlayFragmented(test, elem);
  }

  // This file isn't fragmented; set the media source normally.
  return new Promise(function(resolve, reject) {
    elem.src = test.name;
    resolve();
  });
}

function SetupEME(test, token, params)
{
  var v = document.createElement("video");

  var onSetKeysFail = (params && params.onSetKeysFail)
    ? params.onSetKeysFail
    : bail(token + " Failed to set MediaKeys on <video> element");
  
  v.addEventListener("encrypted", function(ev) {
    info(token + " got encrypted event");
    MediaKeys.create(KEYSYSTEM_TYPE).then(function(mediaKeys) {
      info(token + " created MediaKeys object ok");
      mediaKeys.sessions = [];
      return v.setMediaKeys(mediaKeys);
    }, bail("failed to create MediaKeys object")).then(function() {
      info(token + " set MediaKeys on <video> element ok");

      var session = v.mediaKeys.createSession(test.sessionType);
      if (params && params.onsessioncreated) {
        params.onsessioncreated(session);
      }
      session.addEventListener("message", UpdateSessionFunc(test));
      session.generateRequest(ev.initDataType, ev.initData).then(function() {
      }, bail(token + " Failed to initialise MediaKeySession"));

    }, onSetKeysFail);
  });

  return v;
}
