GLSLConformanceTester = (function(){

var wtu = WebGLTestUtils;
var defaultVertexShader = [
  "attribute vec4 vPosition;",
  "void main()",
  "{",
  "    gl_Position = vPosition;",
  "}"
].join('\n');

var defaultFragmentShader = [
  "precision mediump float;",
  "void main()",
  "{",
  "    gl_FragColor = vec4(1.0,0.0,0.0,1.0);",
  "}"
].join('\n');

function log(msg) {
  if (window.console && window.console.log) {
    window.console.log(msg);
  }
}

var vShaderDB = {};
var fShaderDB = {};

/**
 * vShaderSource: the source code for vertex shader
 * vShaderSuccess: true if vertex shader compiliation should
 *   succeed.
 * fShaderSource: the source code for fragment shader
 * fShaderSuccess: true if fragment shader compiliation should
 *   succeed.
 * linkSuccess: true of link should succeed
 * passMsg: msg to describe success condition.
 *
 */
function runOneTest(gl, info) {
  var passMsg = info.passMsg
  log(passMsg);

  if (info.vShaderSource === undefined) {
    if (info.vShaderId) {
      info.vShaderSource = document.getElementById(info.vShaderId).text;
    } else {
      info.vShader = 'defaultVertexShader';
      info.vShaderSource = defaultVertexShader;
    }
  }
  if (info.fShaderSource === undefined) {
    if (info.fShaderId) {
      info.fShaderSource = document.getElementById(info.fShaderId).text;
    } else {
      info.fShader = 'defaultFragmentShader';
      info.fShaderSource = defaultFragmentShader;
    }
  }

  var vSource = info.vShaderPrep ? info.vShaderPrep(info.vShaderSource) :
    info.vShaderSource;

  // Reuse identical shaders so we test shared shader.
  var vShader = vShaderDB[vSource];
  if (!vShader) {
    vShader = wtu.loadShader(gl, vSource, gl.VERTEX_SHADER);
    if (info.vShaderTest) {
      if (!info.vShaderTest(vShader)) {
        testFailed("[vertex shader test] " + passMsg);
        return;
      }
    }
    // As per GLSL 1.0.17 10.27 we can only check for success on
    // compileShader, not failure.
    if (info.vShaderSuccess && !vShader) {
      testFailed("[unexpected vertex shader compile status] (expected: " +
                 info.vShaderSuccess + ") " + passMsg);
    }
    // Save the shaders so we test shared shader.
    if (vShader) {
      vShaderDB[vSource] = vShader;
    }
  }

  var fSource = info.fShaderPrep ? info.fShaderPrep(info.fShaderSource) :
    info.fShaderSource;

  // Reuse identical shaders so we test shared shader.
  var fShader = fShaderDB[fSource];
  if (!fShader) {
    fShader = wtu.loadShader(gl, fSource, gl.FRAGMENT_SHADER);
    if (info.fShaderTest) {
      if (!info.fShaderTest(fShader)) {
        testFailed("[fragment shdaer test] " + passMsg);
        return;
      }
    }
    //debug(fShader == null ? "fail" : "succeed");
    // As per GLSL 1.0.17 10.27 we can only check for success on
    // compileShader, not failure.
    if (info.fShaderSuccess && !fShader) {
      testFailed("[unexpected fragment shader compile status] (expected: " +
                info.fShaderSuccess + ") " + passMsg);
      return;
    }
    // Safe the shaders so we test shared shader.
    if (fShader) {
      fShaderDB[fSource] = fShader;
    }
  }

  if (vShader && fShader) {
    var program = gl.createProgram();
    gl.attachShader(program, vShader);
    gl.attachShader(program, fShader);
    gl.linkProgram(program);
    var linked = (gl.getProgramParameter(program, gl.LINK_STATUS) != 0);
    if (!linked) {
      var error = gl.getProgramInfoLog(program);
      log("*** Error linking program '"+program+"':"+error);
    }
    if (linked != info.linkSuccess) {
      testFailed("[unexpected link status] " + passMsg);
      return;
    }
  } else {
    if (info.linkSuccess) {
      testFailed("[link failed] " + passMsg);
      return;
    }
  }
  testPassed(passMsg);
}

function runTests(shaderInfos) {
  var wtu = WebGLTestUtils;
  var canvas = document.createElement('canvas');
  var gl = wtu.create3DContext();
  if (!gl) {
    testFailed("context does not exist");
    finishTest();
    return;
  }

  for (var ii = 0; ii < shaderInfos.length; ++ii) {
    runOneTest(gl, shaderInfos[ii]);
  }

  finishTest();
};

function loadExternalShaders(filename, passMsg) {
  var shaderInfos = [];
  var lines = wtu.readFileList(filename);
  for (var ii = 0; ii < lines.length; ++ii) {
    var info = {
      vShaderSource:  defaultVertexShader,
      vShaderSuccess: true,
      fShaderSource:  defaultFragmentShader,
      fShaderSuccess: true,
      linkSuccess:    true,
    };

    var line = lines[ii];
    var files = line.split(/ +/);
    var passMsg = "";
    for (var jj = 0; jj < files.length; ++jj) {
      var file = files[jj];
      var shaderSource = wtu.readFile(file);
      var firstLine = shaderSource.split("\n")[0];
      var success = undefined;
      if (firstLine.indexOf("fail") >= 0) {
        success = false;
      } else if (firstLine.indexOf("succeed") >= 0) {
        success = true;
      }
      if (success === undefined) {
        testFailed("bad first line in " + file + ":" + firstLine);
        continue;
      }
      if (!wtu.startsWith(firstLine, "// ")) {
        testFailed("bad first line in " + file + ":" + firstLine);
        continue;
      }
      passMsg = passMsg + (passMsg.length ? ", " : "") + firstLine.substr(3);
      if (wtu.endsWith(file, ".vert")) {
        info.vShaderSource = shaderSource;
        info.vShaderSuccess = success;
      } else if (wtu.endsWith(file, ".frag")) {
        info.fShaderSource = shaderSource;
        info.fShaderSuccess = success;
      }
    }
    info.linkSuccess = info.vShaderSuccess && info.fShaderSuccess;
    info.passMsg = passMsg;
    shaderInfos.push(info);
  }
  return shaderInfos;
}

function getSource(elem) {
  var str = elem.text;
  return str.replace(/^\s*/, '').replace(/\s*$/, '');
}

function getPassMessage(source) {
  var lines = source.split('\n');
  return lines[0].substring(3);
}

function getSuccess(msg) {
  if (msg.indexOf("fail") >= 0) {
    return false;
  }
  if (msg.indexOf("succeed") >= 0) {
    return true;
  }
  testFailed("bad test description. Must have 'fail' or 'success'");
}

function runTest() {
  var vShaderElem = document.getElementById('vertexShader');
  var vShaderSource = defaultVertexShader;
  var vShaderSuccess = true;

  var fShaderElem = document.getElementById('fragmentShader');
  var fShaderSource = defaultFragmentShader;
  var fShaderSuccess = true;

  var passMsg = undefined;

  if (vShaderElem) {
    vShaderSource = getSource(vShaderElem);
    passMsg = getPassMessage(vShaderSource);
    vShaderSuccess = getSuccess(passMsg);
  }

  if (fShaderElem) {
    fShaderSource = getSource(fShaderElem);
    passMsg = getPassMessage(fShaderSource);
    fShaderSuccess = getSuccess(passMsg);
  }

  var linkSuccess = vShaderSuccess && fShaderSuccess;

  if (passMsg === undefined) {
    testFailed("no test shader found.");
    finishTest();
    return;
  }

  var info = {
    vShaderSource: vShaderSource,
    vShaderSuccess: vShaderSuccess,
    fShaderSource: fShaderSource,
    fShaderSuccess: fShaderSuccess,
    linkSuccess: linkSuccess,
    passMsg: passMsg
  };
  description(passMsg);
  runTests([info]);
}

return {
  runTest: runTest,
  runTests: runTests,
  loadExternalShaders: loadExternalShaders,

  none: false,
};
}());
