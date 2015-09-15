/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu} = require("chrome");

var TiltUtils = require("devtools/tilt/tilt-utils");
var {TiltMath, mat4} = require("devtools/tilt/tilt-math");

Cu.import("resource://gre/modules/Services.jsm");

const WEBGL_CONTEXT_NAME = "experimental-webgl";


/**
 * Module containing thin wrappers around low-level WebGL functions.
 */
var TiltGL = {};
module.exports = TiltGL;

/**
 * Contains commonly used helper methods used in any 3D application.
 *
 * @param {HTMLCanvasElement} aCanvas
 *                            the canvas element used for rendering
 * @param {Function} onError
 *                   optional, function called if initialization failed
 * @param {Function} onLoad
 *                   optional, function called if initialization worked
 */
TiltGL.Renderer = function TGL_Renderer(aCanvas, onError, onLoad)
{
  /**
   * The WebGL context obtained from the canvas element, used for drawing.
   */
  this.context = TiltGL.create3DContext(aCanvas);

  // check if the context was created successfully
  if (!this.context) {
    TiltUtils.Output.alert("Firefox", TiltUtils.L10n.get("initTilt.error"));
    TiltUtils.Output.error(TiltUtils.L10n.get("initWebGL.error"));

    if ("function" === typeof onError) {
      onError();
    }
    return;
  }

  // set the default clear color and depth buffers
  this.context.clearColor(0, 0, 0, 0);
  this.context.clearDepth(1);

  /**
   * Variables representing the current framebuffer width and height.
   */
  this.width = aCanvas.width;
  this.height = aCanvas.height;
  this.initialWidth = this.width;
  this.initialHeight = this.height;

  /**
   * The current model view matrix.
   */
  this.mvMatrix = mat4.identity(mat4.create());

  /**
   * The current projection matrix.
   */
  this.projMatrix = mat4.identity(mat4.create());

  /**
   * The current fill color applied to any objects which can be filled.
   * These are rectangles, circles, boxes, 2d or 3d primitives in general.
   */
  this._fillColor = [];

  /**
   * The current stroke color applied to any objects which can be stroked.
   * This property mostly refers to lines.
   */
  this._strokeColor = [];

  /**
   * Variable representing the current stroke weight.
   */
  this._strokeWeightValue = 0;

  /**
   * A shader useful for drawing vertices with only a color component.
   */
  this._colorShader = new TiltGL.Program(this.context, {
    vs: TiltGL.ColorShader.vs,
    fs: TiltGL.ColorShader.fs,
    attributes: ["vertexPosition"],
    uniforms: ["mvMatrix", "projMatrix", "fill"]
  });

  // create helper functions to create shaders, meshes, buffers and textures
  this.Program =
    TiltGL.Program.bind(TiltGL.Program, this.context);
  this.VertexBuffer =
    TiltGL.VertexBuffer.bind(TiltGL.VertexBuffer, this.context);
  this.IndexBuffer =
    TiltGL.IndexBuffer.bind(TiltGL.IndexBuffer, this.context);
  this.Texture =
    TiltGL.Texture.bind(TiltGL.Texture, this.context);

  // set the default mvp matrices, tint, fill, stroke and other visual props.
  this.defaults();

  // the renderer was created successfully
  if ("function" === typeof onLoad) {
    onLoad();
  }
};

TiltGL.Renderer.prototype = {

  /**
   * Clears the color and depth buffers.
   */
  clear: function TGLR_clear()
  {
    let gl = this.context;
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  },

  /**
   * Sets if depth testing should be enabled or not.
   * Disabling could be useful when handling transparency (for example).
   *
   * @param {Boolean} aEnabledFlag
   *                  true if depth testing should be enabled
   */
  depthTest: function TGLR_depthTest(aEnabledFlag)
  {
    let gl = this.context;

    if (aEnabledFlag) {
      gl.enable(gl.DEPTH_TEST);
    } else {
      gl.disable(gl.DEPTH_TEST);
    }
  },

  /**
   * Sets if stencil testing should be enabled or not.
   *
   * @param {Boolean} aEnabledFlag
   *                  true if stencil testing should be enabled
   */
  stencilTest: function TGLR_stencilTest(aEnabledFlag)
  {
    let gl = this.context;

    if (aEnabledFlag) {
      gl.enable(gl.STENCIL_TEST);
    } else {
      gl.disable(gl.STENCIL_TEST);
    }
  },

  /**
   * Sets cull face, either "front", "back" or disabled.
   *
   * @param {String} aModeFlag
   *                 blending mode, either "front", "back", "both" or falsy
   */
  cullFace: function TGLR_cullFace(aModeFlag)
  {
    let gl = this.context;

    switch (aModeFlag) {
      case "front":
        gl.enable(gl.CULL_FACE);
        gl.cullFace(gl.FRONT);
        break;
      case "back":
        gl.enable(gl.CULL_FACE);
        gl.cullFace(gl.BACK);
        break;
      case "both":
        gl.enable(gl.CULL_FACE);
        gl.cullFace(gl.FRONT_AND_BACK);
        break;
      default:
        gl.disable(gl.CULL_FACE);
    }
  },

  /**
   * Specifies the orientation of front-facing polygons.
   *
   * @param {String} aModeFlag
   *                 either "cw" or "ccw"
   */
  frontFace: function TGLR_frontFace(aModeFlag)
  {
    let gl = this.context;

    switch (aModeFlag) {
      case "cw":
        gl.frontFace(gl.CW);
        break;
      case "ccw":
        gl.frontFace(gl.CCW);
        break;
    }
  },

  /**
   * Sets blending, either "alpha" or "add" (additive blending).
   * Anything else disables blending.
   *
   * @param {String} aModeFlag
   *                 blending mode, either "alpha", "add" or falsy
   */
  blendMode: function TGLR_blendMode(aModeFlag)
  {
    let gl = this.context;

    switch (aModeFlag) {
      case "alpha":
        gl.enable(gl.BLEND);
        gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);
        break;
      case "add":
        gl.enable(gl.BLEND);
        gl.blendFunc(gl.SRC_ALPHA, gl.ONE);
        break;
      default:
        gl.disable(gl.BLEND);
    }
  },

  /**
   * Helper function to activate the color shader.
   *
   * @param {TiltGL.VertexBuffer} aVerticesBuffer
   *                              a buffer of vertices positions
   * @param {Array} aColor
   *                the color fill to be used as [r, g, b, a] with 0..1 range
   * @param {Array} aMvMatrix
   *                the model view matrix
   * @param {Array} aProjMatrix
   *                the projection matrix
   */
  useColorShader: function TGLR_useColorShader(
    aVerticesBuffer, aColor, aMvMatrix, aProjMatrix)
  {
    let program = this._colorShader;

    // use this program
    program.use();

    // bind the attributes and uniforms as necessary
    program.bindVertexBuffer("vertexPosition", aVerticesBuffer);
    program.bindUniformMatrix("mvMatrix", aMvMatrix || this.mvMatrix);
    program.bindUniformMatrix("projMatrix", aProjMatrix || this.projMatrix);
    program.bindUniformVec4("fill", aColor || this._fillColor);
  },

  /**
   * Draws bound vertex buffers using the specified parameters.
   *
   * @param {Number} aDrawMode
   *                 WebGL enum, like TRIANGLES
   * @param {Number} aCount
   *                 the number of indices to be rendered
   */
  drawVertices: function TGLR_drawVertices(aDrawMode, aCount)
  {
    this.context.drawArrays(aDrawMode, 0, aCount);
  },

  /**
   * Draws bound vertex buffers using the specified parameters.
   * This function also makes use of an index buffer.
   *
   * @param {Number} aDrawMode
   *                 WebGL enum, like TRIANGLES
   * @param {TiltGL.IndexBuffer} aIndicesBuffer
   *                             indices for the vertices buffer
   */
  drawIndexedVertices: function TGLR_drawIndexedVertices(
    aDrawMode, aIndicesBuffer)
  {
    let gl = this.context;

    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, aIndicesBuffer._ref);
    gl.drawElements(aDrawMode, aIndicesBuffer.numItems, gl.UNSIGNED_SHORT, 0);
  },

  /**
   * Sets the current fill color.
   *
   * @param {Array} aColor
   *                the color fill to be used as [r, g, b, a] with 0..1 range
   * @param {Number} aMultiplyAlpha
   *                 optional, scalar to multiply the alpha element with
   */
  fill: function TGLR_fill(aColor, aMultiplyAlpha)
  {
    let fill = this._fillColor;

    fill[0] = aColor[0];
    fill[1] = aColor[1];
    fill[2] = aColor[2];
    fill[3] = aColor[3] * (aMultiplyAlpha || 1);
  },

  /**
   * Sets the current stroke color.
   *
   * @param {Array} aColor
   *                the color stroke to be used as [r, g, b, a] with 0..1 range
   * @param {Number} aMultiplyAlpha
   *                 optional, scalar to multiply the alpha element with
   */
  stroke: function TGLR_stroke(aColor, aMultiplyAlpha)
  {
    let stroke = this._strokeColor;

    stroke[0] = aColor[0];
    stroke[1] = aColor[1];
    stroke[2] = aColor[2];
    stroke[3] = aColor[3] * (aMultiplyAlpha || 1);
  },

  /**
   * Sets the current stroke weight (line width).
   *
   * @param {Number} aWeight
   *                 the stroke weight
   */
  strokeWeight: function TGLR_strokeWeight(aWeight)
  {
    if (this._strokeWeightValue !== aWeight) {
      this._strokeWeightValue = aWeight;
      this.context.lineWidth(aWeight);
    }
  },

  /**
   * Sets a default perspective projection, with the near frustum rectangle
   * mapped to the canvas width and height bounds.
   */
  perspective: function TGLR_perspective()
  {
    let fov = 45;
    let w = this.width;
    let h = this.height;
    let x = w / 2;
    let y = h / 2;
    let z = y / Math.tan(TiltMath.radians(fov) / 2);
    let aspect = w / h;
    let znear = z / 10;
    let zfar = z * 10;

    mat4.perspective(fov, aspect, znear, zfar, this.projMatrix, -1);
    mat4.translate(this.projMatrix, [-x, -y, -z]);
    mat4.identity(this.mvMatrix);
  },

  /**
   * Sets a default orthographic projection (recommended for 2d rendering).
   */
  ortho: function TGLR_ortho()
  {
    mat4.ortho(0, this.width, this.height, 0, -1, 1, this.projMatrix);
    mat4.identity(this.mvMatrix);
  },

  /**
   * Sets a custom projection matrix.
   * @param {Array} matrix: the custom projection matrix to be used
   */
  projection: function TGLR_projection(aMatrix)
  {
    mat4.set(aMatrix, this.projMatrix);
    mat4.identity(this.mvMatrix);
  },

  /**
   * Resets the model view matrix to identity.
   * This is a default matrix with no rotation, no scaling, at (0, 0, 0);
   */
  origin: function TGLR_origin()
  {
    mat4.identity(this.mvMatrix);
  },

  /**
   * Transforms the model view matrix with a new matrix.
   * Useful for creating custom transformations.
   *
   * @param {Array} matrix: the matrix to be multiply the model view with
   */
  transform: function TGLR_transform(aMatrix)
  {
    mat4.multiply(this.mvMatrix, aMatrix);
  },

  /**
   * Translates the model view by the x, y and z coordinates.
   *
   * @param {Number} x
   *                 the x amount of translation
   * @param {Number} y
   *                 the y amount of translation
   * @param {Number} z
   *                 optional, the z amount of translation
   */
  translate: function TGLR_translate(x, y, z)
  {
    mat4.translate(this.mvMatrix, [x, y, z || 0]);
  },

  /**
   * Rotates the model view by a specified angle on the x, y and z axis.
   *
   * @param {Number} angle
   *                 the angle expressed in radians
   * @param {Number} x
   *                 the x axis of the rotation
   * @param {Number} y
   *                 the y axis of the rotation
   * @param {Number} z
   *                 the z axis of the rotation
   */
  rotate: function TGLR_rotate(angle, x, y, z)
  {
    mat4.rotate(this.mvMatrix, angle, [x, y, z]);
  },

  /**
   * Rotates the model view by a specified angle on the x axis.
   *
   * @param {Number} aAngle
   *                 the angle expressed in radians
   */
  rotateX: function TGLR_rotateX(aAngle)
  {
    mat4.rotateX(this.mvMatrix, aAngle);
  },

  /**
   * Rotates the model view by a specified angle on the y axis.
   *
   * @param {Number} aAngle
   *                 the angle expressed in radians
   */
  rotateY: function TGLR_rotateY(aAngle)
  {
    mat4.rotateY(this.mvMatrix, aAngle);
  },

  /**
   * Rotates the model view by a specified angle on the z axis.
   *
   * @param {Number} aAngle
   *                 the angle expressed in radians
   */
  rotateZ: function TGLR_rotateZ(aAngle)
  {
    mat4.rotateZ(this.mvMatrix, aAngle);
  },

  /**
   * Scales the model view by the x, y and z coordinates.
   *
   * @param {Number} x
   *                 the x amount of scaling
   * @param {Number} y
   *                 the y amount of scaling
   * @param {Number} z
   *                 optional, the z amount of scaling
   */
  scale: function TGLR_scale(x, y, z)
  {
    mat4.scale(this.mvMatrix, [x, y, z || 1]);
  },

  /**
   * Performs a custom interpolation between two matrices.
   * The result is saved in the first operand.
   *
   * @param {Array} aMat
   *                the first matrix
   * @param {Array} aMat2
   *                the second matrix
   * @param {Number} aLerp
   *                 interpolation amount between the two inputs
   * @param {Number} aDamping
   *                 optional, scalar adjusting the interpolation amortization
   * @param {Number} aBalance
   *                 optional, scalar adjusting the interpolation shift ammount
   */
  lerp: function TGLR_lerp(aMat, aMat2, aLerp, aDamping, aBalance)
  {
    if (aLerp < 0 || aLerp > 1) {
      return;
    }

    // calculate the interpolation factor based on the damping and step
    let f = Math.pow(1 - Math.pow(aLerp, aDamping || 1), 1 / aBalance || 1);

    // interpolate each element from the two matrices
    for (let i = 0, len = this.projMatrix.length; i < len; i++) {
      aMat[i] = aMat[i] + f * (aMat2[i] - aMat[i]);
    }
  },

  /**
   * Resets the drawing style to default.
   */
  defaults: function TGLR_defaults()
  {
    this.depthTest(true);
    this.stencilTest(false);
    this.cullFace(false);
    this.frontFace("ccw");
    this.blendMode("alpha");
    this.fill([1, 1, 1, 1]);
    this.stroke([0, 0, 0, 1]);
    this.strokeWeight(1);
    this.perspective();
    this.origin();
  },

  /**
   * Draws a quad composed of four vertices.
   * Vertices must be in clockwise order, or else drawing will be distorted.
   * Do not abuse this function, it is quite slow.
   *
   * @param {Array} aV0
   *                the [x, y, z] position of the first triangle point
   * @param {Array} aV1
   *                the [x, y, z] position of the second triangle point
   * @param {Array} aV2
   *                the [x, y, z] position of the third triangle point
   * @param {Array} aV3
   *                the [x, y, z] position of the fourth triangle point
   */
  quad: function TGLR_quad(aV0, aV1, aV2, aV3)
  {
    let gl = this.context;
    let fill = this._fillColor;
    let stroke = this._strokeColor;
    let vert = new TiltGL.VertexBuffer(gl, [aV0[0], aV0[1], aV0[2] || 0,
                                            aV1[0], aV1[1], aV1[2] || 0,
                                            aV2[0], aV2[1], aV2[2] || 0,
                                            aV3[0], aV3[1], aV3[2] || 0], 3);

    // use the necessary shader and draw the vertices
    this.useColorShader(vert, fill);
    this.drawVertices(gl.TRIANGLE_FAN, vert.numItems);

    this.useColorShader(vert, stroke);
    this.drawVertices(gl.LINE_LOOP, vert.numItems);

    TiltUtils.destroyObject(vert);
  },

  /**
   * Function called when this object is destroyed.
   */
  finalize: function TGLR_finalize()
  {
    if (this.context) {
      TiltUtils.destroyObject(this._colorShader);
    }
  }
};

/**
 * Creates a vertex buffer containing an array of elements.
 *
 * @param {Object} aContext
 *                 a WebGL context
 * @param {Array} aElementsArray
 *                an array of numbers (floats)
 * @param {Number} aItemSize
 *                 how many items create a block
 * @param {Number} aNumItems
 *                 optional, how many items to use from the array
 */
TiltGL.VertexBuffer = function TGL_VertexBuffer(
  aContext, aElementsArray, aItemSize, aNumItems)
{
  /**
   * The parent WebGL context.
   */
  this._context = aContext;

  /**
   * The array buffer.
   */
  this._ref = null;

  /**
   * Array of number components contained in the buffer.
   */
  this.components = null;

  /**
   * Variables defining the internal structure of the buffer.
   */
  this.itemSize = 0;
  this.numItems = 0;

  // if the array is specified in the constructor, initialize directly
  if (aElementsArray) {
    this.initBuffer(aElementsArray, aItemSize, aNumItems);
  }
};

TiltGL.VertexBuffer.prototype = {

  /**
   * Initializes buffer data to be used for drawing, using an array of floats.
   * The "aNumItems" param can be specified to use only a portion of the array.
   *
   * @param {Array} aElementsArray
   *                an array of floats
   * @param {Number} aItemSize
   *                 how many items create a block
   * @param {Number} aNumItems
   *                 optional, how many items to use from the array
   */
  initBuffer: function TGLVB_initBuffer(aElementsArray, aItemSize, aNumItems)
  {
    let gl = this._context;

    // the aNumItems parameter is optional, we can compute it if not specified
    aNumItems = aNumItems || aElementsArray.length / aItemSize;

    // create the Float32Array using the elements array
    this.components = new Float32Array(aElementsArray);

    // create an array buffer and bind the elements as a Float32Array
    this._ref = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, this._ref);
    gl.bufferData(gl.ARRAY_BUFFER, this.components, gl.STATIC_DRAW);

    // remember some properties, useful when binding the buffer to a shader
    this.itemSize = aItemSize;
    this.numItems = aNumItems;
  },

  /**
   * Function called when this object is destroyed.
   */
  finalize: function TGLVB_finalize()
  {
    if (this._context) {
      this._context.deleteBuffer(this._ref);
    }
  }
};

/**
 * Creates an index buffer containing an array of indices.
 *
 * @param {Object} aContext
 *                 a WebGL context
 * @param {Array} aElementsArray
 *                an array of unsigned integers
 * @param {Number} aNumItems
 *                 optional, how many items to use from the array
 */
TiltGL.IndexBuffer = function TGL_IndexBuffer(
  aContext, aElementsArray, aNumItems)
{
  /**
   * The parent WebGL context.
   */
  this._context = aContext;

  /**
   * The element array buffer.
   */
  this._ref = null;

  /**
   * Array of number components contained in the buffer.
   */
  this.components = null;

  /**
   * Variables defining the internal structure of the buffer.
   */
  this.itemSize = 0;
  this.numItems = 0;

  // if the array is specified in the constructor, initialize directly
  if (aElementsArray) {
    this.initBuffer(aElementsArray, aNumItems);
  }
};

TiltGL.IndexBuffer.prototype = {

  /**
   * Initializes a buffer of vertex indices, using an array of unsigned ints.
   * The item size will automatically default to 1, and the "numItems" will be
   * equal to the number of items in the array if not specified.
   *
   * @param {Array} aElementsArray
   *                an array of numbers (unsigned integers)
   * @param {Number} aNumItems
   *                 optional, how many items to use from the array
   */
  initBuffer: function TGLIB_initBuffer(aElementsArray, aNumItems)
  {
    let gl = this._context;

    // the aNumItems parameter is optional, we can compute it if not specified
    aNumItems = aNumItems || aElementsArray.length;

    // create the Uint16Array using the elements array
    this.components = new Uint16Array(aElementsArray);

    // create an array buffer and bind the elements as a Uint16Array
    this._ref = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this._ref);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, this.components, gl.STATIC_DRAW);

    // remember some properties, useful when binding the buffer to a shader
    this.itemSize = 1;
    this.numItems = aNumItems;
  },

  /**
   * Function called when this object is destroyed.
   */
  finalize: function TGLIB_finalize()
  {
    if (this._context) {
      this._context.deleteBuffer(this._ref);
    }
  }
};

/**
 * A program is composed of a vertex and a fragment shader.
 *
 * @param {Object} aProperties
 *                 optional, an object containing the following properties:
 *        {String} vs: the vertex shader source code
 *        {String} fs: the fragment shader source code
 *         {Array} attributes: an array of attributes as strings
 *         {Array} uniforms: an array of uniforms as strings
 */
TiltGL.Program = function(aContext, aProperties)
{
  // make sure the properties parameter is a valid object
  aProperties = aProperties || {};

  /**
   * The parent WebGL context.
   */
  this._context = aContext;

  /**
   * A reference to the actual GLSL program.
   */
  this._ref = null;

  /**
   * Each program has an unique id assigned.
   */
  this._id = -1;

  /**
   * Two arrays: an attributes array, containing all the cached attributes
   * and a uniforms array, containing all the cached uniforms.
   */
  this._attributes = null;
  this._uniforms = null;

  // if the sources are specified in the constructor, initialize directly
  if (aProperties.vs && aProperties.fs) {
    this.initProgram(aProperties);
  }
};

TiltGL.Program.prototype = {

  /**
   * Initializes a shader program, using specified source code as strings.
   *
   * @param {Object} aProperties
   *                 an object containing the following properties:
   *        {String} vs: the vertex shader source code
   *        {String} fs: the fragment shader source code
   *         {Array} attributes: an array of attributes as strings
   *         {Array} uniforms: an array of uniforms as strings
   */
  initProgram: function TGLP_initProgram(aProperties)
  {
    this._ref = TiltGL.ProgramUtils.create(this._context, aProperties);

    // cache for faster access
    this._id = this._ref.id;
    this._attributes = this._ref.attributes;
    this._uniforms = this._ref.uniforms;

    // cleanup
    delete this._ref.id;
    delete this._ref.attributes;
    delete this._ref.uniforms;
  },

  /**
   * Uses the shader program as current one for the WebGL context; it also
   * enables vertex attributes necessary to enable when using this program.
   * This method also does some useful caching, as the function "useProgram"
   * could take quite a lot of time.
   */
  use: function TGLP_use()
  {
    let id = this._id;
    let utils = TiltGL.ProgramUtils;

    // check if the program wasn't already active
    if (utils._activeProgram !== id) {
      utils._activeProgram = id;

      // use the the program if it wasn't already set
      this._context.useProgram(this._ref);
      this.cleanupVertexAttrib();

      // enable any necessary vertex attributes using the cache
      for (let key in this._attributes) {
        this._context.enableVertexAttribArray(this._attributes[key]);
        utils._enabledAttributes.push(this._attributes[key]);
      }
    }
  },

  /**
   * Disables all currently enabled vertex attribute arrays.
   */
  cleanupVertexAttrib: function TGLP_cleanupVertexAttrib()
  {
    let utils = TiltGL.ProgramUtils;

    for (let attribute of utils._enabledAttributes) {
      this._context.disableVertexAttribArray(attribute);
    }
    utils._enabledAttributes = [];
  },

  /**
   * Binds a vertex buffer as an array buffer for a specific shader attribute.
   *
   * @param {String} aAtribute
   *                 the attribute name obtained from the shader
   * @param {Float32Array} aBuffer
   *                       the buffer to be bound
   */
  bindVertexBuffer: function TGLP_bindVertexBuffer(aAtribute, aBuffer)
  {
    // get the cached attribute value from the shader
    let gl = this._context;
    let attr = this._attributes[aAtribute];
    let size = aBuffer.itemSize;

    gl.bindBuffer(gl.ARRAY_BUFFER, aBuffer._ref);
    gl.vertexAttribPointer(attr, size, gl.FLOAT, false, 0, 0);
  },

  /**
   * Binds a uniform matrix to the current shader.
   *
   * @param {String} aUniform
   *                 the uniform name to bind the variable to
   * @param {Float32Array} m
   *                       the matrix to be bound
   */
  bindUniformMatrix: function TGLP_bindUniformMatrix(aUniform, m)
  {
    this._context.uniformMatrix4fv(this._uniforms[aUniform], false, m);
  },

  /**
   * Binds a uniform vector of 4 elements to the current shader.
   *
   * @param {String} aUniform
   *                 the uniform name to bind the variable to
   * @param {Float32Array} v
   *                       the vector to be bound
   */
  bindUniformVec4: function TGLP_bindUniformVec4(aUniform, v)
  {
    this._context.uniform4fv(this._uniforms[aUniform], v);
  },

  /**
   * Binds a simple float element to the current shader.
   *
   * @param {String} aUniform
   *                 the uniform name to bind the variable to
   * @param {Number} v
   *                 the variable to be bound
   */
  bindUniformFloat: function TGLP_bindUniformFloat(aUniform, f)
  {
    this._context.uniform1f(this._uniforms[aUniform], f);
  },

  /**
   * Binds a uniform texture for a sampler to the current shader.
   *
   * @param {String} aSampler
   *                 the sampler name to bind the texture to
   * @param {TiltGL.Texture} aTexture
   *                       the texture to be bound
   */
  bindTexture: function TGLP_bindTexture(aSampler, aTexture)
  {
    let gl = this._context;

    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, aTexture._ref);
    gl.uniform1i(this._uniforms[aSampler], 0);
  },

  /**
   * Function called when this object is destroyed.
   */
  finalize: function TGLP_finalize()
  {
    if (this._context) {
      this._context.useProgram(null);
      this._context.deleteProgram(this._ref);
    }
  }
};

/**
 * Utility functions for handling GLSL shaders and programs.
 */
TiltGL.ProgramUtils = {

  /**
   * Initializes a shader program, using specified source code as strings,
   * returning the newly created shader program, by compiling and linking the
   * vertex and fragment shader.
   *
   * @param {Object} aContext
   *                 a WebGL context
   * @param {Object} aProperties
   *                 an object containing the following properties:
   *        {String} vs: the vertex shader source code
   *        {String} fs: the fragment shader source code
   *         {Array} attributes: an array of attributes as strings
   *         {Array} uniforms: an array of uniforms as strings
   */
  create: function TGLPU_create(aContext, aProperties)
  {
    // make sure the properties parameter is a valid object
    aProperties = aProperties || {};

    // compile the two shaders
    let vertShader = this.compile(aContext, aProperties.vs, "vertex");
    let fragShader = this.compile(aContext, aProperties.fs, "fragment");
    let program = this.link(aContext, vertShader, fragShader);

    aContext.deleteShader(vertShader);
    aContext.deleteShader(fragShader);

    return this.cache(aContext, aProperties, program);
  },

  /**
   * Compiles a shader source of a specific type, either vertex or fragment.
   *
   * @param {Object} aContext
   *                 a WebGL context
   * @param {String} aShaderSource
   *                 the source code for the shader
   * @param {String} aShaderType
   *                 the shader type ("vertex" or "fragment")
   *
   * @return {WebGLShader} the compiled shader
   */
  compile: function TGLPU_compile(aContext, aShaderSource, aShaderType)
  {
    let gl = aContext, shader, status;

    // make sure the shader source is valid
    if ("string" !== typeof aShaderSource || aShaderSource.length < 1) {
      TiltUtils.Output.error(
        TiltUtils.L10n.get("compileShader.source.error"));
      return null;
    }

    // also make sure the necessary shader mime type is valid
    if (aShaderType === "vertex") {
      shader = gl.createShader(gl.VERTEX_SHADER);
    } else if (aShaderType === "fragment") {
      shader = gl.createShader(gl.FRAGMENT_SHADER);
    } else {
      TiltUtils.Output.error(
        TiltUtils.L10n.format("compileShader.type.error", [aShaderSource]));
      return null;
    }

    // set the shader source and compile it
    gl.shaderSource(shader, aShaderSource);
    gl.compileShader(shader);

    // remember the shader source (useful for debugging and caching)
    shader.src = aShaderSource;

    // verify the compile status; if something went wrong, log the error
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      status = gl.getShaderInfoLog(shader);

      TiltUtils.Output.error(
        TiltUtils.L10n.format("compileShader.compile.error", [status]));
      return null;
    }

    // return the newly compiled shader from the specified source
    return shader;
  },

  /**
   * Links two compiled vertex or fragment shaders together to form a program.
   *
   * @param {Object} aContext
   *                 a WebGL context
   * @param {WebGLShader} aVertShader
   *                      the compiled vertex shader
   * @param {WebGLShader} aFragShader
   *                      the compiled fragment shader
   *
   * @return {WebGLProgram} the newly created and linked shader program
   */
  link: function TGLPU_link(aContext, aVertShader, aFragShader)
  {
    let gl = aContext, program, status;

    // create a program and attach the compiled vertex and fragment shaders
    program = gl.createProgram();

    // attach the vertex and fragment shaders to the program
    gl.attachShader(program, aVertShader);
    gl.attachShader(program, aFragShader);
    gl.linkProgram(program);

    // verify the link status; if something went wrong, log the error
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
      status = gl.getProgramInfoLog(program);

      TiltUtils.Output.error(
        TiltUtils.L10n.format("linkProgram.error", [status]));
      return null;
    }

    // generate an id for the program
    program.id = this._count++;

    return program;
  },

  /**
   * Caches shader attributes and uniforms as properties for a program object.
   *
   * @param {Object} aContext
   *                 a WebGL context
   * @param {Object} aProperties
   *                 an object containing the following properties:
   *         {Array} attributes: optional, an array of attributes as strings
   *         {Array} uniforms: optional, an array of uniforms as strings
   * @param {WebGLProgram} aProgram
   *                       the shader program used for caching
   *
   * @return {WebGLProgram} the same program
   */
  cache: function TGLPU_cache(aContext, aProperties, aProgram)
  {
    // make sure the properties parameter is a valid object
    aProperties = aProperties || {};

    // make sure the attributes and uniforms cache objects are created
    aProgram.attributes = {};
    aProgram.uniforms = {};

    Object.defineProperty(aProgram.attributes, "length",
      { value: 0, writable: true, enumerable: false, configurable: true });

    Object.defineProperty(aProgram.uniforms, "length",
      { value: 0, writable: true, enumerable: false, configurable: true });


    let attr = aProperties.attributes;
    let unif = aProperties.uniforms;

    if (attr) {
      for (let i = 0, len = attr.length; i < len; i++) {
        // try to get a shader attribute from the program
        let param = attr[i];
        let loc = aContext.getAttribLocation(aProgram, param);

        if ("number" === typeof loc && loc > -1) {
          // if we get an attribute location, store it
          // bind the new parameter only if it was not already defined
          if (aProgram.attributes[param] === undefined) {
            aProgram.attributes[param] = loc;
            aProgram.attributes.length++;
          }
        }
      }
    }

    if (unif) {
      for (let i = 0, len = unif.length; i < len; i++) {
        // try to get a shader uniform from the program
        let param = unif[i];
        let loc = aContext.getUniformLocation(aProgram, param);

        if ("object" === typeof loc && loc) {
          // if we get a uniform object, store it
          // bind the new parameter only if it was not already defined
          if (aProgram.uniforms[param] === undefined) {
            aProgram.uniforms[param] = loc;
            aProgram.uniforms.length++;
          }
        }
      }
    }

    return aProgram;
  },

  /**
   * The total number of programs created.
   */
  _count: 0,

  /**
   * Represents the current active shader, identified by an id.
   */
  _activeProgram: -1,

  /**
   * Represents the current enabled attributes.
   */
  _enabledAttributes: []
};

/**
 * This constructor creates a texture from an Image.
 *
 * @param {Object} aContext
 *                 a WebGL context
 * @param {Object} aProperties
 *                 optional, an object containing the following properties:
 *         {Image} source: the source image for the texture
 *        {String} format: the format of the texture ("RGB" or "RGBA")
 *        {String} fill: optional, color to fill the transparent bits
 *        {String} stroke: optional, color to draw an outline
 *        {Number} strokeWeight: optional, the width of the outline
 *        {String} minFilter: either "nearest" or "linear"
 *        {String} magFilter: either "nearest" or "linear"
 *        {String} wrapS: either "repeat" or "clamp"
 *        {String} wrapT: either "repeat" or "clamp"
 *       {Boolean} mipmap: true if should generate mipmap
 */
TiltGL.Texture = function(aContext, aProperties)
{
  // make sure the properties parameter is a valid object
  aProperties = aProperties || {};

  /**
   * The parent WebGL context.
   */
  this._context = aContext;

  /**
   * A reference to the WebGL texture object.
   */
  this._ref = null;

  /**
   * Each texture has an unique id assigned.
   */
  this._id = -1;

  /**
   * Variables specifying the width and height of the texture.
   * If these values are less than 0, the texture hasn't loaded yet.
   */
  this.width = -1;
  this.height = -1;

  /**
   * Specifies if the texture has loaded or not.
   */
  this.loaded = false;

  // if the image is specified in the constructor, initialize directly
  if ("object" === typeof aProperties.source) {
    this.initTexture(aProperties);
  } else {
    TiltUtils.Output.error(
      TiltUtils.L10n.get("initTexture.source.error"));
  }
};

TiltGL.Texture.prototype = {

  /**
   * Initializes a texture from a pre-existing image or canvas.
   *
   * @param {Image} aImage
   *                the source image or canvas
   * @param {Object} aProperties
   *                 an object containing the following properties:
   *         {Image} source: the source image for the texture
   *        {String} format: the format of the texture ("RGB" or "RGBA")
   *        {String} fill: optional, color to fill the transparent bits
   *        {String} stroke: optional, color to draw an outline
   *        {Number} strokeWeight: optional, the width of the outline
   *        {String} minFilter: either "nearest" or "linear"
   *        {String} magFilter: either "nearest" or "linear"
   *        {String} wrapS: either "repeat" or "clamp"
   *        {String} wrapT: either "repeat" or "clamp"
   *       {Boolean} mipmap: true if should generate mipmap
   */
  initTexture: function TGLT_initTexture(aProperties)
  {
    this._ref = TiltGL.TextureUtils.create(this._context, aProperties);

    // cache for faster access
    this._id = this._ref.id;
    this.width = this._ref.width;
    this.height = this._ref.height;
    this.loaded = true;

    // cleanup
    delete this._ref.id;
    delete this._ref.width;
    delete this._ref.height;
    delete this.onload;
  },

  /**
   * Function called when this object is destroyed.
   */
  finalize: function TGLT_finalize()
  {
    if (this._context) {
      this._context.deleteTexture(this._ref);
    }
  }
};

/**
 * Utility functions for creating and manipulating textures.
 */
TiltGL.TextureUtils = {

  /**
   * Initializes a texture from a pre-existing image or canvas.
   *
   * @param {Object} aContext
   *                 a WebGL context
   * @param {Image} aImage
   *                the source image or canvas
   * @param {Object} aProperties
   *                 an object containing some of the following properties:
   *         {Image} source: the source image for the texture
   *        {String} format: the format of the texture ("RGB" or "RGBA")
   *        {String} fill: optional, color to fill the transparent bits
   *        {String} stroke: optional, color to draw an outline
   *        {Number} strokeWeight: optional, the width of the outline
   *        {String} minFilter: either "nearest" or "linear"
   *        {String} magFilter: either "nearest" or "linear"
   *        {String} wrapS: either "repeat" or "clamp"
   *        {String} wrapT: either "repeat" or "clamp"
   *       {Boolean} mipmap: true if should generate mipmap
   *
   * @return {WebGLTexture} the created texture
   */
  create: function TGLTU_create(aContext, aProperties)
  {
    // make sure the properties argument is an object
    aProperties = aProperties || {};

    if (!aProperties.source) {
      return null;
    }

    let gl = aContext;
    let width = aProperties.source.width;
    let height = aProperties.source.height;
    let format = gl[aProperties.format || "RGB"];

    // make sure the image is power of two before binding to a texture
    let source = this.resizeImageToPowerOfTwo(aProperties);

    // first, create the texture to hold the image data
    let texture = gl.createTexture();

    // attach the image data to the newly create texture
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texImage2D(gl.TEXTURE_2D, 0, format, format, gl.UNSIGNED_BYTE, source);
    this.setTextureParams(gl, aProperties);

    // do some cleanup
    gl.bindTexture(gl.TEXTURE_2D, null);

    // remember the width and the height
    texture.width = width;
    texture.height = height;

    // generate an id for the texture
    texture.id = this._count++;

    return texture;
  },

  /**
   * Sets texture parameters for the current texture binding.
   * Optionally, you can also (re)set the current texture binding manually.
   *
   * @param {Object} aContext
   *                 a WebGL context
   * @param {Object} aProperties
   *                 an object containing the texture properties
   */
  setTextureParams: function TGLTU_setTextureParams(aContext, aProperties)
  {
    // make sure the properties argument is an object
    aProperties = aProperties || {};

    let gl = aContext;
    let minFilter = gl.TEXTURE_MIN_FILTER;
    let magFilter = gl.TEXTURE_MAG_FILTER;
    let wrapS = gl.TEXTURE_WRAP_S;
    let wrapT = gl.TEXTURE_WRAP_T;

    // bind a new texture if necessary
    if (aProperties.texture) {
      gl.bindTexture(gl.TEXTURE_2D, aProperties.texture.ref);
    }

    // set the minification filter
    if ("nearest" === aProperties.minFilter) {
      gl.texParameteri(gl.TEXTURE_2D, minFilter, gl.NEAREST);
    } else if ("linear" === aProperties.minFilter && aProperties.mipmap) {
      gl.texParameteri(gl.TEXTURE_2D, minFilter, gl.LINEAR_MIPMAP_LINEAR);
    } else {
      gl.texParameteri(gl.TEXTURE_2D, minFilter, gl.LINEAR);
    }

    // set the magnification filter
    if ("nearest" === aProperties.magFilter) {
      gl.texParameteri(gl.TEXTURE_2D, magFilter, gl.NEAREST);
    } else {
      gl.texParameteri(gl.TEXTURE_2D, magFilter, gl.LINEAR);
    }

    // set the wrapping on the x-axis for the texture
    if ("repeat" === aProperties.wrapS) {
      gl.texParameteri(gl.TEXTURE_2D, wrapS, gl.REPEAT);
    } else {
      gl.texParameteri(gl.TEXTURE_2D, wrapS, gl.CLAMP_TO_EDGE);
    }

    // set the wrapping on the y-axis for the texture
    if ("repeat" === aProperties.wrapT) {
      gl.texParameteri(gl.TEXTURE_2D, wrapT, gl.REPEAT);
    } else {
      gl.texParameteri(gl.TEXTURE_2D, wrapT, gl.CLAMP_TO_EDGE);
    }

    // generate mipmap if necessary
    if (aProperties.mipmap) {
      gl.generateMipmap(gl.TEXTURE_2D);
    }
  },

  /**
   * This shim renders a content window to a canvas element, but clamps the
   * maximum width and height of the canvas to the WebGL MAX_TEXTURE_SIZE.
   *
   * @param {Window} aContentWindow
   *                 the content window to get a texture from
   * @param {Number} aMaxImageSize
   *                 the maximum image size to be used
   *
   * @return {Image} the new content window image
   */
  createContentImage: function TGLTU_createContentImage(
    aContentWindow, aMaxImageSize)
  {
    // calculate the total width and height of the content page
    let size = TiltUtils.DOM.getContentWindowDimensions(aContentWindow);

    // use a custom canvas element and a 2d context to draw the window
    let canvas = TiltUtils.DOM.initCanvas(null);
    canvas.width = TiltMath.clamp(size.width, 0, aMaxImageSize);
    canvas.height = TiltMath.clamp(size.height, 0, aMaxImageSize);

    // use the 2d context.drawWindow() magic
    let ctx = canvas.getContext("2d");
    ctx.drawWindow(aContentWindow, 0, 0, canvas.width, canvas.height, "#fff");

    return canvas;
  },

  /**
   * Scales an image's width and height to next power of two.
   * If the image already has power of two sizes, it is immediately returned,
   * otherwise, a new image is created.
   *
   * @param {Image} aImage
   *                the image to be scaled
   * @param {Object} aProperties
   *                 an object containing the following properties:
   *         {Image} source: the source image to resize
   *       {Boolean} resize: true to resize the image if it has npot dimensions
   *        {String} fill: optional, color to fill the transparent bits
   *        {String} stroke: optional, color to draw an image outline
   *        {Number} strokeWeight: optional, the width of the outline
   *
   * @return {Image} the resized image
   */
  resizeImageToPowerOfTwo: function TGLTU_resizeImageToPowerOfTwo(aProperties)
  {
    // make sure the properties argument is an object
    aProperties = aProperties || {};

    if (!aProperties.source) {
      return null;
    }

    let isPowerOfTwoWidth = TiltMath.isPowerOfTwo(aProperties.source.width);
    let isPowerOfTwoHeight = TiltMath.isPowerOfTwo(aProperties.source.height);

    // first check if the image is not already power of two
    if (!aProperties.resize || (isPowerOfTwoWidth && isPowerOfTwoHeight)) {
      return aProperties.source;
    }

    // calculate the power of two dimensions for the npot image
    let width = TiltMath.nextPowerOfTwo(aProperties.source.width);
    let height = TiltMath.nextPowerOfTwo(aProperties.source.height);

    // create a canvas, then we will use a 2d context to scale the image
    let canvas = TiltUtils.DOM.initCanvas(null, {
      width: width,
      height: height
    });

    let ctx = canvas.getContext("2d");

    // optional fill (useful when handling transparent images)
    if (aProperties.fill) {
      ctx.fillStyle = aProperties.fill;
      ctx.fillRect(0, 0, width, height);
    }

    // draw the image with power of two dimensions
    ctx.drawImage(aProperties.source, 0, 0, width, height);

    // optional stroke (useful when creating textures for edges)
    if (aProperties.stroke) {
      ctx.strokeStyle = aProperties.stroke;
      ctx.lineWidth = aProperties.strokeWeight;
      ctx.strokeRect(0, 0, width, height);
    }

    return canvas;
  },

  /**
   * The total number of textures created.
   */
  _count: 0
};

/**
 * A color shader. The only useful thing it does is set the gl_FragColor.
 *
 * @param {Attribute} vertexPosition: the vertex position
 * @param {Uniform} mvMatrix: the model view matrix
 * @param {Uniform} projMatrix: the projection matrix
 * @param {Uniform} color: the color to set the gl_FragColor to
 */
TiltGL.ColorShader = {

  /**
   * Vertex shader.
   */
  vs: [
    "attribute vec3 vertexPosition;",

    "uniform mat4 mvMatrix;",
    "uniform mat4 projMatrix;",

    "void main() {",
    "    gl_Position = projMatrix * mvMatrix * vec4(vertexPosition, 1.0);",
    "}"
  ].join("\n"),

  /**
   * Fragment shader.
   */
  fs: [
    "#ifdef GL_ES",
    "precision lowp float;",
    "#endif",

    "uniform vec4 fill;",

    "void main() {",
    "    gl_FragColor = fill;",
    "}"
  ].join("\n")
};

TiltGL.isWebGLForceEnabled = function TGL_isWebGLForceEnabled()
{
  return Services.prefs.getBoolPref("webgl.force-enabled");
};

/**
 * Tests if the WebGL OpenGL or Angle renderer is available using the
 * GfxInfo service.
 *
 * @return {Boolean} true if WebGL is available
 */
TiltGL.isWebGLSupported = function TGL_isWebGLSupported()
{
  let supported = false;

  try {
    let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
    let angle = gfxInfo.FEATURE_WEBGL_ANGLE;
    let opengl = gfxInfo.FEATURE_WEBGL_OPENGL;

    // if either the Angle or OpenGL renderers are available, WebGL should work
    supported = gfxInfo.getFeatureStatus(angle) === gfxInfo.FEATURE_STATUS_OK ||
                gfxInfo.getFeatureStatus(opengl) === gfxInfo.FEATURE_STATUS_OK;
  } catch(e) {
    if (e && e.message) { TiltUtils.Output.error(e.message); }
    return false;
  }
  return supported;
};

/**
 * Helper function to create a 3D context.
 *
 * @param {HTMLCanvasElement} aCanvas
 *                            the canvas to get the WebGL context from
 * @param {Object} aFlags
 *                 optional, flags used for initialization
 *
 * @return {Object} the WebGL context, or null if anything failed
 */
TiltGL.create3DContext = function TGL_create3DContext(aCanvas, aFlags)
{
  TiltGL.clearCache();

  // try to get a valid context from an existing canvas
  let context = null;

  try {
    context = aCanvas.getContext(WEBGL_CONTEXT_NAME, aFlags);
  } catch(e) {
    if (e && e.message) { TiltUtils.Output.error(e.message); }
    return null;
  }
  return context;
};

/**
 * Clears the cache and sets all the variables to default.
 */
TiltGL.clearCache = function TGL_clearCache()
{
  TiltGL.ProgramUtils._activeProgram = -1;
  TiltGL.ProgramUtils._enabledAttributes = [];
};
