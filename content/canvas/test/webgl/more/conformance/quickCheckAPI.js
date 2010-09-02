/*
  QuickCheck tests for WebGL:

    1. Write a valid arg generator for each function
      1.1. Write valid arg predicates to use with random generator:
            if value passes generator, accept it as valid.
      1.2. Often needs initializing and cleanup:
            setup - generate - cleanup
            gl.createBuffer - test(bindBufferGenerator) - gl.deleteBuffer

    2. Write an invalid arg generator
      2.1. Take valid args, modify an arg until the args no longer pass
            checkArgValidity.
      2.2. Repeat for all args.

    3. Test functions using the generators
      3.1. Args generated with the valid arg generator should pass
            assertOk(f(args))
      3.2. Args generated with the invalid arg generator should pass
            assertFail(f(args))
*/
var GLcanvas = document.createElement('canvas');
var canvas2D = document.createElement('canvas');
GLcanvas.width = GLcanvas.height = 256;
GL = GLcanvas.getContext(GL_CONTEXT_ID);
Array.from = function(o) {
  var a = [];
  for (var i=0; i<o.length; i++)
    a.push(o[i]);
  return a;
}
Array.prototype.has = function(v) { return this.indexOf(v) != -1; }
Array.prototype.random = function() { return this[randomInt(this.length)]; }

castToInt = function(o) {
  if (typeof o == 'number')
    return isNaN(o) ? 0 : Math.floor(o);
  if (o == true) return 1;
  return 0;
};

// Creates a constant checker / generator from its arguments.
//
// E.g. if you want a constant checker for the constants 1, 2, and 3, you
// would do the following:
//
//   var cc = constCheck(1,2,3);
//   var randomConst = cc.random();
//   if (cc.has(randomConst))
//     console.log("randomConst is included in cc's constants");
//
constCheck = function() {
  var a = Array.from(arguments);
  a.has = function(v) { return this.indexOf(castToInt(v)) != -1; };
  return a;
}

bindTextureTarget = constCheck(GL.TEXTURE_2D, GL.TEXTURE_CUBE_MAP);
blendEquationMode = constCheck(GL.FUNC_ADD, GL.FUNC_SUBTRACT, GL.FUNC_REVERSE_SUBTRACT);
blendFuncSfactor = constCheck(
  GL.ZERO, GL.ONE, GL.SRC_COLOR, GL.ONE_MINUS_SRC_COLOR, GL.DST_COLOR,
  GL.ONE_MINUS_DST_COLOR, GL.SRC_ALPHA, GL.ONE_MINUS_SRC_ALPHA, GL.DST_ALPHA,
  GL.ONE_MINUS_DST_ALPHA, GL.CONSTANT_COLOR, GL.ONE_MINUS_CONSTANT_COLOR,
  GL.CONSTANT_ALPHA, GL.ONE_MINUS_CONSTANT_ALPHA, GL.SRC_ALPHA_SATURATE
);
blendFuncDfactor = constCheck(
  GL.ZERO, GL.ONE, GL.SRC_COLOR, GL.ONE_MINUS_SRC_COLOR, GL.DST_COLOR,
  GL.ONE_MINUS_DST_COLOR, GL.SRC_ALPHA, GL.ONE_MINUS_SRC_ALPHA, GL.DST_ALPHA,
  GL.ONE_MINUS_DST_ALPHA, GL.CONSTANT_COLOR, GL.ONE_MINUS_CONSTANT_COLOR,
  GL.CONSTANT_ALPHA, GL.ONE_MINUS_CONSTANT_ALPHA
);
bufferTarget = constCheck(GL.ARRAY_BUFFER, GL.ELEMENT_ARRAY_BUFFER);
bufferMode = constCheck(GL.STREAM_DRAW, GL.STATIC_DRAW, GL.DYNAMIC_DRAW);
clearMask = constCheck(
  GL.COLOR_BUFFER_BIT | GL.DEPTH_BUFFER_BIT | GL.STENCIL_BUFFER_BIT,
  GL.COLOR_BUFFER_BIT | GL.DEPTH_BUFFER_BIT,
  GL.COLOR_BUFFER_BIT | GL.STENCIL_BUFFER_BIT,
  GL.DEPTH_BUFFER_BIT | GL.STENCIL_BUFFER_BIT,
  GL.COLOR_BUFFER_BIT, GL.DEPTH_BUFFER_BIT, GL.STENCIL_BUFFER_BIT, 0
);
cullFace = constCheck(GL.FRONT, GL.BACK, GL.FRONT_AND_BACK);
depthFuncFunc = constCheck(
  GL.NEVER, GL.LESS, GL.EQUAL, GL.LEQUAL, GL.GREATER, GL.NOTEQUAL,
  GL.GEQUAL, GL.ALWAYS
);
stencilFuncFunc = depthFuncFunc;
enableCap = constCheck(
  GL.BLEND, GL.CULL_FACE, GL.DEPTH_TEST, GL.DITHER, GL.POLYGON_OFFSET_FILL,
  GL.SAMPLE_ALPHA_TO_COVERAGE, GL.SAMPLE_COVERAGE, GL.SCISSOR_TEST,
  GL.STENCIL_TEST
);
frontFaceMode = constCheck(GL.CCW, GL.CW);
getParameterPname = constCheck(
  GL.ACTIVE_TEXTURE || "GL.ACTIVE_TEXTURE",
  GL.ALIASED_LINE_WIDTH_RANGE || "GL.ALIASED_LINE_WIDTH_RANGE",
  GL.ALIASED_POINT_SIZE_RANGE || "GL.ALIASED_POINT_SIZE_RANGE",
  GL.ALPHA_BITS || "GL.ALPHA_BITS",
  GL.ARRAY_BUFFER_BINDING || "GL.ARRAY_BUFFER_BINDING",
  GL.BLEND || "GL.BLEND",
  GL.BLEND_COLOR || "GL.BLEND_COLOR",
  GL.BLEND_DST_ALPHA || "GL.BLEND_DST_ALPHA",
  GL.BLEND_DST_RGB || "GL.BLEND_DST_RGB",
  GL.BLEND_EQUATION_ALPHA || "GL.BLEND_EQUATION_ALPHA",
  GL.BLEND_EQUATION_RGB || "GL.BLEND_EQUATION_RGB",
  GL.BLEND_SRC_ALPHA || "GL.BLEND_SRC_ALPHA",
  GL.BLEND_SRC_RGB || "GL.BLEND_SRC_RGB",
  GL.BLUE_BITS || "GL.BLUE_BITS",
  GL.COLOR_CLEAR_VALUE || "GL.COLOR_CLEAR_VALUE",
  GL.COLOR_WRITEMASK || "GL.COLOR_WRITEMASK",
  GL.COMPRESSED_TEXTURE_FORMATS || "GL.COMPRESSED_TEXTURE_FORMATS",
  GL.CULL_FACE || "GL.CULL_FACE",
  GL.CULL_FACE_MODE || "GL.CULL_FACE_MODE",
  GL.CURRENT_PROGRAM || "GL.CURRENT_PROGRAM",
  GL.DEPTH_BITS || "GL.DEPTH_BITS",
  GL.DEPTH_CLEAR_VALUE || "GL.DEPTH_CLEAR_VALUE",
  GL.DEPTH_FUNC || "GL.DEPTH_FUNC",
  GL.DEPTH_RANGE || "GL.DEPTH_RANGE",
  GL.DEPTH_TEST || "GL.DEPTH_TEST",
  GL.DEPTH_WRITEMASK || "GL.DEPTH_WRITEMASK",
  GL.DITHER || "GL.DITHER",
  GL.ELEMENT_ARRAY_BUFFER_BINDING || "GL.ELEMENT_ARRAY_BUFFER_BINDING",
  GL.FRAMEBUFFER_BINDING || "GL.FRAMEBUFFER_BINDING",
  GL.FRONT_FACE || "GL.FRONT_FACE",
  GL.GENERATE_MIPMAP_HINT || "GL.GENERATE_MIPMAP_HINT",
  GL.GREEN_BITS || "GL.GREEN_BITS",
  GL.LINE_WIDTH || "GL.LINE_WIDTH",
  GL.MAX_COMBINED_TEXTURE_IMAGE_UNITS || "GL.MAX_COMBINED_TEXTURE_IMAGE_UNITS",
  GL.MAX_CUBE_MAP_TEXTURE_SIZE || "GL.MAX_CUBE_MAP_TEXTURE_SIZE",
  GL.MAX_FRAGMENT_UNIFORM_VECTORS || "GL.MAX_FRAGMENT_UNIFORM_VECTORS",
  GL.MAX_RENDERBUFFER_SIZE || "GL.MAX_RENDERBUFFER_SIZE",
  GL.MAX_TEXTURE_IMAGE_UNITS || "GL.MAX_TEXTURE_IMAGE_UNITS",
  GL.MAX_TEXTURE_SIZE || "GL.MAX_TEXTURE_SIZE",
  GL.MAX_VARYING_VECTORS || "GL.MAX_VARYING_VECTORS",
  GL.MAX_VERTEX_ATTRIBS || "GL.MAX_VERTEX_ATTRIBS",
  GL.MAX_VERTEX_TEXTURE_IMAGE_UNITS || "GL.MAX_VERTEX_TEXTURE_IMAGE_UNITS",
  GL.MAX_VERTEX_UNIFORM_VECTORS || "GL.MAX_VERTEX_UNIFORM_VECTORS",
  GL.MAX_VIEWPORT_DIMS || "GL.MAX_VIEWPORT_DIMS",
  GL.NUM_COMPRESSED_TEXTURE_FORMATS || "GL.NUM_COMPRESSED_TEXTURE_FORMATS",
  GL.PACK_ALIGNMENT || "GL.PACK_ALIGNMENT",
  GL.POLYGON_OFFSET_FACTOR || "GL.POLYGON_OFFSET_FACTOR",
  GL.POLYGON_OFFSET_FILL || "GL.POLYGON_OFFSET_FILL",
  GL.POLYGON_OFFSET_UNITS || "GL.POLYGON_OFFSET_UNITS",
  GL.RED_BITS || "GL.RED_BITS",
  GL.RENDERBUFFER_BINDING || "GL.RENDERBUFFER_BINDING",
  GL.SAMPLE_BUFFERS || "GL.SAMPLE_BUFFERS",
  GL.SAMPLE_COVERAGE_INVERT || "GL.SAMPLE_COVERAGE_INVERT",
  GL.SAMPLE_COVERAGE_VALUE || "GL.SAMPLE_COVERAGE_VALUE",
  GL.SAMPLES || "GL.SAMPLES",
  GL.SCISSOR_BOX || "GL.SCISSOR_BOX",
  GL.SCISSOR_TEST || "GL.SCISSOR_TEST",
  GL.STENCIL_BACK_FAIL || "GL.STENCIL_BACK_FAIL",
  GL.STENCIL_BACK_FUNC || "GL.STENCIL_BACK_FUNC",
  GL.STENCIL_BACK_PASS_DEPTH_FAIL || "GL.STENCIL_BACK_PASS_DEPTH_FAIL",
  GL.STENCIL_BACK_PASS_DEPTH_PASS || "GL.STENCIL_BACK_PASS_DEPTH_PASS",
  GL.STENCIL_BACK_REF || "GL.STENCIL_BACK_REF",
  GL.STENCIL_BACK_VALUE_MASK || "GL.STENCIL_BACK_VALUE_MASK",
  GL.STENCIL_BACK_WRITEMASK || "GL.STENCIL_BACK_WRITEMASK",
  GL.STENCIL_BITS || "GL.STENCIL_BITS",
  GL.STENCIL_CLEAR_VALUE || "GL.STENCIL_CLEAR_VALUE",
  GL.STENCIL_FAIL || "GL.STENCIL_FAIL",
  GL.STENCIL_FUNC || "GL.STENCIL_FUNC",
  GL.STENCIL_PASS_DEPTH_FAIL || "GL.STENCIL_PASS_DEPTH_FAIL",
  GL.STENCIL_PASS_DEPTH_PASS || "GL.STENCIL_PASS_DEPTH_PASS",
  GL.STENCIL_REF || "GL.STENCIL_REF",
  GL.STENCIL_TEST || "GL.STENCIL_TEST",
  GL.STENCIL_VALUE_MASK || "GL.STENCIL_VALUE_MASK",
  GL.STENCIL_WRITEMASK || "GL.STENCIL_WRITEMASK",
  GL.SUBPIXEL_BITS || "GL.SUBPIXEL_BITS",
  GL.TEXTURE_BINDING_2D || "GL.TEXTURE_BINDING_2D",
  GL.TEXTURE_BINDING_CUBE_MAP || "GL.TEXTURE_BINDING_CUBE_MAP",
  GL.UNPACK_ALIGNMENT || "GL.UNPACK_ALIGNMENT",
  GL.VIEWPORT || "GL.VIEWPORT"
);
mipmapHint = constCheck(GL.FASTEST, GL.NICEST, GL.DONT_CARE);
pixelStoreiPname = constCheck(GL.PACK_ALIGNMENT, GL.UNPACK_ALIGNMENT);
pixelStoreiParam = constCheck(1,2,4,8);
shaderType = constCheck(GL.VERTEX_SHADER, GL.FRAGMENT_SHADER);
stencilOp = constCheck(GL.KEEP, GL.ZERO, GL.REPLACE, GL.INCR, GL.INCR_WRAP,
                        GL.DECR, GL.DECR_WRAP, GL.INVERT);
texImageTarget = constCheck(
  GL.TEXTURE_2D,
  GL.TEXTURE_CUBE_MAP_POSITIVE_X,
  GL.TEXTURE_CUBE_MAP_NEGATIVE_X,
  GL.TEXTURE_CUBE_MAP_POSITIVE_Y,
  GL.TEXTURE_CUBE_MAP_NEGATIVE_Y,
  GL.TEXTURE_CUBE_MAP_POSITIVE_Z,
  GL.TEXTURE_CUBE_MAP_NEGATIVE_Z
);
texImageInternalFormat = constCheck(
  GL.ALPHA,
  GL.LUMINANCE,
  GL.LUMINANCE_ALPHA,
  GL.RGB,
  GL.RGBA
);
texImageFormat = constCheck(
  GL.ALPHA,
  GL.LUMINANCE,
  GL.LUMINANCE_ALPHA,
  GL.RGB,
  GL.RGBA
);
texImageType = constCheck(GL.UNSIGNED_BYTE);
texParameterPname = constCheck(
  GL.TEXTURE_MIN_FILTER, GL.TEXTURE_MAG_FILTER,
  GL.TEXTURE_WRAP_S, GL.TEXTURE_WRAP_T);
texParameterParam = {};
texParameterParam[GL.TEXTURE_MIN_FILTER] = constCheck(
  GL.NEAREST, GL.LINEAR, GL.NEAREST_MIPMAP_NEAREST, GL.LINEAR_MIPMAP_NEAREST,
  GL.NEAREST_MIPMAP_LINEAR, GL.LINEAR_MIPMAP_LINEAR);
texParameterParam[GL.TEXTURE_MAG_FILTER] = constCheck(GL.NEAREST, GL.LINEAR);
texParameterParam[GL.TEXTURE_WRAP_S] = constCheck(
  GL.CLAMP_TO_EDGE, GL.MIRRORED_REPEAT, GL.REPEAT);
texParameterParam[GL.TEXTURE_WRAP_T] = texParameterParam[GL.TEXTURE_WRAP_S];
textureUnit = constCheck.apply(this, (function(){
  var textureUnits = [];
  var texUnits = GL.getParameter(GL.MAX_TEXTURE_IMAGE_UNITS);
  for (var i=0; i<texUnits; i++) textureUnits.push(GL['TEXTURE'+i]);
  return textureUnits;
})());

var StencilBits = GL.getParameter(GL.STENCIL_BITS);
var MaxStencilValue = 1 << StencilBits;

var MaxVertexAttribs = GL.getParameter(GL.MAX_VERTEX_ATTRIBS);
var LineWidthRange = GL.getParameter(GL.ALIASED_LINE_WIDTH_RANGE);

// Returns true if bufData can be passed to GL.bufferData
isBufferData = function(bufData) {
  if (typeof bufData == 'number')
    return bufData >= 0;
  if (bufData instanceof ArrayBuffer)
    return true;
  return WebGLArrayTypes.some(function(t) {
    return bufData instanceof t;
  });
};

isVertexAttribute = function(idx) {
  if (typeof idx != 'number') return false;
  return idx >= 0 && idx < MaxVertexAttribs;
};

isValidName = function(name) {
  if (typeof name != 'string') return false;
  for (var i=0; i<name.length; i++) {
    var c = name.charCodeAt(i);
    if (c & 0x00FF == 0 || c & 0xFF00 == 0) {
      return false;
    }
  }
  return true;
};

WebGLArrayTypes = [
  Float32Array,
  Int32Array,
  Int16Array,
  Int8Array,
  Uint32Array,
  Uint16Array,
  Uint8Array
];
webGLArrayContentGenerators = [randomLength, randomSmallIntArray];
randomWebGLArray = function() {
  var t = WebGLArrayTypes.random();
  return new t(webGLArrayContentGenerators.random()());
};

randomArrayBuffer = function(buflen) {
  if (buflen == null) buflen = 256;
  var len = randomInt(buflen)+1;
  var rv;
  try {
    rv = new ArrayBuffer(len);
  } catch(e) {
    log("Error creating ArrayBuffer with length " + len);
    throw(e);
  }
  return rv;
};

bufferDataGenerators = [randomLength, randomWebGLArray, randomArrayBuffer];
randomBufferData = function() {
  return bufferDataGenerators.random()();
};

randomSmallWebGLArray = function(buflen) {
  var t = WebGLArrayTypes.random();
  return new t(randomInt(buflen/4)+1);
};

bufferSubDataGenerators = [randomSmallWebGLArray, randomArrayBuffer];
randomBufferSubData = function(buflen) {
  var data = bufferSubDataGenerators.random()(buflen);
  var offset = randomInt(buflen - data.byteLength);
  return {data:data, offset:offset};
};

randomColor = function() {
  return [Math.random(), Math.random(), Math.random(), Math.random()];
};

randomName = function() {
  var arr = [];
  var len = randomLength()+1;
  for (var i=0; i<len; i++) {
    var l = randomInt(255)+1;
    var h = randomInt(255)+1;
    var c = (h << 8) | l;
    arr.push(String.fromCharCode(c));
  }
  return arr.join('');
};
randomVertexAttribute = function() {
  return randomInt(MaxVertexAttribs);
};

randomBool = function() { return Math.random() > 0.5; };

randomStencil = function() {
  return randomInt(MaxStencilValue);
};

randomLineWidth = function() {
  var lo = LineWidthRange[0],
      hi = LineWidthRange[1];
  return randomFloatFromRange(lo, hi);
};

randomImage = function(w,h) {
  var img;
  var r = Math.random();
  if (r < 0.5) {
    img = document.createElement('canvas');
    img.width = w; img.height = h;
    img.getContext('2d').fillRect(0,0,w,h);
  } else if (r < 0.5) {
    img = document.createElement('video');
    img.width = w; img.height = h;
  } else if (r < 0.75) {
    img = document.createElement('img');
    img.width = w; img.height = h;
  } else {
    img = canvas2D.getContext('2d').createImageData(w,h);
  }
  return img
};

// ArgGenerators contains argument generators for WebGL functions.
// The argument generators are used for running random tests against the WebGL
// functions.
//
// ArgGenerators is an object consisting of functionName : argGen -properties.
//
// functionName is a WebGL context function name and the argGen is an argument
// generator object that encapsulates the requirements to run
// randomly generated tests on the WebGL function.
//
// An argGen object has the following methods:
//   - setup    -- set up state for testing the GL function, returns values
//                 that need cleanup in teardown. Run once before entering a
//                 test loop.
//   - teardown -- do cleanup on setup's return values after testing is complete
//   - generate -- generate a valid set of random arguments for the GL function
//   - returnValueCleanup -- do cleanup on value returned by the tested GL function
//   - cleanup  -- do cleanup on generated arguments from generate
//   - checkArgValidity -- check if passed args are valid. Has a call signature
//                         that matches generate's return value. Returns true
//                         if args are valid, false if not.
//
//   Example test loop that demonstrates how the function args and return
//   values flow together:
//
//   var setupArgs = argGen.setup();
//   for (var i=0; i<numberOfTests; i++) {
//     var generatedArgs = argGen.generate.apply(argGen, setupArgs);
//     var validArgs = argGen.checkArgValidity.apply(argGen, generatedArgs);
//     var rv = call the GL function with generatedArgs;
//     argGen.returnValueCleanup(rv);
//     argGen.cleanup.apply(argGen, generatedArgs);
//   }
//   argGen.teardown.apply(argGen, setupArgs);
//
ArgGenerators = {

// GL functions in alphabetical order

// A

  activeTexture : {
    generate : function() { return [textureUnit.random()]; },
    checkArgValidity : function(t) { return textureUnit.has(t); },
    teardown : function() { GL.activeTexture(GL.TEXTURE0); }
  },
  attachShader : {
    generate : function() {
      var p = GL.createProgram();
      var sh = GL.createShader(shaderType.random());
      return [p, sh];
    },
    checkArgValidity : function(p, sh) {
      return GL.isProgram(p) && GL.isShader(sh) && !GL.getAttachedShaders(p).has(sh);
    },
    cleanup : function(p, sh) {
      try {GL.detachShader(p,sh);} catch(e) {}
      try {GL.deleteProgram(p);} catch(e) {}
      try {GL.deleteShader(sh);} catch(e) {}
    }
  },

// B

  bindAttribLocation : {
    generate : function() {
      var program = GL.createProgram();
      return [program, randomVertexAttribute(), randomName()];
    },
    checkArgValidity : function(program, index, name) {
      return GL.isProgram(program) && isVertexAttribute(index) && isValidName(name);
    },
    cleanup : function(program, index, name) {
      try { GL.deleteProgram(program); } catch(e) {}
    }
  },
  bindBuffer : {
    generate : function(buf) {
      return [bufferTarget.random(), GL.createBuffer()];
    },
    checkArgValidity : function(target, buf) {
      if (!bufferTarget.has(target))
        return false;
      GL.bindBuffer(target, buf);
      return GL.isBuffer(buf);
    },
    cleanup : function(t, buf, m) {
      GL.deleteBuffer(buf);
    }
  },
  bindFramebuffer : {
    generate : function() {
      return [GL.FRAMEBUFFER, Math.random() > 0.5 ? null : GL.createFramebuffer()];
    },
    checkArgValidity : function(target, fbo) {
      if (target != GL.FRAMEBUFFER)
        return false;
      if (fbo != null)
          GL.bindFramebuffer(target, fbo);
      return (fbo == null || GL.isFramebuffer(fbo));
    },
    cleanup : function(target, fbo) {
      GL.bindFramebuffer(target, null);
      if (fbo)
        GL.deleteFramebuffer(fbo);
    }
  },
  bindRenderbuffer : {
    generate : function() {
      return [GL.RENDERBUFFER, Math.random() > 0.5 ? null : GL.createRenderbuffer()];
    },
    checkArgValidity : function(target, rbo) {
      if (target != GL.RENDERBUFFER)
        return false;
      if (rbo != null)
        GL.bindRenderbuffer(target, rbo);
      return (rbo == null || GL.isRenderbuffer(rbo));
    },
    cleanup : function(target, rbo) {
      GL.bindRenderbuffer(target, null);
      if (rbo)
        GL.deleteRenderbuffer(rbo);
    }
  },
  bindTexture : {
    generate : function() {
      return [bindTextureTarget.random(), Math.random() > 0.5 ? null : GL.createTexture()];
    },
    checkArgValidity : function(target, o) {
      if (!bindTextureTarget.has(target))
        return false;
      if (o != null)
        GL.bindTexture(target, o);
      return (o == null || GL.isTexture(o));
    },
    cleanup : function(target, o) {
      GL.bindTexture(target, null);
      if (o)
        GL.deleteTexture(o);
    }
  },
  blendColor : {
    generate : function() { return randomColor(); },
    teardown : function() { GL.blendColor(0,0,0,0); }
  },
  blendEquation : {
    generate : function() { return [blendEquationMode.random()]; },
    checkArgValidity : function(o) { return blendEquationMode.has(o); },
    teardown : function() { GL.blendEquation(GL.FUNC_ADD); }
  },
  blendEquationSeparate : {
    generate : function() {
      return [blendEquationMode.random(), blendEquationMode.random()];
    },
    checkArgValidity : function(o,p) {
      return blendEquationMode.has(o) && blendEquationMode.has(p);
    },
    teardown : function() { GL.blendEquationSeparate(GL.FUNC_ADD, GL.FUNC_ADD); }
  },
  blendFunc : {
    generate : function() {
      return [blendFuncSfactor.random(), blendFuncDfactor.random()];
    },
    checkArgValidity : function(s,d) {
      return blendFuncSfactor.has(s) && blendFuncDfactor.has(d);
    },
    teardown : function() { GL.blendFunc(GL.ONE, GL.ZERO); }
  },
  blendFuncSeparate : {
    generate : function() {
      return [blendFuncSfactor.random(), blendFuncDfactor.random(),
              blendFuncSfactor.random(), blendFuncDfactor.random()];
    },
    checkArgValidity : function(s,d,as,ad) {
      return blendFuncSfactor.has(s) && blendFuncDfactor.has(d) &&
              blendFuncSfactor.has(as) && blendFuncDfactor.has(ad) ;
    },
    teardown : function() {
      GL.blendFuncSeparate(GL.ONE, GL.ZERO, GL.ONE, GL.ZERO);
    }
  },
  bufferData : {
    setup : function() {
      var buf = GL.createBuffer();
      var ebuf = GL.createBuffer();
      GL.bindBuffer(GL.ARRAY_BUFFER, buf);
      GL.bindBuffer(GL.ELEMENT_ARRAY_BUFFER, ebuf);
      return [buf, ebuf];
    },
    generate : function(buf, ebuf) {
      return [bufferTarget.random(), randomBufferData(), bufferMode.random()];
    },
    checkArgValidity : function(target, bufData, mode) {
      return bufferTarget.has(target) && isBufferData(bufData) && bufferMode.has(mode);
    },
    teardown : function(buf, ebuf) {
      GL.deleteBuffer(buf);
      GL.deleteBuffer(ebuf);
    },
  },
  bufferSubData : {
    setup : function() {
      var buf = GL.createBuffer();
      var ebuf = GL.createBuffer();
      GL.bindBuffer(GL.ARRAY_BUFFER, buf);
      GL.bufferData(GL.ARRAY_BUFFER, 256, GL.STATIC_DRAW);
      GL.bindBuffer(GL.ELEMENT_ARRAY_BUFFER, ebuf);
      GL.bufferData(GL.ELEMENT_ARRAY_BUFFER, 256, GL.STATIC_DRAW);
      return [buf, ebuf];
    },
    generate : function(buf, ebuf) {
      var d = randomBufferSubData(256);
      return [bufferTarget.random(), d.offset, d.data];
    },
    checkArgValidity : function(target, offset, data) {
      return bufferTarget.has(target) && offset >= 0 && data.byteLength >= 0 && offset + data.byteLength <= 256;
    },
    teardown : function(buf, ebuf) {
      GL.deleteBuffer(buf);
      GL.deleteBuffer(ebuf);
    },
  },

// C

  checkFramebufferStatus : {
    generate : function() {
      return [Math.random() > 0.5 ? null : GL.createFramebuffer()];
    },
    checkArgValidity : function(fbo) {
      if (fbo != null)
        GL.bindFramebuffer(GL.FRAMEBUFFER, fbo);
      return fbo == null || GL.isFramebuffer(fbo);
    },
    cleanup : function(fbo){
      GL.bindFramebuffer(GL.FRAMEBUFFER, null);
      if (fbo != null)
        try{ GL.deleteFramebuffer(fbo); } catch(e) {}
    }
  },
  clear : {
    generate : function() { return [clearMask.random()]; },
    checkArgValidity : function(mask) { return clearMask.has(mask); }
  },
  clearColor : {
    generate : function() { return randomColor(); },
    teardown : function() { GL.clearColor(0,0,0,0); }
  },
  clearDepth : {
    generate : function() { return [Math.random()]; },
    teardown : function() { GL.clearDepth(1); }
  },
  clearStencil : {
    generate : function() { return [randomStencil()]; },
    teardown : function() { GL.clearStencil(0); }
  },
  colorMask : {
    generate : function() {
      return [randomBool(), randomBool(), randomBool(), randomBool()];
    },
    teardown : function() { GL.colorMask(true, true, true, true); }
  },
  compileShader : {}, // FIXME
  copyTexImage2D : {}, // FIXME
  copyTexSubImage2D : {}, // FIXME
  createBuffer : {
    generate : function() { return []; },
    returnValueCleanup : function(o) { GL.deleteBuffer(o); }
  },
  createFramebuffer : {
    generate : function() { return []; },
    returnValueCleanup : function(o) { GL.deleteFramebuffer(o); }
  },
  createProgram : {
    generate : function() { return []; },
    returnValueCleanup : function(o) { GL.deleteProgram(o); }
  },
  createRenderbuffer : {
    generate : function() { return []; },
    returnValueCleanup : function(o) { GL.deleteRenderbuffer(o); }
  },
  createShader : {
    generate : function() { return [shaderType.random()]; },
    checkArgValidity : function(t) { return shaderType.has(t); },
    returnValueCleanup : function(o) { GL.deleteShader(o); }
  },
  createTexture : {
    generate : function() { return []; },
    returnValueCleanup : function(o) { GL.deleteTexture(o); }
  },
  cullFace : {
    generate : function() { return [cullFace.random()]; },
    checkArgValidity : function(f) { return cullFace.has(f); },
    teardown : function() { GL.cullFace(GL.BACK); }
  },

// D

  deleteBuffer : {
    generate : function() { return [GL.createBuffer()]; },
    checkArgValidity : function(o) {
      GL.bindBuffer(GL.ARRAY_BUFFER, o);
      return GL.isBuffer(o);
    },
    cleanup : function(o) {
      GL.bindBuffer(GL.ARRAY_BUFFER, null);
      try { GL.deleteBuffer(o); } catch(e) {}
    }
  },
  deleteFramebuffer : {
    generate : function() { return [GL.createFramebuffer()]; },
    checkArgValidity : function(o) {
      GL.bindFramebuffer(GL.FRAMEBUFFER, o);
      return GL.isFramebuffer(o);
    },
    cleanup : function(o) {
      GL.bindFramebuffer(GL.FRAMEBUFFER, null);
      try { GL.deleteFramebuffer(o); } catch(e) {}
    }
  },
  deleteProgram : {
    generate : function() { return [GL.createProgram()]; },
    checkArgValidity : function(o) { return GL.isProgram(o); },
    cleanup : function(o) { try { GL.deleteProgram(o); } catch(e) {} }
  },
  deleteRenderbuffer : {
    generate : function() { return [GL.createRenderbuffer()]; },
    checkArgValidity : function(o) {
      GL.bindRenderbuffer(GL.RENDERBUFFER, o);
      return GL.isRenderbuffer(o);
    },
    cleanup : function(o) {
      GL.bindRenderbuffer(GL.RENDERBUFFER, null);
      try { GL.deleteRenderbuffer(o); } catch(e) {}
    }
  },
  deleteShader : {
    generate : function() { return [GL.createShader(shaderType.random())]; },
    checkArgValidity : function(o) { return GL.isShader(o); },
    cleanup : function(o) { try { GL.deleteShader(o); } catch(e) {} }
  },
  deleteTexture : {
    generate : function() { return [GL.createTexture()]; },
    checkArgValidity : function(o) {
      GL.bindTexture(GL.TEXTURE_2D, o);
      return GL.isTexture(o);
    },
    cleanup : function(o) {
      GL.bindTexture(GL.TEXTURE_2D, null);
      try { GL.deleteTexture(o); } catch(e) {}
    }
  },
  depthFunc : {
    generate : function() { return [depthFuncFunc.random()]; },
    checkArgValidity : function(f) { return depthFuncFunc.has(f); },
    teardown : function() { GL.depthFunc(GL.LESS); }
  },
  depthMask : {
    generate : function() { return [randomBool()]; },
    teardown : function() { GL.depthFunc(GL.TRUE); }
  },
  depthRange : {
    generate : function() { return [Math.random(), Math.random()]; },
    teardown : function() { GL.depthRange(0, 1); }
  },
  detachShader : {
    generate : function() {
      var p = GL.createProgram();
      var sh = GL.createShader(shaderType.random());
      GL.attachShader(p, sh);
      return [p, sh];
    },
    checkArgValidity : function(p, sh) {
      return GL.isProgram(p) && GL.isShader(sh) && GL.getAttachedShaders(p).has(sh);
    },
    cleanup : function(p, sh) {
      try {GL.deleteProgram(p);} catch(e) {}
      try {GL.deleteShader(sh);} catch(e) {}
    }
  },
  disable : {
    generate : function() { return [enableCap.random()]; },
    checkArgValidity : function(c) { return enableCap.has(c); },
    cleanup : function(c) { if (c == GL.DITHER) GL.enable(c); }
  },
  disableVertexAttribArray : {
    generate : function() { return [randomVertexAttribute()]; },
    checkArgValidity : function(v) { return isVertexAttribute(v); }
  },
  drawArrays : {}, // FIXME
  drawElements : {}, // FIXME

// E

  enable : {
    generate : function() { return [enableCap.random()]; },
    checkArgValidity : function(c) { return enableCap.has(c); },
    cleanup : function(c) { if (c != GL.DITHER) GL.disable(c); }
  },
  enableVertexAttribArray : {
    generate : function() { return [randomVertexAttribute()]; },
    checkArgValidity : function(v) { return isVertexAttribute(castToInt(v)); },
    cleanup : function(v) { GL.disableVertexAttribArray(v); }
  },

// F

  finish : {
    generate : function() { return []; }
  },
  flush : {
    generate : function() { return []; }
  },
  framebufferRenderbuffer : {}, // FIXME
  framebufferTexture2D : {}, // FIXME
  frontFace : {
    generate : function() { return [frontFaceMode.random()]; },
    checkArgValidity : function(c) { return frontFaceMode.has(c); },
    cleanup : function(c) { GL.frontFace(GL.CCW); }
  },

// G

  generateMipmap : {
    setup : function() {
      var tex = GL.createTexture();
      var tex2 = GL.createTexture();
      GL.bindTexture(GL.TEXTURE_2D, tex);
      GL.bindTexture(GL.TEXTURE_CUBE_MAP, tex2);
      var pix = new Uint8Array(16*16*4);
      GL.texImage2D(GL.TEXTURE_2D, 0, GL.RGBA, 16, 16, 0, GL.RGBA, GL.UNSIGNED_BYTE, pix);
      GL.texImage2D(GL.TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL.RGBA, 16, 16, 0, GL.RGBA, GL.UNSIGNED_BYTE, pix);
      GL.texImage2D(GL.TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL.RGBA, 16, 16, 0, GL.RGBA, GL.UNSIGNED_BYTE, pix);
      GL.texImage2D(GL.TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL.RGBA, 16, 16, 0, GL.RGBA, GL.UNSIGNED_BYTE, pix);
      GL.texImage2D(GL.TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL.RGBA, 16, 16, 0, GL.RGBA, GL.UNSIGNED_BYTE, pix);
      GL.texImage2D(GL.TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL.RGBA, 16, 16, 0, GL.RGBA, GL.UNSIGNED_BYTE, pix);
      GL.texImage2D(GL.TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL.RGBA, 16, 16, 0, GL.RGBA, GL.UNSIGNED_BYTE, pix);
    },
    generate : function() { return [bindTextureTarget.random()]; },
    checkArgValidity : function(t) { return bindTextureTarget.has(t); },
    teardown : function(tex, tex2) {
      GL.bindTexture(GL.TEXTURE_2D, null);
      GL.bindTexture(GL.TEXTURE_CUBE_MAP, null);
      GL.deleteTexture(tex);
      GL.deleteTexture(tex2);
    }
  },
  getActiveAttrib : {
  /* FIXME the queried attrib needs to be an active one
    generate : function() {
      var program = GL.createProgram();
      return [program, randomVertexAttribute()];
    },
    checkArgValidity : function(program, index) {
      return GL.isProgram(program) && isVertexAttribute(index);
    },
    cleanup : function(program, index) {
      GL.deleteProgram(program);
    }
  */
  },
  getActiveUniform : {}, // FIXME
  getAttachedShaders : {
    setup : function() {
      var program = GL.createProgram();
      var s1 = GL.createShader(GL.VERTEX_SHADER);
      var s2 = GL.createShader(GL.FRAGMENT_SHADER);
      GL.attachShader(program, s1);
      GL.attachShader(program, s2);
      return [program, s1, s2];
    },
    generate : function(program, s1, s2) {
      return [program]
    },
    checkArgValidity : function(program) {
      return GL.isProgram(program);
    },
    teardown : function(program, s1, s2) {
      GL.deleteProgram(program);
      GL.deleteShader(s1);
      GL.deleteShader(s2);
    }
  },
  getAttribLocation : {
    generate : function() {
      var program = GL.createProgram();
      var name = randomName();
      GL.bindAttribLocation(program, randomVertexAttribute(), name);
      return [program, name];
    },
    checkArgValidity : function(program, name) {
      return GL.isProgram(program) && isValidName(name);
    },
    cleanup : function(program, name) {
      try { GL.deleteProgram(program); } catch(e) {}
    }
  },
  getParameter : {
// FIXME disabled because crashes, see bug 576620
//    generate : function() { return [getParameterPname.random()]; },
//    checkArgValidity : function(p) { return getParameterPname.has(p); }
  },
  getBufferParameter : {}, // FIXME
  getError : {
    generate : function() { return []; }
  },
  getFramebufferAttachmentParameter : {}, // FIXME
  getProgramParameter : {}, // FIXME
  getProgramInfoLog : {}, // FIXME
  getRenderbufferParameter : {}, // FIXME
  getShaderParameter : {}, // FIXME
  getShaderInfoLog : {}, // FIXME
  getShaderSource : {}, // FIXME
  getString : {}, // FIXME
  getTexParameter : {}, // FIXME
  getUniform : {}, // FIXME
  getUniformLocation : {}, // FIXME
  getVertexAttrib : {}, // FIXME
  getVertexAttribOffset : {}, // FIXME

// H

  hint : {
    generate : function() { return [GL.GENERATE_MIPMAP_HINT, mipmapHint.random()]; },
    checkValidArgs : function(h, m) {
      return h == GL.GENERATE_MIPMAP_HINT && mipmapHint.has(m);
    },
    teardown : function(){ GL.hint(GL.GENERATE_MIPMAP_HINT, GL.DONT_CARE); }
  },

// I

  isBuffer : {
    generate : function() { return [GL.createBuffer()]; },
    cleanup : function(o) { try { GL.deleteBuffer(o); } catch(e) {} }
  },
  isEnabled : {
    generate : function() { return [enableCap.random()]; },
    checkArgValidity : function(c) { return enableCap.has(c); }
  },
  isFramebuffer : {
    generate : function() { return [GL.createFramebuffer()]; },
    cleanup : function(o) { try { GL.deleteFramebuffer(o); } catch(e) {} }
  },
  isProgram : {
    generate : function() { return [GL.createProgram()]; },
    cleanup : function(o) { try { GL.deleteProgram(o); } catch(e) {} }
  },
  isRenderbuffer : {
    generate : function() { return [GL.createRenderbuffer()]; },
    cleanup : function(o) { try { GL.deleteRenderbuffer(o); } catch(e) {} }
  },
  isShader : {
    generate : function() { return [GL.createShader(shaderType.random())]; },
    cleanup : function(o) { try { GL.deleteShader(o); } catch(e) {} }
  },
  isTexture : {
    generate : function() { return [GL.createTexture()]; },
    cleanup : function(o) { try { GL.deleteTexture(o); } catch(e) {} }
  },

// L

  lineWidth : {
    generate : function() { return [randomLineWidth()]; },
    teardown : function() { GL.lineWidth(1); }
  },
  linkProgram : {}, // FIXME

// P
  pixelStorei : {
    generate : function() {
      return [pixelStoreiPname.random(), pixelStoreiParam.random()];
    },
    checkArgValidity : function(pname, param) {
      return pixelStoreiPname.has(pname) && pixelStoreiParam.has(param);
    },
    teardown : function() {
      GL.pixelStorei(GL.PACK_ALIGNMENT, 4);
      GL.pixelStorei(GL.UNPACK_ALIGNMENT, 4);
    }
  },
  polygonOffset : {
    generate : function() { return [randomFloat(), randomFloat()]; },
    teardown : function() { GL.polygonOffset(0,0); }
  },

// R

  readPixels : {}, // FIXME
  renderbufferStorage : {}, // FIXME

// S

  sampleCoverage : {
    generate : function() { return [randomFloatFromRange(0,1), randomBool()] },
    teardown : function() { GL.sampleCoverage(1, false); }
  },
  scissor : {
    generate : function() {
      return [randomInt(3000)-1500, randomInt(3000)-1500, randomIntFromRange(0,3000), randomIntFromRange(0,3000)];
    },
    checkArgValidity : function(x,y,w,h) {
      return castToInt(w) >= 0 && castToInt(h) >= 0;
    },
    teardown : function() {
      GL.scissor(0,0,GL.canvas.width, GL.canvas.height);
    }
  },
  shaderSource : {}, // FIXME
  stencilFunc : {
    generate : function(){
      return [stencilFuncFunc.random(), randomInt(MaxStencilValue), randomInt(0xffffffff)];
    },
    checkArgValidity : function(func, ref, mask) {
      return stencilFuncFunc.has(func) && castToInt(ref) >= 0 && castToInt(ref) < MaxStencilValue;
    },
    teardown : function() {
      GL.stencilFunc(GL.ALWAYS, 0, 0xffffffff);
    }
  },
  stencilFuncSeparate : {
    generate : function(){
      return [cullFace.random(), stencilFuncFunc.random(), randomInt(MaxStencilValue), randomInt(0xffffffff)];
    },
    checkArgValidity : function(face, func, ref, mask) {
      return cullFace.has(face) && stencilFuncFunc.has(func) && castToInt(ref) >= 0 && castToInt(ref) < MaxStencilValue;
    },
    teardown : function() {
      GL.stencilFunc(GL.ALWAYS, 0, 0xffffffff);
    }
  },
  stencilMask : {
    generate : function() { return [randomInt(0xffffffff)]; },
    teardown : function() { GL.stencilMask(0xffffffff); }
  },
  stencilMaskSeparate : {
    generate : function() { return [cullFace.random(), randomInt(0xffffffff)]; },
    checkArgValidity : function(face, mask) {
      return cullFace.has(face);
    },
    teardown : function() { GL.stencilMask(0xffffffff); }
  },
  stencilOp : {
    generate : function() {
      return [stencilOp.random(), stencilOp.random(), stencilOp.random()];
    },
    checkArgValidity : function(sfail, dpfail, dppass) {
      return stencilOp.has(sfail) && stencilOp.has(dpfail) && stencilOp.has(dppass);
    },
    teardown : function() { GL.stencilOp(GL.KEEP, GL.KEEP, GL.KEEP); }
  },
  stencilOpSeparate : {
    generate : function() {
      return [cullFace.random(), stencilOp.random(), stencilOp.random(), stencilOp.random()];
    },
    checkArgValidity : function(face, sfail, dpfail, dppass) {
      return cullFace.has(face) && stencilOp.has(sfail) &&
              stencilOp.has(dpfail) && stencilOp.has(dppass);
    },
    teardown : function() { GL.stencilOp(GL.KEEP, GL.KEEP, GL.KEEP); }
  },

// T
  texImage2D : {
    noAlreadyTriedCheck : true, // Object.toSource is very slow here
    setup : function() {
      var tex = GL.createTexture();
      var tex2 = GL.createTexture();
      GL.bindTexture(GL.TEXTURE_2D, tex);
      GL.bindTexture(GL.TEXTURE_CUBE_MAP, tex2);
      return [tex, tex2];
    },
    generate : function() {
      var format = texImageFormat.random();
      if (Math.random() < 0.5) {
        var img = randomImage(16,16);
        var a = [ texImageTarget.random(), 0, format, format, GL.UNSIGNED_BYTE, img ];
        return a;
      } else {
        var pix = null;
        if (Math.random > 0.5) {
          pix = new Uint8Array(16*16*4);
        }
        return [
          texImageTarget.random(), 0,
          format, 16, 16, 0,
          format, GL.UNSIGNED_BYTE, pix
        ];
      }
    },
    checkArgValidity : function(target, level, internalformat, width, height, border, format, type, data) {
               // or : function(target, level, internalformat, format, type, image)
      if (!texImageTarget.has(target) || castToInt(level) < 0)
        return false;
      if (arguments.length <= 6) {
        var xformat = width;
        var xtype = height;
        var ximage = border;
        if ((ximage instanceof HTMLImageElement ||
             ximage instanceof HTMLVideoElement ||
             ximage instanceof HTMLCanvasElement ||
             ximage instanceof ImageData) &&
            texImageInternalFormat.has(internalformat) &&
            texImageFormat.has(xformat) &&
            texImageType.has(xtype) &&
            internalformat == xformat)
          return true;
        return false;
      }
      var w = castToInt(width), h = castToInt(height), b = castToInt(border);
      return texImageInternalFormat.has(internalformat) && w >= 0 && h >= 0 &&
            b == 0 && (data == null || data.byteLength == w*h*4) &&
            texImageFormat.has(format) && texImageType.has(type)
            && internalformat == format;
    },
    teardown : function(tex, tex2) {
      GL.bindTexture(GL.TEXTURE_2D, null);
      GL.bindTexture(GL.TEXTURE_CUBE_MAP, null);
      GL.deleteTexture(tex);
      GL.deleteTexture(tex2);
    }
  },
  texParameterf : {
    generate : function() {
      var pname = texParameterPname.random();
      var param = texParameterParam[pname].random();
      return [bindTextureTarget.random(), pname, param];
    },
    checkArgValidity : function(target, pname, param) {
      if (!bindTextureTarget.has(target))
        return false;
      if (!texParameterPname.has(pname))
        return false;
      return texParameterParam[pname].has(param);
    }
  },
  texParameteri : {
    generate : function() {
      var pname = texParameterPname.random();
      var param = texParameterParam[pname].random();
      return [bindTextureTarget.random(), pname, param];
    },
    checkArgValidity : function(target, pname, param) {
      if (!bindTextureTarget.has(target))
        return false;
      if (!texParameterPname.has(pname))
        return false;
      return texParameterParam[pname].has(param);
    }
  },
  texSubImage2D : {}, // FIXME

// U

  uniform1f : {}, // FIXME
  uniform1fv : {}, // FIXME
  uniform1i : {}, // FIXME
  uniform1iv : {}, // FIXME
  uniform2f : {}, // FIXME
  uniform2fv : {}, // FIXME
  uniform2i : {}, // FIXME
  uniform2iv : {}, // FIXME
  uniform3f : {}, // FIXME
  uniform3fv : {}, // FIXME
  uniform3i : {}, // FIXME
  uniform3iv : {}, // FIXME
  uniform4f : {}, // FIXME
  uniform4fv : {}, // FIXME
  uniform4i : {}, // FIXME
  uniform4iv : {}, // FIXME
  uniformMatrix2fv : {}, // FIXME
  uniformMatrix3fv : {}, // FIXME
  uniformMatrix4fv : {}, // FIXME
  useProgram : {}, // FIXME

// V

  validateProgram : {}, // FIXME
  vertexAttrib1f : {}, // FIXME
  vertexAttrib1fv : {}, // FIXME
  vertexAttrib2f : {}, // FIXME
  vertexAttrib2fv : {}, // FIXME
  vertexAttrib3f : {}, // FIXME
  vertexAttrib3fv : {}, // FIXME
  vertexAttrib4f : {}, // FIXME
  vertexAttrib4fv : {}, // FIXME
  vertexAttribPointer : {}, // FIXME
  viewport : {
    generate : function() {
      return [randomInt(3000)-1500, randomInt(3000)-1500, randomIntFromRange(0,3000), randomIntFromRange(0,3000)];
    },
    checkArgValidity : function(x,y,w,h) {
      return castToInt(w) >= 0 && castToInt(h) >= 0;
    },
    teardown : function() {
      GL.viewport(0,0,GL.canvas.width, GL.canvas.height);
    }
  }

};

mutateArgs = function(args) {
  var mutateCount = randomIntFromRange(1, args.length);
  var newArgs = Array.from(args);
  for (var i=0; i<mutateCount; i++) {
    var idx = randomInt(args.length);
    newArgs[idx] = generateRandomArg(idx, args.length);
  }
  return newArgs;
};

// Calls testFunction numberOfTests times with arguments generated by
// argGen.generate() (or empty arguments if no generate present).
//
// The arguments testFunction is called with are the generated args,
// the argGen, and what argGen.setup() returned or [] if argGen has not setup
// method. I.e. testFunction(generatedArgs, argGen, setupVars).
//
argGeneratorTestRunner = function(argGen, testFunction, numberOfTests) {
  // do argument generator setup if needed
  var setupVars = argGen.setup ? argGen.setup() : [];
  var error;
  for (var i=0; i<numberOfTests; i++) {
    var failed = false;
    // generate arguments if argGen has a generate method
    var generatedArgs = argGen.generate ? argGen.generate.apply(argGen, setupVars) : [];
    try {
      // call testFunction with the generated args
      testFunction.call(this, generatedArgs, argGen, setupVars);
    } catch (e) {
      failed = true;
      error = e;
    }
    // if argGen needs cleanup for generated args, do it here
    if (argGen.cleanup)
      argGen.cleanup.apply(argGen, generatedArgs);
    if (failed) break;
  }
  // if argGen needs to do a final cleanup for setupVars, do it here
  if (argGen.teardown)
    argGen.teardown.apply(argGen, setupVars);
  if (error) throw(error);
}
