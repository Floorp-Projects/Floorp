WebGLUtil = (function() {
  // ---------------------------------------------------------------------------
  // Error handling (for obvious failures, such as invalid element ids)

  function defaultErrorFunc(str) {
    console.log('Error: ' + str);
  }

  var gErrorFunc = defaultErrorFunc;
  function setErrorFunc(func) {
    gErrorFunc = func;
  }

  function error(str) {
    gErrorFunc(str);
  }

  // ---------------------------------------------------------------------------
  // Warning handling (for failures that may be intentional)

  function defaultWarningFunc(str) {
    console.log('Warning: ' + str);
  }

  var gWarningFunc = defaultWarningFunc;
  function setWarningFunc(func) {
    gWarningFunc = func;
  }

  function warning(str) {
    gWarningFunc(str);
  }

  // ---------------------------------------------------------------------------
  // WebGL helpers

  function getWebGL(canvasId, requireConformant, attributes) {
    // `requireConformant` will default to falsey if it is not supplied.

    var canvas = document.getElementById(canvasId);

    var gl = null;
    try {
      gl = canvas.getContext('webgl', attributes);
    } catch(e) {}

    if (!gl && !requireConformant) {
      try {
        gl = canvas.getContext('experimental-webgl', attributes);
      } catch(e) {}
    }

    if (!gl) {
      error('WebGL context could not be retrieved from \'' + canvasId + '\'.');
      return null;
    }

    return gl;
  }

  function withWebGL2(canvasId, callback, onFinished) {
    var prefArrArr = [
      ['webgl.force-enabled', true],
      ['webgl.disable-angle', true],
      ['webgl.bypass-shader-validation', true],
      ['webgl.enable-prototype-webgl2', true],
    ];
    var prefEnv = {'set': prefArrArr};
    SpecialPowers.pushPrefEnv(prefEnv, function() {
      var canvas = document.getElementById(canvasId);

      var gl = null;
      try {
        gl = canvas.getContext('webgl2');
      } catch(e) {}

      if (!gl) {
        todo(false, 'WebGL2 is not supported');
        onFinished();
        return;
      }

      function errorFunc(str) {
        ok(false, 'Error: ' + str);
      }
      setErrorFunc(errorFunc);
      setWarningFunc(errorFunc);

      callback(gl);
      onFinished();
    });
  }

  function getContentFromElem(elem) {
    var str = "";
    var k = elem.firstChild;
    while (k) {
      if (k.nodeType == 3)
        str += k.textContent;

      k = k.nextSibling;
    }

    return str;
  }

  // Returns a valid shader, or null on errors.
  function createShaderById(gl, id) {
    var elem = document.getElementById(id);
    if (!elem) {
      error('Failed to create shader from non-existent id \'' + id + '\'.');
      return null;
    }

    var src = getContentFromElem(elem);

    var shader;
    if (elem.type == "x-shader/x-fragment") {
      shader = gl.createShader(gl.FRAGMENT_SHADER);
    } else if (elem.type == "x-shader/x-vertex") {
      shader = gl.createShader(gl.VERTEX_SHADER);
    } else {
      error('Bad MIME type for shader \'' + id + '\': ' + elem.type + '.');
      return null;
    }

    gl.shaderSource(shader, src);
    gl.compileShader(shader);

    return shader;
  }

  function createProgramByIds(gl, vsId, fsId) {
    var vs = createShaderById(gl, vsId);
    var fs = createShaderById(gl, fsId);
    if (!vs || !fs)
      return null;

    var prog = gl.createProgram();
    gl.attachShader(prog, vs);
    gl.attachShader(prog, fs);
    gl.linkProgram(prog);

    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
      var str = "Shader program linking failed:";
      str += "\nShader program info log:\n" + gl.getProgramInfoLog(prog);
      str += "\n\nVert shader log:\n" + gl.getShaderInfoLog(vs);
      str += "\n\nFrag shader log:\n" + gl.getShaderInfoLog(fs);
      warning(str);
      return null;
    }

    return prog;
  }

  return {
    setErrorFunc: setErrorFunc,
    setWarningFunc: setWarningFunc,

    getWebGL: getWebGL,
    withWebGL2: withWebGL2,
    createShaderById: createShaderById,
    createProgramByIds: createProgramByIds,
  };
})();
