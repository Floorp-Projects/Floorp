WebGLUtil = (function() {
  // ---------------------------------------------------------------------------
  // WebGL helpers

  // Returns a valid shader, or null on errors.
  function createShaderById(gl, id) {
    var elem = document.getElementById(id);
    if (!elem) {
      throw new Error(
        "Failed to create shader from non-existent id '" + id + "'."
      );
    }

    var src = elem.innerHTML.trim();

    var shader;
    if (elem.type == "x-shader/x-fragment") {
      shader = gl.createShader(gl.FRAGMENT_SHADER);
    } else if (elem.type == "x-shader/x-vertex") {
      shader = gl.createShader(gl.VERTEX_SHADER);
    } else {
      throw new Error(
        "Bad MIME type for shader '" + id + "': " + elem.type + "."
      );
    }

    gl.shaderSource(shader, src);
    gl.compileShader(shader);

    return shader;
  }

  function createProgramByIds(gl, vsId, fsId) {
    var vs = createShaderById(gl, vsId);
    var fs = createShaderById(gl, fsId);
    if (!vs || !fs) {
      return null;
    }

    var prog = gl.createProgram();
    gl.attachShader(prog, vs);
    gl.attachShader(prog, fs);
    gl.linkProgram(prog);

    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
      var str = "Shader program linking failed:";
      str += "\nShader program info log:\n" + gl.getProgramInfoLog(prog);
      str += "\n\nVert shader log:\n" + gl.getShaderInfoLog(vs);
      str += "\n\nFrag shader log:\n" + gl.getShaderInfoLog(fs);
      console.error(str);
      return null;
    }

    return prog;
  }

  return {
    createShaderById,
    createProgramByIds,
  };
})();
