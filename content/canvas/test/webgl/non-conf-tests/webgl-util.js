WebGLUtil = (function() {
  // ---------------------------------------------------------------------------
  // Error handling

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
  // WebGL helpers

  function getWebGL(canvasId, requireConformant) {
    // `requireConformant` will default to falsey if it is not supplied.

    var canvas = document.getElementById(canvasId);

    var gl = null;
    try {
      gl = canvas.getContext('webgl');
    } catch(e) {}

    if (!gl && !requireConformant) {
      try {
        gl = canvas.getContext('experimental-webgl');
      } catch(e) {}
    }

    if (!gl) {
      error('WebGL context could not be retrieved from \'' + canvasId + '\'.');
      return null;
    }

    return gl;
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

    var src = getContentById(id);

    var shader;
    if (elem.type == "x-shader/x-fragment") {
      shader = gl.createShader(gl.FRAGMENT_SHADER);
    } else if (shaderScript.type == "x-shader/x-vertex") {
      shader = gl.createShader(gl.VERTEX_SHADER);
    } else {
      error('Bad MIME type for shader \'' + id + '\': ' + elem.type + '.');
      return null;
    }

    gl.shaderSource(shader, str);
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
      var str = "Shader program linking failed:\n";
      str += "Shader program info log:\n" + gl.getProgramInfoLog(prog) + "\n\n";
      str += "Vert shader log:\n" + gl.getShaderInfoLog(vs) + "\n\n";
      str += "Frag shader log:\n" + gl.getShaderInfoLog(fs);
      error(str);
      return null;
    }

    return prog;
  }

  return {
    setErrorFunc: setErrorFunc,

    getWebGL: getWebGL,
    createShaderById: createShaderById,
    createProgramByIds: createProgramByIds,
  };
})();
