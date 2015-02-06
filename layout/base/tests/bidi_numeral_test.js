// Canvas related code stolen from http://developer.mozilla.org/en/docs/Code_snippets:Canvas
var RemoteCanvas = function(url, id) {
    this.url = url;
    this.id = id;
    this.snapshot = null;
};

RemoteCanvas.CANVAS_WIDTH = 200;
RemoteCanvas.CANVAS_HEIGHT = 100;

RemoteCanvas.prototype.compare = function(otherCanvas, expected) {
    return compareSnapshots(this.snapshot, otherCanvas.snapshot, expected)[0];
}

RemoteCanvas.prototype.load = function(callback) {
  var iframe = document.createElement("iframe");
  iframe.id = this.id + "-iframe";
  iframe.width = RemoteCanvas.CANVAS_WIDTH + "px";
  iframe.height = RemoteCanvas.CANVAS_HEIGHT + "px";
  iframe.src = this.url;
  var me = this;
  iframe.addEventListener("load", function() {
      me.remotePageLoaded(callback);
    }, false);
  window.document.body.appendChild(iframe);
};

RemoteCanvas.prototype.remotePageLoaded = function(callback) {
  var ldrFrame = document.getElementById(this.id + "-iframe");
  this.snapshot = snapshotWindow(ldrFrame.contentWindow);
  callback(this);
};

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
      if (passes[currentPass].op == "==") {
        ok(canvases[0].compare(canvases[1], true), "Rendering of reftest " + fileprefix + passes[currentPass].file +
           " is different with bidi.numeral == " + passes[currentPass].bidiNumeralValue);
      } else if (passes[currentPass].op == "!=") {
        ok(canvases[0].compare(canvases[1], false), "Rendering of reftest " + fileprefix + passes[currentPass].file +
           " is not different with bidi.numeral == " + passes[currentPass].bidiNumeralValue);
      }


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

  SpecialPowers.pushPrefEnv({'set': [['bidi.numeral', passes[currentPass].bidiNumeralValue]]},callback);

  function callback() {
    var testCanvas = new RemoteCanvas(fileprefix + file + ".html", "test-" + currentPass);
    testCanvas.load(callbackTestCanvas);

    var refCanvas = new RemoteCanvas(fileprefix + file + "-ref.html", "ref-" + currentPass);
    refCanvas.load(callbackTestCanvas);
  }
}

run();
