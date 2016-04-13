/* WebWorker for test_offscreencanvas_*.html */
(function(){

var port = null;

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

function sendBlob(blob) {
  postMessageGeneral({type: "blob", blob: blob});
}

function sendImageBitmap(img) {
  if (port) {
    port.postMessage({type: "imagebitmap", bitmap: img});
  } else {
    postMessage({type: "imagebitmap", bitmap: img});
  }
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
    checkGLError(prefix + "[viewport]");

    preDraw(prefix);
    checkGLError(prefix + "[predraw]");
    gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
    checkGLError(prefix + "[drawarrays]");
    postDraw(prefix);
    checkGLError(prefix + "[postdraw]");
    if (needCommitFrame) {
      gl.commit();
      checkGLError(prefix + "[commit]");
    }
    checkGLError(prefix);
  };
}

/* entry point */
function entryFunction(testStr, subtests, offscreenCanvas) {
  var test = testStr;
  var canvas = offscreenCanvas;
  if (test == "webgl_imagebitmap") {
    canvas = new OffscreenCanvas(64, 64);
  }

  if (test != "subworker") {
    ok(canvas, "Canvas successfully transfered to worker");
    ok(canvas.getContext, "Canvas has getContext");

    ok(canvas.width == 64, "OffscreenCanvas width should be 64");
    ok(canvas.height == 64, "OffscreenCanvas height should be 64");
  }

  var draw;

  //------------------------------------------------------------------------
  // Basic WebGL test
  //------------------------------------------------------------------------
  if (test == "webgl") {
    draw = createDrawFunc(canvas);
    if (!draw) {
      finish();
      return;
    }

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
  else if (test == "webgl_fallback") {
    draw = createDrawFunc(canvas);
    if (!draw) {
      return;
    }

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
  else if (test == "webgl_toblob") {
    draw = createDrawFunc(canvas);
    if (!draw) {
      return;
    }

    draw("", false);
    canvas.toBlob().then(function(blob) {
      sendBlob(blob);
    });
  }
  //------------------------------------------------------------------------
  // Test toImageBitmap
  //------------------------------------------------------------------------
  else if (test == "webgl_imagebitmap") {
    draw = createDrawFunc(canvas);
    if (!draw) {
      return;
    }

    draw("", false);
    var imgBitmap = canvas.transferToImageBitmap();
    sendImageBitmap(imgBitmap);
  }
  //------------------------------------------------------------------------
  // Canvas Size Change from Worker
  //------------------------------------------------------------------------
  else if (test == "webgl_changesize") {
    draw = createDrawFunc(canvas);
    if (!draw) {
      finish();
      return;
    }

    draw("64x64", true);

    setTimeout(function() {
      canvas.width = 128;
      canvas.height = 128;
      draw("Increased to 128x128", true);

      setTimeout(function() {
        canvas.width = 32;
        canvas.width = 32;
        draw("Decreased to 32x32", true);

        setTimeout(function() {
          canvas.width = 64;
          canvas.height = 64;
          draw("Increased to 64x64", true);

          ok(true, "Worker is done");
          finish();
        }, 0);
      }, 0);
    }, 0);
  }
  //------------------------------------------------------------------------
  // Using OffscreenCanvas from sub workers
  //------------------------------------------------------------------------
  else if (test == "subworker") {
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
        if (t.test == "subworker") {
          var result = [];
          t.subtests.forEach(function(test) {
            result = result.concat(findTransferables(test));
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
  entryFunction(evt.data.test, evt.data.subtests, evt.data.canvas);
};

onconnect = function(evt) {
  port = evt.ports[0];

  port.addEventListener('message', function(evt) {
    entryFunction(evt.data.test, evt.data.subtests, evt.data.canvas);
  });

  port.start();
};

if (!isInWorker()) {
  window.entryFunction = entryFunction;
}

})();
