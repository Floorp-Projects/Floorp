// Canvas related code stolen from http://developer.mozilla.org/en/docs/Code_snippets:Canvas
var RemoteCanvas = function(url, id) {
    this.url = url;
    this.id = id;
    this.snapshot = null;
};

RemoteCanvas.CANVAS_WIDTH = 200;
RemoteCanvas.CANVAS_HEIGHT = 100;

RemoteCanvas.prototype.load = function(callback, callbackData) {
  var iframe = document.createElement("iframe");
  iframe.id = this.id + "-iframe";
  iframe.width = RemoteCanvas.CANVAS_WIDTH + "px";
  iframe.height = RemoteCanvas.CANVAS_HEIGHT + "px";
  iframe.src = this.url;
  var me = this;
  iframe.addEventListener("load", function() {
      me.remotePageLoaded(callback, callbackData);
    }, false);
  window.document.body.appendChild(iframe);
};

RemoteCanvas.prototype.remotePageLoaded = function(callback, callbackData) {
  var ldrFrame = document.getElementById(this.id + "-iframe");
  this.snapshot = snapshotWindow(ldrFrame.contentWindow);
  callback(this, callbackData);
};

var currentPass = 0;

function run()
{
  SimpleTest.waitForExplicitFinish();

  do_test();
}

function do_test()
{
  var canvases = [ null, null ];
  var countDone = 0;
  function callbackTestCanvas(canvas, index)
  {
    ++countDone;
    canvases[index] = canvas;

    if (countDone == 2) { // when both canvases are loaded
      assertSnapshots(canvases[0].snapshot, canvases[1].snapshot,
                      passes[currentPass].op == "==", 0,
                      testfile,
                      reffile + " (with bidi.numeral == " + passes[currentPass].bidiNumeralValue + ")");

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
  var testfile = fileprefix + file + ".html";
  var reffile = fileprefix + file + "-ref.html";

  var header = document.createElement("p");
  header.appendChild(document.createTextNode("Testing reftest " + fileprefix + file +
    " with bidi.numeral == " + passes[currentPass].bidiNumeralValue +
    " expecting " + passes[currentPass].op));
  document.body.appendChild(header);

  SpecialPowers.pushPrefEnv({'set': [['bidi.numeral', passes[currentPass].bidiNumeralValue]]},callback);

  function callback() {
    var testCanvas = new RemoteCanvas(testfile, "test-" + currentPass);
    testCanvas.load(callbackTestCanvas, 0);

    var refCanvas = new RemoteCanvas(reffile, "ref-" + currentPass);
    refCanvas.load(callbackTestCanvas, 1);
  }
}

run();
