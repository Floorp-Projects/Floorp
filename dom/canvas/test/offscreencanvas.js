/* WebWorker for test_offscreencanvas_*.html */
(function(){

var port = null;
var INIT_CONST_CANVAS_SIZE = 64;
var COUNT_FOR_SIZE_CHANGE = 3;

function isInWorker() {
  try {
    return !(self instanceof Window);
  } catch (e) {
    return true;
  }
}

function postMessageGeneral(data) {
  if (isInWorker()) {
    if (port) {
      port.postMessage(data);
    } else {
      postMessage(data);
    }
  } else {
    postMessage(data, "*");
  }
}

function ok(expect, msg) {
  postMessageGeneral({type: "test", result: !!expect, name: msg});
}

function finish() {
  postMessageGeneral({type: "finish"});
}

function drawCount(count) {
  postMessageGeneral({type: "draw", count: count});
}

function sendBlob(canvasType, blob) {
  postMessageGeneral({type: "blob", canvasType: canvasType, blob: blob});
}

function sendImageBitmap(img) {
  if (port) {
    port.postMessage({type: "imagebitmap", bitmap: img});
  } else {
    postMessage({type: "imagebitmap", bitmap: img});
  }
}

function sendImageBitmapWithSize(img, size) {
  postMessage({type: "imagebitmapwithsize", bitmap: img, size: size});
}
//--------------------------------------------------------------------
// Canvas 2D Drawing Functions
//--------------------------------------------------------------------
function createDrawFunc2D(canvas) {
  var context2d;

  try {
    context2d = canvas.getContext("2d");
  } catch (e) {}

  if (!context2d) {
    ok(false, "Canvas 2D is unavailable");
    return null;
  }

  return function(prefix, needCommitFrame) {
    if (prefix) {
      prefix = "[" + prefix + "] ";
    } else {
      prefix = "";
    }

    context2d.rect(0, 0, canvas.width, canvas.height);
    context2d.fillStyle = "#00FF00";
    context2d.fill();
    if (needCommitFrame) {
      context2d.commit();
      ok(true, prefix + 'Frame is committed');
    }
  };
}

//--------------------------------------------------------------------
// WebGL Drawing Functions
//--------------------------------------------------------------------
function createDrawFunc(canvas) {
  var gl;

  try {
    gl = canvas.getContext("experimental-webgl");
  } catch (e) {}

  if (!gl) {
    ok(false, "WebGL is unavailable");
    return null;
  }

  var vertSrc = "attribute vec2 position; \
                 void main(void) { \
                   gl_Position = vec4(position, 0.0, 1.0); \
                 }";

  var fragSrc = "precision mediump float; \
                 void main(void) { \
                   gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); \
                 }";

  // Returns a valid shader, or null on errors.
  var createShader = function(src, t) {
    var shader = gl.createShader(t);

    gl.shaderSource(shader, src);
    gl.compileShader(shader);

    return shader;
  };

  var createProgram = function(vsSrc, fsSrc) {
    var vs = createShader(vsSrc, gl.VERTEX_SHADER);
    var fs = createShader(fsSrc, gl.FRAGMENT_SHADER);

    var prog = gl.createProgram();
    gl.attachShader(prog, vs);
    gl.attachShader(prog, fs);
    gl.linkProgram(prog);

    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
      var str = "Shader program linking failed:";
      str += "\nShader program info log:\n" + gl.getProgramInfoLog(prog);
      str += "\n\nVert shader log:\n" + gl.getShaderInfoLog(vs);
      str += "\n\nFrag shader log:\n" + gl.getShaderInfoLog(fs);
      console.log(str);
      ok(false, "Shader program linking failed");
      return null;
    }

    return prog;
  };

  gl.disable(gl.DEPTH_TEST);

  var program = createProgram(vertSrc, fragSrc);
  ok(program, "Creating shader program");

  program.positionAttr = gl.getAttribLocation(program, "position");
  ok(program.positionAttr >= 0, "position attribute should be valid");

  var vertCoordArr = new Float32Array([
    -1, -1,
     1, -1,
    -1,  1,
     1,  1,
  ]);
  var vertCoordBuff = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, vertCoordBuff);
  gl.bufferData(gl.ARRAY_BUFFER, vertCoordArr, gl.STATIC_DRAW);

  var checkGLError = function(prefix, refValue) {
    if (!refValue) {
      refValue = 0;
    }

    var error = gl.getError();
    ok(error == refValue,
         prefix + 'gl.getError should be 0x' + refValue.toString(16) +
         ', was 0x' + error.toString(16) + '.');
  };

  var testPixel = function(x, y, refData, infoString) {
    var pixel = new Uint8Array(4);
    gl.readPixels(x, y, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, pixel);

    var pixelMatches = pixel[0] == refData[0] &&
                       pixel[1] == refData[1] &&
                       pixel[2] == refData[2] &&
                       pixel[3] == refData[3];
    ok(pixelMatches, infoString);
  };

  var preDraw = function(prefix) {
    gl.clearColor(1.0, 0.0, 0.0, 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT);

    testPixel(0, 0, [255, 0, 0, 255], prefix + 'Should be red before drawing.');
  };

  var postDraw = function(prefix) {
    testPixel(0, 0, [0, 255, 0, 255], prefix + 'Should be green after drawing.');
  };

  gl.useProgram(program);
  gl.enableVertexAttribArray(program.position);
  gl.vertexAttribPointer(program.position, 2, gl.FLOAT, false, 0, 0);

  // Start drawing
  checkGLError('after setup');

  return function(prefix, needCommitFrame) {
    if (prefix) {
      prefix = "[" + prefix + "] ";
    } else {
      prefix = "";
    }

    gl.viewport(0, 0, canvas.width, canvas.height);

    preDraw(prefix);
    gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
    postDraw(prefix);
    if (needCommitFrame) {
      gl.commit();
    }
    checkGLError(prefix);
  };
}

/* entry point */
function entryFunction(canvasTypeStr, testTypeStr, subtests, offscreenCanvas) {
  var canvasType = canvasTypeStr;
  var testType = testTypeStr;
  var canvas = offscreenCanvas;
  if ((testType == "imagebitmap") || (testType == "changesize")) {
    canvas = new OffscreenCanvas(INIT_CONST_CANVAS_SIZE, INIT_CONST_CANVAS_SIZE);
  }

  if (testType != "subworker") {
    ok(canvas, "Canvas successfully transfered to worker");
    ok(canvas.getContext, "Canvas has getContext");

    ok(canvas.width == INIT_CONST_CANVAS_SIZE,
       "OffscreenCanvas width should be " + INIT_CONST_CANVAS_SIZE);
    ok(canvas.height == INIT_CONST_CANVAS_SIZE,
       "OffscreenCanvas height should be " + INIT_CONST_CANVAS_SIZE);
  }

  var draw;
  if (testType != "subworker") {
    if (canvasType == "2d") {
      draw = createDrawFunc2D(canvas);
    } else if (canvasType == "webgl") {
      draw = createDrawFunc(canvas);
    } else {
      ok(false, "Unexpected canvasType");
    }

    if (!draw) {
      finish();
      return;
    }
  }
  //------------------------------------------------------------------------
  // Basic test
  //------------------------------------------------------------------------
  if (testType == "basic") {
    var count = 0;
    var iid = setInterval(function() {
      if (count++ > 20) {
        clearInterval(iid);
        ok(true, "Worker is done");
        finish();
        return;
      }
      draw("loop " +count, true);
    }, 0);
  }
  //------------------------------------------------------------------------
  // Test dynamic fallback
  //------------------------------------------------------------------------
  else if (testType == "fallback") {
    var count = 0;
    var iid = setInterval(function() {
      ++count;
      draw("loop " + count, true);
      drawCount(count);
    }, 0);
  }
  //------------------------------------------------------------------------
  // Test toBlob
  //------------------------------------------------------------------------
  else if (testType == "toblob") {
    draw("", false);
    canvas.toBlob().then(function(blob) {
      sendBlob(canvasType, blob);
    });
  }
  //------------------------------------------------------------------------
  // Test toImageBitmap
  //------------------------------------------------------------------------
  else if (testType == "imagebitmap") {
    draw("", false);
    var imgBitmap = canvas.transferToImageBitmap();
    sendImageBitmap(imgBitmap);
    finish();
  }
  //------------------------------------------------------------------------
  // Canvas Size Change from Worker
  //------------------------------------------------------------------------
  else if (testType == "changesize") {
    var count = 0;
    var iid = setInterval(function() {
      if (count++ > COUNT_FOR_SIZE_CHANGE) {
        finish();
        clearInterval(iid);
        return;
      }

      canvas.width = INIT_CONST_CANVAS_SIZE * count;
      canvas.height = INIT_CONST_CANVAS_SIZE * count;
      draw("loop " + count, true);
      var imgBitmap = canvas.transferToImageBitmap();
      sendImageBitmapWithSize(imgBitmap, INIT_CONST_CANVAS_SIZE * count);
    }, 0);
  }
  //------------------------------------------------------------------------
  // Using OffscreenCanvas from sub workers
  //------------------------------------------------------------------------
  else if (testType == "subworker") {
    /* subworker tests take a list of tests to run on children */
    var stillRunning = 0;
    subtests.forEach(function (subtest) {
      ++stillRunning;
      var subworker = new Worker('offscreencanvas.js');
      subworker.onmessage = function(evt) {
        /* report finish to parent when all children are finished */
        if (evt.data.type == "finish") {
          subworker.terminate();
          if (--stillRunning == 0) {
            ok(true, "Worker is done");
            finish();
          }
          return;
        }
        /* relay all other messages to parent */
        postMessage(evt.data);
      };

      var findTransferables = function(t) {
        if (t.testType == "subworker") {
          var result = [];
          t.subtests.forEach(function(testType) {
            result = result.concat(findTransferables(testType));
          });

          return result;
        } else {
          return [t.canvas];
        }
      };

      subworker.postMessage(subtest, findTransferables(subtest));
    });
  }
};

onmessage = function(evt) {
  port = evt.ports[0];
  entryFunction(evt.data.canvasType, evt.data.testType, evt.data.subtests,
                evt.data.canvas);
};

onconnect = function(evt) {
  port = evt.ports[0];

  port.addEventListener('message', function(evt) {
    entryFunction(evt.data.canvasType,  evt.data.testType, evt.data.subtests,
                  evt.data.canvas);
  });

  port.start();
};

if (!isInWorker()) {
  window.entryFunction = entryFunction;
}

})();
