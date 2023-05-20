WebGLUtil = (function () {
  // ---------------------------------------------------------------------------
  // WebGL helpers

  function withWebGL2(canvasId, callback, onFinished) {
    var run = function () {
      var canvas = document.getElementById(canvasId);

      var gl = null;
      try {
        gl = canvas.getContext("webgl2");
      } catch (e) {}

      if (!gl) {
        todo(false, "WebGL2 is not supported");
        onFinished();
        return;
      }

      callback(gl);
      onFinished();
    };

    try {
      var prefArrArr = [
        ["webgl.force-enabled", true],
        ["webgl.enable-webgl2", true],
      ];
      var prefEnv = { set: prefArrArr };
      SpecialPowers.pushPrefEnv(prefEnv, run);
    } catch (e) {
      warning("No SpecialPowers, but trying WebGL2 anyway...");
      run();
    }
  }

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
    withWebGL2,

    createShaderById,
    createProgramByIds,

    linkProgramByIds(gl, vertSrcElem, fragSrcElem) {
      const prog = gl.createProgram();

      function attachShaderById(type, srcElem) {
        const shader = gl.createShader(type);
        gl.shaderSource(shader, srcElem.innerHTML.trim() + "\n");
        gl.compileShader(shader);
        gl.attachShader(prog, shader);
        prog[type] = shader;
      }
      attachShaderById(gl.VERTEX_SHADER, vertSrcElem);
      attachShaderById(gl.FRAGMENT_SHADER, fragSrcElem);

      gl.linkProgram(prog);
      const success = gl.getProgramParameter(prog, gl.LINK_STATUS);
      if (!success) {
        console.error("Error linking program:");
        console.error("\nLink log: " + gl.getProgramInfoLog(prog));
        console.error(
          "\nVert shader log: " + gl.getShaderInfoLog(prog[gl.VERTEX_SHADER])
        );
        console.error(
          "\nFrag shader log: " + gl.getShaderInfoLog(prog[gl.FRAGMENT_SHADER])
        );
        return null;
      }
      gl.deleteShader(prog[gl.VERTEX_SHADER]);
      gl.deleteShader(prog[gl.FRAGMENT_SHADER]);

      let count = gl.getProgramParameter(prog, gl.ACTIVE_ATTRIBUTES);
      for (let i = 0; i < count; i++) {
        const info = gl.getActiveAttrib(prog, i);
        prog[info.name] = gl.getAttribLocation(prog, info.name);
      }
      count = gl.getProgramParameter(prog, gl.ACTIVE_UNIFORMS);
      for (let i = 0; i < count; i++) {
        const info = gl.getActiveUniform(prog, i);
        prog[info.name] = gl.getUniformLocation(prog, info.name);
      }
      return prog;
    },
  };
})();
