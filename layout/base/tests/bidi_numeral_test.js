// Canvas related code stolen from http://developer.mozilla.org/en/docs/Code_snippets:Canvas
var RemoteCanvas = function(url, id) {
    this.url = url;
    this.id = id;
};

RemoteCanvas.CANVAS_WIDTH = 200;
RemoteCanvas.CANVAS_HEIGHT = 100;

RemoteCanvas.prototype.getCanvas = function() {
  return document.getElementById(this.id + "-canvas");
}

RemoteCanvas.prototype.load = function(callback) {
  var iframe = document.createElement("iframe");
  iframe.id = this.id + "-iframe";
  iframe.width = RemoteCanvas.CANVAS_WIDTH + "px";
  iframe.height = RemoteCanvas.CANVAS_HEIGHT + "px";
  iframe.src = this.url;
  var me = this;
  iframe.addEventListener("load", function() {
      window.setTimeout(function() {
          me.remotePageLoaded(callback);
        }, 500);
    }, true);
  window.document.body.appendChild(iframe);
};

RemoteCanvas.prototype.remotePageLoaded = function(callback) {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalBrowserRead');
  var ldrFrame = document.getElementById(this.id + "-iframe");
  var remoteWindow = ldrFrame.contentWindow;

  var canvas = document.createElement("canvas");
  canvas.id = this.id + "-canvas";
  canvas.style.width = RemoteCanvas.CANVAS_WIDTH + "px";
  canvas.style.height = RemoteCanvas.CANVAS_HEIGHT + "px";
  canvas.width = RemoteCanvas.CANVAS_WIDTH;
  canvas.height = RemoteCanvas.CANVAS_HEIGHT;

  var ctx = canvas.getContext("2d");
  ctx.clearRect(0, 0,
                RemoteCanvas.CANVAS_WIDTH,
                RemoteCanvas.CANVAS_HEIGHT);
  ctx.drawWindow(remoteWindow,
                 0, 0,
                 RemoteCanvas.CANVAS_WIDTH,
                 RemoteCanvas.CANVAS_HEIGHT,
                 "rgb(255,255,255)");

  window.document.body.appendChild(canvas);

  callback(this);
};

function bidiNumeral(val) {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalPreferencesRead UniversalPreferencesWrite UniversalXPConnect');
  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                        .getService(Components.interfaces.nsIPrefBranch);

  if (typeof val == "undefined")
    return prefs.getIntPref("bidi.numeral");
  else
    prefs.setIntPref("bidi.numeral", val);
}

var bidiNumeralDefault = bidiNumeral();

var currentPass = 0;

function run()
{
  SimpleTest.waitForExplicitFinish();

  do_test();
}

function do_test()
{
  var canvases = [];
  function callbackTestCanvas(canvas)
  {
    canvases.push(canvas);

    if (canvases.length == 2) { // when both canvases are loaded
      var img_1 = canvases[0].getCanvas().toDataURL("image/png", "");
      var img_2 = canvases[1].getCanvas().toDataURL("image/png", "");
      if (passes[currentPass].op == "==") {
        ok(img_1 == img_2, "Rendering of reftest " + fileprefix + passes[currentPass].file +
           " is different with bidi.numeral == " + passes[currentPass].bidiNumeralValue);
      } else if (passes[currentPass].op == "!=") {
        ok(img_1 != img_2, "Rendering of reftest " + fileprefix + passes[currentPass].file +
           " is not different with bidi.numeral == " + passes[currentPass].bidiNumeralValue);
      }

      bidiNumeral(bidiNumeralDefault);

      if (currentPass < passes.length - 1) {
        ++currentPass;
        do_test();
      } else {
        SimpleTest.finish();
      }
    }
  }

  var fileprefix = passes[currentPass].prefix + "-";
  var file = passes[currentPass].file;

  var header = document.createElement("p");
  header.appendChild(document.createTextNode("Testing reftest " + fileprefix + file +
    " with bidi.numeral == " + passes[currentPass].bidiNumeralValue +
    " expecting " + passes[currentPass].op));
  document.body.appendChild(header);

  bidiNumeral(passes[currentPass].bidiNumeralValue);

  var testCanvas = new RemoteCanvas(fileprefix + file + ".html", "test-" + currentPass);
  testCanvas.load(callbackTestCanvas);

  var refCanvas = new RemoteCanvas(fileprefix + file + "-ref.html", "ref-" + currentPass);
  refCanvas.load(callbackTestCanvas);
}

run();
