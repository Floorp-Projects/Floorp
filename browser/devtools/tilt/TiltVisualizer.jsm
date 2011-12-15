/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Tilt: A WebGL-based 3D visualization of a webpage.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Victor Porof <vporof@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 ***** END LICENSE BLOCK *****/

/*global Components, ChromeWorker */
/*global TiltGL, TiltMath, vec3, mat4, quat4, TiltUtils, TiltVisualizerStyle */
"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;

const ELEMENT_MIN_SIZE = 4;
const INVISIBLE_ELEMENTS = {
  "head": true,
  "base": true,
  "basefont": true,
  "isindex": true,
  "link": true,
  "meta": true,
  "option": true,
  "script": true,
  "style": true,
  "title": true
};

const STACK_THICKNESS = 15;
const WIREFRAME_COLOR = [0, 0, 0, 0.25];
const INITIAL_TRANSITION_DURATION = 100;
const INITIAL_Z_TRANSLATION = 400;

const MOUSE_CLICK_THRESHOLD = 10;
const ARCBALL_SENSITIVITY = 0.3;
const ARCBALL_ROTATION_STEP = 0.15;
const ARCBALL_TRANSLATION_STEP = 35;
const ARCBALL_ZOOM_STEP = 0.1;
const ARCBALL_ZOOM_MIN = -3000;
const ARCBALL_ZOOM_MAX = 500;

const TILT_CRAFTER = "resource:///modules/devtools/TiltWorkerCrafter.js";
const TILT_PICKER = "resource:///modules/devtools/TiltWorkerPicker.js";

Cu.import("resource:///modules/devtools/TiltGL.jsm");
Cu.import("resource:///modules/devtools/TiltMath.jsm");
Cu.import("resource:///modules/devtools/TiltUtils.jsm");
Cu.import("resource:///modules/devtools/TiltVisualizerStyle.jsm");

let EXPORTED_SYMBOLS = ["TiltVisualizer"];

/**
 * Initializes the visualization presenter and controller.
 *
 * @param {Object} aProperties
 *                 an object containing the following properties:
 *       {Element} parentNode: the parent node to hold the visualization
 *        {Window} contentWindow: the content window holding the visualized doc
 *      {Function} requestAnimationFrame: responsible with scheduling loops
 *   {InspectorUI} inspectorUI: necessary instance of the InspectorUI
 *      {Function} onError: optional, function called if initialization failed
 *      {Function} onLoad: optional, function called if initialization worked
 */
function TiltVisualizer(aProperties)
{
  // make sure the properties parameter is a valid object
  aProperties = aProperties || {};

  /**
   * The canvas element used for rendering the visualization.
   */
  this.canvas = TiltUtils.DOM.initCanvas(aProperties.parentNode, {
    focusable: true,
    append: true
  });

  /**
   * Visualization logic and drawing loop.
   */
  this.presenter = new TiltVisualizer.Presenter(this.canvas,
    aProperties.contentWindow,
    aProperties.requestAnimationFrame,
    aProperties.inspectorUI,
    aProperties.onError || null,
    aProperties.onLoad || null);

  /**
   * Visualization mouse and keyboard controller.
   */
  this.controller = new TiltVisualizer.Controller(this.canvas, this.presenter);
}

TiltVisualizer.prototype = {

  /**
   * Checks if this object was initialized properly.
   *
   * @return {Boolean} true if the object was initialized properly
   */
  isInitialized: function TV_isInitialized()
  {
    return this.presenter && this.presenter.isInitialized() &&
           this.controller && this.controller.isInitialized();
  },

  /**
   * Removes the overlay canvas used for rendering the visualization.
   */
  removeOverlay: function TV_removeOverlay()
  {
    if (this.canvas && this.canvas.parentNode) {
      this.canvas.parentNode.removeChild(this.canvas);
    }
  },

  /**
   * Explicitly cleans up this visualizer and sets everything to null.
   */
  cleanup: function TV_cleanup()
  {
    if (this.controller) {
      TiltUtils.destroyObject(this.controller);
    }
    if (this.presenter) {
      TiltUtils.destroyObject(this.presenter);
    }
    TiltUtils.destroyObject(this);
    TiltUtils.clearCache();
    TiltUtils.gc();
  }
};

/**
 * This object manages the visualization logic and drawing loop.
 *
 * @param {HTMLCanvasElement} aCanvas
 *                            the canvas element used for rendering
 * @param {Object} aContentWindow
 *                 the content window holding the document to be visualized
 * @param {Function} aRequestAnimationFrame
 *                   function responsible with scheduling loop frames
 * @param {InspectorUI} aInspectorUI
 *                      necessary instance of the InspectorUI
 * @param {Function} onError
 *                   function called if initialization failed
 * @param {Function} onLoad
 *                   function called if initialization worked
 */
TiltVisualizer.Presenter = function TV_Presenter(
  aCanvas, aContentWindow, aRequestAnimationFrame, aInspectorUI,
  onError, onLoad)
{
  this.canvas = aCanvas;
  this.contentWindow = aContentWindow;
  this.inspectorUI = aInspectorUI;
  this.tiltUI = aInspectorUI.chromeWin.Tilt;

  /**
   * Create the renderer, containing useful functions for easy drawing.
   */
  this.renderer = new TiltGL.Renderer(aCanvas, onError, onLoad);

  /**
   * A custom shader used for drawing the visualization mesh.
   */
  this.visualizationProgram = null;

  /**
   * The combined mesh representing the document visualization.
   */
  this.texture = null;
  this.meshStacks = null;
  this.meshWireframe = null;
  this.traverseData = null;

  /**
   * A highlight quad drawn over a stacked dom node.
   */
  this.highlight = {
    disabled: true,
    v0: vec3.create(),
    v1: vec3.create(),
    v2: vec3.create(),
    v3: vec3.create()
  };

  /**
   * Scene transformations, exposing offset, translation and rotation.
   * Modified by events in the controller through delegate functions.
   */
  this.transforms = {
    offset: vec3.create(),      // mesh offset, aligned to the viewport center
    translation: vec3.create(), // scene translation, on the [x, y, z] axis
    rotation: quat4.create()    // scene rotation, expressed as a quaternion
  };

  /**
   * Variables holding information about the initial and current node selected.
   */
  this._initialSelection = false; // true if an initial selection was made
  this._currentSelection = -1; // the selected node index

  /**
   * Variable specifying if the scene should be redrawn.
   * This should happen usually when the visualization is translated/rotated.
   */
  this.redraw = true;

  /**
   * A frame counter, incremented each time the scene is redrawn.
   */
  this.frames = 0;

  /**
   * The initialization logic.
   */
  let setup = function TVP_setup()
  {
    let renderer = this.renderer;

    // if the renderer was destroyed, don't continue setup
    if (!renderer || !renderer.context) {
      return;
    }

    // create the visualization shaders and program to draw the stacks mesh
    this.visualizationProgram = new renderer.Program({
      vs: TiltVisualizer.MeshShader.vs,
      fs: TiltVisualizer.MeshShader.fs,
      attributes: ["vertexPosition", "vertexTexCoord", "vertexColor"],
      uniforms: ["mvMatrix", "projMatrix", "sampler"]
    });

    this.setupTexture();
    this.setupMeshData();
    this.setupEventListeners();
    this.canvas.focus();
  }.bind(this);

  /**
   * The animation logic.
   */
  let loop = function TVP_loop()
  {
    let renderer = this.renderer;

    // if the renderer was destroyed, don't continue rendering
    if (!renderer || !renderer.context) {
      return;
    }

    // prepare for the next frame of the animation loop
    aRequestAnimationFrame(loop);

    // only redraw if we really have to
    if (this.redraw) {
      this.redraw = false;
      this.drawVisualization();
    }

    // call the attached ondraw event handler if specified (by the controller)
    if ("function" === typeof this.ondraw) {
      this.ondraw();
    }
  }.bind(this);

  setup();
  loop();
};

TiltVisualizer.Presenter.prototype = {

  /**
   * Draws the visualization mesh and highlight quad.
   */
  drawVisualization: function TVP_drawVisualization()
  {
    let renderer = this.renderer;
    let transforms = this.transforms;

    // if the mesh wasn't created yet, don't continue rendering
    if (!this.meshStacks || !this.meshWireframe) {
      return;
    }

    // clear the context to an opaque black background
    renderer.clear();
    renderer.perspective();

    // apply a transition transformation using an ortho and perspective matrix
    let f = this.frames / INITIAL_TRANSITION_DURATION;
    let w = renderer.width;
    let h = renderer.height;
    renderer.lerp(renderer.projMatrix, mat4.ortho(0, w, h, 0, -1, 1000), f, 8);

    // apply the preliminary transformations to the model view
    renderer.translate(w * 0.5, h * 0.5, -INITIAL_Z_TRANSLATION);

    // calculate the camera matrix using the rotation and translation
    renderer.translate(transforms.translation[0], 0,
                       transforms.translation[2]);

    renderer.transform(quat4.toMat4(transforms.rotation));

    // offset the visualization mesh to center
    renderer.translate(transforms.offset[0],
                       transforms.offset[1] + transforms.translation[1], 0);

    // draw the visualization mesh
    renderer.strokeWeight(2);
    renderer.depthTest(true);
    this.drawMeshStacks();
    this.drawMeshWireframe();
    this.drawHighlight();

    // make sure the initial transition is drawn until finished
    if (this.frames < INITIAL_TRANSITION_DURATION) {
      this.redraw = true;
    }
    this.frames++;
  },

  /**
   * Draws the meshStacks object.
   */
  drawMeshStacks: function TVP_drawMeshStacks()
  {
    let renderer = this.renderer;
    let mesh = this.meshStacks;

    let visualizationProgram = this.visualizationProgram;
    let texture = this.texture;
    let mvMatrix = renderer.mvMatrix;
    let projMatrix = renderer.projMatrix;

    // use the necessary shader
    visualizationProgram.use();

    // bind the attributes and uniforms as necessary
    visualizationProgram.bindVertexBuffer("vertexPosition", mesh.vertices);
    visualizationProgram.bindVertexBuffer("vertexTexCoord", mesh.texCoord);
    visualizationProgram.bindVertexBuffer("vertexColor", mesh.color);

    visualizationProgram.bindUniformMatrix("mvMatrix", mvMatrix);
    visualizationProgram.bindUniformMatrix("projMatrix", projMatrix);
    visualizationProgram.bindTexture("sampler", texture);

    // draw the vertices as TRIANGLES indexed elements
    renderer.drawIndexedVertices(renderer.context.TRIANGLES, mesh.indices);

    // save the current model view and projection matrices
    mesh.mvMatrix = mat4.create(mvMatrix);
    mesh.projMatrix = mat4.create(projMatrix);
  },

  /**
   * Draws the meshWireframe object.
   */
  drawMeshWireframe: function TVP_drawMeshWireframe()
  {
    let renderer = this.renderer;
    let mesh = this.meshWireframe;

    // use the necessary shader
    renderer.useColorShader(mesh.vertices, WIREFRAME_COLOR);

    // draw the vertices as LINES indexed elements
    renderer.drawIndexedVertices(renderer.context.LINES, mesh.indices);
  },

  /**
   * Draws a highlighted quad around a currently selected node.
   */
  drawHighlight: function TVP_drawHighlight()
  {
    // check if there's anything to highlight (i.e any node is selected)
    if (!this.highlight.disabled) {

      // set the corresponding state to draw the highlight quad
      let renderer = this.renderer;
      let highlight = this.highlight;

      renderer.depthTest(false);
      renderer.fill(highlight.fill, 0.5);
      renderer.stroke(highlight.stroke);
      renderer.strokeWeight(highlight.strokeWeight);
      renderer.quad(highlight.v0, highlight.v1, highlight.v2, highlight.v3);
    }
  },

  /**
   * Creates or refreshes the texture applied to the visualization mesh.
   */
  setupTexture: function TVP_setupTexture()
  {
    let renderer = this.renderer;

    // destroy any previously created texture
    TiltUtils.destroyObject(this.texture);

    // if the renderer was destroyed, don't continue setup
    if (!renderer || !renderer.context) {
      return;
    }

    // get the maximum texture size
    this.maxTextureSize =
      renderer.context.getParameter(renderer.context.MAX_TEXTURE_SIZE);

    // use a simple shim to get the image representation of the document
    // this will be removed once the MOZ_window_region_texture bug #653656
    // is finished; currently just converting the document image to a texture
    // applied to the mesh
    this.texture = new renderer.Texture({
      source: TiltGL.TextureUtils.createContentImage(this.contentWindow,
                                                     this.maxTextureSize),
      format: "RGB"
    });
  },

  /**
   * Create the combined mesh representing the document visualization by
   * traversing the document & adding a stack for each node that is drawable.
   *
   * @param {Object} aData
   *                 object containing the necessary mesh verts, texcoord etc.
   */
  setupMesh: function TVP_setupMesh(aData)
  {
    let renderer = this.renderer;

    // destroy any previously created mesh
    TiltUtils.destroyObject(this.meshStacks);
    TiltUtils.destroyObject(this.meshWireframe);

    // if the renderer was destroyed, don't continue setup
    if (!renderer || !renderer.context) {
      return;
    }

    // create the visualization mesh using the vertices, texture coordinates
    // and indices computed when traversing the document object model
    this.meshStacks = {
      vertices: new renderer.VertexBuffer(aData.vertices, 3),
      texCoord: new renderer.VertexBuffer(aData.texCoord, 2),
      color: new renderer.VertexBuffer(aData.color, 3),
      indices: new renderer.IndexBuffer(aData.stacksIndices)
    };

    // additionally, create a wireframe representation to make the
    // visualization a bit more pretty
    this.meshWireframe = {
      vertices: this.meshStacks.vertices,
      indices: new renderer.IndexBuffer(aData.wireframeIndices)
    };

    // if there's no initial selection made, highlight the required node
    if (!this._initialSelection) {
      this._initialSelection = true;
      this.highlightNode(this.inspectorUI.selection);
    }

    let width = renderer.width;
    let height = renderer.height;

    // set the necessary mesh offsets
    this.transforms.offset[0] = -Math.min(aData.meshWidth, width) * 0.5;
    this.transforms.offset[1] = -Math.min(aData.meshHeight, height) * 0.5;

    // make sure the canvas is opaque now that the initialization is finished
    this.canvas.style.background = TiltVisualizerStyle.canvas.background;

    this.drawVisualization();
    this.redraw = true;
  },

  /**
   * Computes the mesh vertices, texture coordinates etc.
   */
  setupMeshData: function TVP_setupMeshData()
  {
    let renderer = this.renderer;

    // if the renderer was destroyed, don't continue setup
    if (!renderer || !renderer.context) {
      return;
    }

    // traverse the document and get the depths, coordinates and local names
    this.traverseData = TiltUtils.DOM.traverse(this.contentWindow, {
      invisibleElements: INVISIBLE_ELEMENTS,
      minSize: ELEMENT_MIN_SIZE,
      maxX: this.texture.width,
      maxY: this.texture.height
    });

    let worker = new ChromeWorker(TILT_CRAFTER);

    worker.addEventListener("message", function TVP_onMessage(event) {
      this.setupMesh(event.data);
    }.bind(this), false);

    // calculate necessary information regarding vertices, texture coordinates
    // etc. in a separate thread, as this process may take a while
    worker.postMessage({
      thickness: STACK_THICKNESS,
      style: TiltVisualizerStyle.nodes,
      texWidth: this.texture.width,
      texHeight: this.texture.height,
      nodesInfo: this.traverseData.info
    });
  },

  /**
   * Sets up event listeners necessary for the presenter.
   */
  setupEventListeners: function TVP_setupEventListeners()
  {
    // bind the owner object to the necessary functions
    TiltUtils.bindObjectFunc(this, "^on");

    this.contentWindow.addEventListener("resize", this.onResize, false);
  },

  /**
   * Called when the content window of the current browser is resized.
   */
  onResize: function TVP_onResize(e)
  {
    let width = e.target.innerWidth;
    let height = e.target.innerHeight;

    // handle aspect ratio changes to update the projection matrix
    this.renderer.width = width;
    this.renderer.height = height;

    this.redraw = true;
  },

  /**
   * Highlights a specific node.
   *
   * @param {Element} aNode
   *                  the html node to be highlighted
   */
  highlightNode: function TVP_highlightNode(aNode)
  {
    if (!aNode) {
      return;
    }

    this.highlightNodeFor(this.traverseData.nodes.indexOf(aNode));
  },

  /**
   * Picks a stacked dom node at the x and y screen coordinates and highlights
   * the selected node in the mesh.
   *
   * @param {Number} x
   *                 the current horizontal coordinate of the mouse
   * @param {Number} y
   *                 the current vertical coordinate of the mouse
   */
  highlightNodeAt: function TVP_highlightNodeAt(x, y)
  {
    // try to pick a mesh node using the current x, y coordinates
    this.pickNode(x, y, {

      /**
       * Mesh picking failed (nothing was found for the picked point).
       */
      onfail: function TVP_onHighlightFail()
      {
        this.highlightNodeFor(-1);
      }.bind(this),

      /**
       * Mesh picking succeeded.
       *
       * @param {Object} aIntersection
       *                 object containing the intersection details
       */
      onpick: function TVP_onHighlightPick(aIntersection)
      {
        this.highlightNodeFor(aIntersection.index);
      }.bind(this)
    });
  },

  /**
   * Sets the corresponding highlight coordinates and color based on the
   * information supplied.
   *
   * @param {Number} aNodeIndex
   *                 the index of the node in the this.traverseData array
   */
  highlightNodeFor: function TVP_highlightNodeFor(aNodeIndex)
  {
    this.redraw = true;

    // if the node was already selected, don't do anything
    if (this._currentSelection === aNodeIndex) {
      return;
    }
    // if an invalid or nonexisted node is specified, disable the highlight
    if (aNodeIndex < 0) {
      this._currentSelection = -1;
      this.highlight.disabled = true;
      return;
    }

    let highlight = this.highlight;
    let info = this.traverseData.info[aNodeIndex];
    let node = this.traverseData.nodes[aNodeIndex];
    let style = TiltVisualizerStyle.nodes;

    highlight.disabled = false;
    highlight.fill = style[info.name] || style.highlight.defaultFill;
    highlight.stroke = style.highlight.defaultStroke;
    highlight.strokeWeight = style.highlight.defaultStrokeWeight;

    let x = info.coord.left;
    let y = info.coord.top;
    let w = info.coord.width;
    let h = info.coord.height;
    let z = info.depth;

    vec3.set([x,     y,     z * STACK_THICKNESS], highlight.v0);
    vec3.set([x + w, y,     z * STACK_THICKNESS], highlight.v1);
    vec3.set([x + w, y + h, z * STACK_THICKNESS], highlight.v2);
    vec3.set([x,     y + h, z * STACK_THICKNESS], highlight.v3);

    this._currentSelection = aNodeIndex;
    this.inspectorUI.inspectNode(node);
  },

  /**
   * Picks a stacked dom node at the x and y screen coordinates and issues
   * a callback function with the found intersection.
   *
   * @param {Number} x
   *                 the current horizontal coordinate of the mouse
   * @param {Number} y
   *                 the current vertical coordinate of the mouse
   * @param {Object} aProperties
   *                 an object containing the following properties:
   *      {Function} onpick: function to be called at intersection
   *      {Function} onfail: function to be called if no intersections
   */
  pickNode: function TVP_pickNode(x, y, aProperties)
  {
    // make sure the properties parameter is a valid object
    aProperties = aProperties || {};

    // if the mesh wasn't created yet, don't continue picking
    if (!this.meshStacks || !this.meshWireframe) {
      return;
    }

    let worker = new ChromeWorker(TILT_PICKER);

    worker.addEventListener("message", function TVP_onMessage(event) {
      if (event.data) {
        if ("function" === typeof aProperties.onpick) {
          aProperties.onpick(event.data);
        }
      } else {
        if ("function" === typeof aProperties.onfail) {
          aProperties.onfail();
        }
      }
    }, false);

    let width = this.renderer.width;
    let height = this.renderer.height;
    let mesh = this.meshStacks;

    // create a ray following the mouse direction from the near clipping plane
    // to the far clipping plane, to check for intersections with the mesh,
    // and do all the heavy lifting in a separate thread
    worker.postMessage({
      thickness: STACK_THICKNESS,
      vertices: mesh.vertices.components,

      // create the ray destined for 3D picking
      ray: vec3.createRay([x, y, 0], [x, y, 1], [0, 0, width, height],
        mesh.mvMatrix,
        mesh.projMatrix)
    });
  },

  /**
   * Delegate translation method, used by the controller.
   *
   * @param {Array} aTranslation
   *                the new translation on the [x, y, z] axis
   */
  setTranslation: function TVP_setTranslation(aTranslation)
  {
    let x = aTranslation[0];
    let y = aTranslation[1];
    let z = aTranslation[2];
    let transforms = this.transforms;

    // only update the translation if it's not already set
    if (transforms.translation[0] !== x ||
        transforms.translation[1] !== y ||
        transforms.translation[2] !== z) {

      vec3.set(aTranslation, transforms.translation);
      this.redraw = true;
    }
  },

  /**
   * Delegate rotation method, used by the controller.
   *
   * @param {Array} aQuaternion
   *                the rotation quaternion, as [x, y, z, w]
   */
  setRotation: function TVP_setRotation(aQuaternion)
  {
    let x = aQuaternion[0];
    let y = aQuaternion[1];
    let z = aQuaternion[2];
    let w = aQuaternion[3];
    let transforms = this.transforms;

    // only update the rotation if it's not already set
    if (transforms.rotation[0] !== x ||
        transforms.rotation[1] !== y ||
        transforms.rotation[2] !== z ||
        transforms.rotation[3] !== w) {

      quat4.set(aQuaternion, transforms.rotation);
      this.redraw = true;
    }
  },

  /**
   * Checks if this object was initialized properly.
   *
   * @return {Boolean} true if the object was initialized properly
   */
  isInitialized: function TVP_isInitialized()
  {
    return this.renderer && this.renderer.context;
  },

  /**
   * Function called when this object is destroyed.
   */
  finalize: function TVP_finalize()
  {
    TiltUtils.destroyObject(this.visualizationProgram);
    TiltUtils.destroyObject(this.texture);

    if (this.meshStacks) {
      TiltUtils.destroyObject(this.meshStacks.vertices);
      TiltUtils.destroyObject(this.meshStacks.texCoord);
      TiltUtils.destroyObject(this.meshStacks.color);
      TiltUtils.destroyObject(this.meshStacks.indices);
    }

    if (this.meshWireframe) {
      TiltUtils.destroyObject(this.meshWireframe.indices);
    }

    TiltUtils.destroyObject(this.highlight);
    TiltUtils.destroyObject(this.transforms);
    TiltUtils.destroyObject(this.renderer);

    this.contentWindow.removeEventListener("resize", this.onResize, false);
  }
};

/**
 * A mouse and keyboard controller implementation.
 *
 * @param {HTMLCanvasElement} aCanvas
 *                            the visualization canvas element
 * @param {TiltVisualizer.Presenter} aPresenter
 *                                   the presenter instance to control
 */
TiltVisualizer.Controller = function TV_Controller(aCanvas, aPresenter)
{
  this.canvas = aCanvas;
  this.presenter = aPresenter;

  /**
   * The initial controller dimensions and offset, in pixels.
   */
  this.left = aPresenter.contentWindow.pageXOffset || 0;
  this.top = aPresenter.contentWindow.pageYOffset || 0;
  this.width = aCanvas.width;
  this.height = aCanvas.height;

  /**
   * Arcball used to control the visualization using the mouse.
   */
  this.arcball = new TiltVisualizer.Arcball(this.width, this.height, 0,
    [this.width + this.left < aPresenter.maxTextureSize ? -this.left : 0,
     this.height + this.top < aPresenter.maxTextureSize ? -this.top : 0]);

  /**
   * Object containing the rotation quaternion and the translation amount.
   */
  this.coordinates = null;

  // bind the owner object to the necessary functions
  TiltUtils.bindObjectFunc(this, "update");
  TiltUtils.bindObjectFunc(this, "^on");

  // bind commonly used mouse and keyboard events with the controller
  aCanvas.addEventListener("mousedown", this.onMouseDown, false);
  aCanvas.addEventListener("mouseup", this.onMouseUp, false);
  aCanvas.addEventListener("click", this.onMouseClick, false);
  aCanvas.addEventListener("mousemove", this.onMouseMove, false);
  aCanvas.addEventListener("mouseover", this.onMouseOver, false);
  aCanvas.addEventListener("mouseout", this.onMouseOut, false);
  aCanvas.addEventListener("MozMousePixelScroll", this.onMozScroll, false);
  aCanvas.addEventListener("keydown", this.onKeyDown, false);
  aCanvas.addEventListener("keyup", this.onKeyUp, false);

  // handle resize events to change the arcball dimensions
  aPresenter.contentWindow.addEventListener("resize", this.onResize, false);

  // attach this controller's update function to the presenter ondraw event
  aPresenter.ondraw = this.update;
};

TiltVisualizer.Controller.prototype = {

  /**
   * Function called each frame, updating the visualization camera transforms.
   */
  update: function TVC_update()
  {
    this.coordinates = this.arcball.update();

    this.presenter.setRotation(this.coordinates.rotation);
    this.presenter.setTranslation(this.coordinates.translation);
  },

  /**
   * Called once after every time a mouse button is pressed.
   */
  onMouseDown: function TVC_onMouseDown(e)
  {
    e.target.focus();
    e.preventDefault();
    e.stopPropagation();

    // calculate x and y coordinates using using the client and target offset
    this._downX = e.clientX - e.target.offsetLeft;
    this._downY = e.clientY - e.target.offsetTop;

    this.arcball.mouseDown(this._downX, this._downY, e.which);
  },

  /**
   * Called every time a mouse button is released.
   */
  onMouseUp: function TVC_onMouseUp(e)
  {
    e.preventDefault();
    e.stopPropagation();

    // calculate x and y coordinates using using the client and target offset
    let button = e.which;
    let upX = e.clientX - e.target.offsetLeft;
    let upY = e.clientY - e.target.offsetTop;

    this.arcball.mouseUp(upX, upY, button);
  },

  /**
   * Called every time a mouse button is clicked.
   */
  onMouseClick: function TVC_onMouseClick(e)
  {
    e.preventDefault();
    e.stopPropagation();

    // calculate x and y coordinates using using the client and target offset
    let button = e.which;
    let clickX = e.clientX - e.target.offsetLeft;
    let clickY = e.clientY - e.target.offsetTop;

    // a click in Tilt is issued only when the mouse pointer stays in
    // relatively the same position
    if (Math.abs(this._downX - clickX) < MOUSE_CLICK_THRESHOLD &&
        Math.abs(this._downY - clickY) < MOUSE_CLICK_THRESHOLD) {

      this.presenter.highlightNodeAt(clickX, clickY);
    }
  },

  /**
   * Called every time the mouse moves.
   */
  onMouseMove: function TVC_onMouseMove(e)
  {
    e.preventDefault();
    e.stopPropagation();

    // calculate x and y coordinates using using the client and target offset
    let moveX = e.clientX - e.target.offsetLeft;
    let moveY = e.clientY - e.target.offsetTop;

    this.arcball.mouseMove(moveX, moveY);
  },

  /**
   * Called when the mouse leaves the visualization bounds.
   */
  onMouseOver: function TVC_onMouseOver(e)
  {
    e.preventDefault();
    e.stopPropagation();

    this.arcball.mouseOver();
  },

  /**
   * Called when the mouse leaves the visualization bounds.
   */
  onMouseOut: function TVC_onMouseOut(e)
  {
    e.preventDefault();
    e.stopPropagation();

    this.arcball.mouseOut();
  },

  /**
   * Called when the mouse wheel is used.
   */
  onMozScroll: function TVC_onMozScroll(e)
  {
    e.preventDefault();
    e.stopPropagation();

    this.arcball.zoom(e.detail);
  },

  /**
   * Called when a key is pressed.
   */
  onKeyDown: function TVC_onKeyDown(e)
  {
    let code = e.keyCode || e.which;

    if (code >= e.DOM_VK_LEFT && code <= e.DOM_VK_DOWN) {
      e.preventDefault();
      e.stopPropagation();
    }
    this.arcball.keyDown(code);
  },

  /**
   * Called when a key is released.
   */
  onKeyUp: function TVC_onKeyUp(e)
  {
    let code = e.keyCode || e.which;

    if (code >= e.DOM_VK_LEFT && code <= e.DOM_VK_DOWN) {
      e.preventDefault();
      e.stopPropagation();
    }
    if (code === e.DOM_VK_ESCAPE) {
      this.presenter.tiltUI.destroy(this.presenter.tiltUI.currentWindowId);
      return;
    }
    this.arcball.keyUp(code);
  },

  /**
   * Called when the content window of the current browser is resized.
   */
  onResize: function TVC_onResize(e)
  {
    let width = e.target.innerWidth;
    let height = e.target.innerHeight;

    this.arcball.resize(width, height);
  },

  /**
   * Checks if this object was initialized properly.
   *
   * @return {Boolean} true if the object was initialized properly
   */
  isInitialized: function TVC_isInitialized()
  {
    return this.arcball ? true : false;
  },

  /**
   * Function called when this object is destroyed.
   */
  finalize: function TVC_finalize()
  {
    let canvas = this.canvas;
    let presenter = this.presenter;

    TiltUtils.destroyObject(this.arcball);
    TiltUtils.destroyObject(this.coordinates);

    canvas.removeEventListener("mousedown", this.onMouseDown, false);
    canvas.removeEventListener("mouseup", this.onMouseUp, false);
    canvas.removeEventListener("click", this.onMouseClick, false);
    canvas.removeEventListener("mousemove", this.onMouseMove, false);
    canvas.removeEventListener("mouseover", this.onMouseOver, false);
    canvas.removeEventListener("mouseout", this.onMouseOut, false);
    canvas.removeEventListener("MozMousePixelScroll", this.onMozScroll, false);
    canvas.removeEventListener("keydown", this.onKeyDown, false);
    canvas.removeEventListener("keyup", this.onKeyUp, false);
    presenter.contentWindow.removeEventListener("resize", this.onResize,false);
    presenter.ondraw = null;
  }
};

/**
 * This is a general purpose 3D rotation controller described by Ken Shoemake
 * in the Graphics Interface ’92 Proceedings. It features good behavior
 * easy implementation, cheap execution.
 *
 * @param {Number} aWidth
 *                 the width of canvas
 * @param {Number} aHeight
 *                 the height of canvas
 * @param {Number} aRadius
 *                 optional, the radius of the arcball
 * @param {Array} aInitialTrans
 *                optional, initial vector translation
 * @param {Array} aInitialRot
 *                optional, initial quaternion rotation
 */
TiltVisualizer.Arcball = function TV_Arcball(
  aWidth, aHeight, aRadius, aInitialTrans, aInitialRot)
{
  /**
   * Values retaining the current horizontal and vertical mouse coordinates.
   */
  this._mousePress = vec3.create();
  this._mouseRelease = vec3.create();
  this._mouseMove = vec3.create();
  this._mouseLerp = vec3.create();
  this._mouseButton = -1;

  /**
   * Object retaining the current pressed key codes.
   */
  this._keyCode = {};

  /**
   * The vectors representing the mouse coordinates mapped on the arcball
   * and their perpendicular converted from (x, y) to (x, y, z) at specific
   * events like mousePressed and mouseDragged.
   */
  this._startVec = vec3.create();
  this._endVec = vec3.create();
  this._pVec = vec3.create();

  /**
   * The corresponding rotation quaternions.
   */
  this._lastRot = quat4.create();
  this._deltaRot = quat4.create();
  this._currentRot = quat4.create(aInitialRot);

  /**
   * The current camera translation coordinates.
   */
  this._lastTrans = vec3.create();
  this._deltaTrans = vec3.create();
  this._currentTrans = vec3.create(aInitialTrans);
  this._zoomAmount = 0;

  /**
   * Additional rotation and translation vectors.
   */
  this._addKeyRot = vec3.create();
  this._addKeyTrans = vec3.create();
  this._deltaKeyRot = quat4.create();
  this._deltaKeyTrans = vec3.create();

  // set the current dimensions of the arcball
  this.resize(aWidth, aHeight, aRadius);
};

TiltVisualizer.Arcball.prototype = {

  /**
   * Call this function whenever you need the updated rotation quaternion
   * and the zoom amount. These values will be returned as "rotation" and
   * "translation" properties inside an object.
   *
   * @return {Object} the rotation quaternion and the translation amount
   */
  update: function TVA_update()
  {
    let mousePress = this._mousePress;
    let mouseRelease = this._mouseRelease;
    let mouseMove = this._mouseMove;
    let mouseLerp = this._mouseLerp;
    let mouseButton = this._mouseButton;

    // smoothly update the mouse coordinates
    mouseLerp[0] += (mouseMove[0] - mouseLerp[0]) * ARCBALL_SENSITIVITY;
    mouseLerp[1] += (mouseMove[1] - mouseLerp[1]) * ARCBALL_SENSITIVITY;

    // cache the interpolated mouse coordinates
    let x = mouseLerp[0];
    let y = mouseLerp[1];

    // the smoothed arcball rotation may not be finished when the mouse is
    // pressed again, so cancel the rotation if other events occur or the
    // animation finishes
    if (mouseButton === 3 || x === mouseRelease[0] && y === mouseRelease[1]) {
      this._rotating = false;
    }

    let startVec = this._startVec;
    let endVec = this._endVec;
    let pVec = this._pVec;

    let lastRot = this._lastRot;
    let deltaRot = this._deltaRot;
    let currentRot = this._currentRot;

    // left mouse button handles rotation
    if (mouseButton === 1 || this._rotating) {
      // the rotation doesn't stop immediately after the left mouse button is
      // released, so add a flag to smoothly continue it until it ends
      this._rotating = true;

      // find the sphere coordinates of the mouse positions
      this.pointToSphere(x, y, this.width, this.height, this.radius, endVec);

      // compute the vector perpendicular to the start & end vectors
      vec3.cross(startVec, endVec, pVec);

      // if the begin and end vectors don't coincide
      if (vec3.length(pVec) > 0) {
        deltaRot[0] = pVec[0];
        deltaRot[1] = pVec[1];
        deltaRot[2] = pVec[2];

        // in the quaternion values, w is cosine (theta / 2),
        // where theta is the rotation angle
        deltaRot[3] = -vec3.dot(startVec, endVec);
      } else {
        // return an identity rotation quaternion
        deltaRot[0] = 0;
        deltaRot[1] = 0;
        deltaRot[2] = 0;
        deltaRot[3] = 1;
      }

      // calculate the current rotation based on the mouse click events
      quat4.multiply(lastRot, deltaRot, currentRot);
    } else {
      // save the current quaternion to stack rotations
      quat4.set(currentRot, lastRot);
    }

    let lastTrans = this._lastTrans;
    let deltaTrans = this._deltaTrans;
    let currentTrans = this._currentTrans;

    // right mouse button handles panning
    if (mouseButton === 3) {
      // calculate a delta translation between the new and old mouse position
      // and save it to the current translation
      deltaTrans[0] = mouseMove[0] - mousePress[0];
      deltaTrans[1] = mouseMove[1] - mousePress[1];

      currentTrans[0] = lastTrans[0] + deltaTrans[0];
      currentTrans[1] = lastTrans[1] + deltaTrans[1];
    } else {
      // save the current panning to stack translations
      lastTrans[0] = currentTrans[0];
      lastTrans[1] = currentTrans[1];
    }

    let zoomAmount = this._zoomAmount;
    let keyCode = this._keyCode;

    // mouse wheel handles zooming
    deltaTrans[2] = (zoomAmount - currentTrans[2]) * ARCBALL_ZOOM_STEP;
    currentTrans[2] += deltaTrans[2];

    let addKeyRot = this._addKeyRot;
    let addKeyTrans = this._addKeyTrans;
    let deltaKeyRot = this._deltaKeyRot;
    let deltaKeyTrans = this._deltaKeyTrans;

    // handle additional rotation and translation by the keyboard
    if (keyCode[Ci.nsIDOMKeyEvent.DOM_VK_A]) {
      addKeyRot[0] -= ARCBALL_SENSITIVITY * ARCBALL_ROTATION_STEP;
    }
    if (keyCode[Ci.nsIDOMKeyEvent.DOM_VK_D]) {
      addKeyRot[0] += ARCBALL_SENSITIVITY * ARCBALL_ROTATION_STEP;
    }
    if (keyCode[Ci.nsIDOMKeyEvent.DOM_VK_W]) {
      addKeyRot[1] += ARCBALL_SENSITIVITY * ARCBALL_ROTATION_STEP;
    }
    if (keyCode[Ci.nsIDOMKeyEvent.DOM_VK_S]) {
      addKeyRot[1] -= ARCBALL_SENSITIVITY * ARCBALL_ROTATION_STEP;
    }
    if (keyCode[Ci.nsIDOMKeyEvent.DOM_VK_LEFT]) {
      addKeyTrans[0] -= ARCBALL_SENSITIVITY * ARCBALL_TRANSLATION_STEP;
    }
    if (keyCode[Ci.nsIDOMKeyEvent.DOM_VK_RIGHT]) {
      addKeyTrans[0] += ARCBALL_SENSITIVITY * ARCBALL_TRANSLATION_STEP;
    }
    if (keyCode[Ci.nsIDOMKeyEvent.DOM_VK_UP]) {
      addKeyTrans[1] -= ARCBALL_SENSITIVITY * ARCBALL_TRANSLATION_STEP;
    }
    if (keyCode[Ci.nsIDOMKeyEvent.DOM_VK_DOWN]) {
      addKeyTrans[1] += ARCBALL_SENSITIVITY * ARCBALL_TRANSLATION_STEP;
    }
    if (keyCode[Ci.nsIDOMKeyEvent.DOM_VK_I] ||
        keyCode[Ci.nsIDOMKeyEvent.DOM_VK_ADD] ||
        keyCode[Ci.nsIDOMKeyEvent.DOM_VK_EQUALS]) {
      this.zoom(-ARCBALL_TRANSLATION_STEP);
    }
    if (keyCode[Ci.nsIDOMKeyEvent.DOM_VK_O] ||
        keyCode[Ci.nsIDOMKeyEvent.DOM_VK_SUBTRACT]) {
      this.zoom(ARCBALL_TRANSLATION_STEP);
    }
    if (keyCode[Ci.nsIDOMKeyEvent.DOM_VK_R] ||
        keyCode[Ci.nsIDOMKeyEvent.DOM_VK_0]) {
      this._zoomAmount = 0;
    }

    // update the delta key rotations and translations
    deltaKeyRot[0] +=
      (addKeyRot[0] - deltaKeyRot[0]) * ARCBALL_SENSITIVITY;
    deltaKeyRot[1] +=
      (addKeyRot[1] - deltaKeyRot[1]) * ARCBALL_SENSITIVITY;

    deltaKeyTrans[0] +=
      (addKeyTrans[0] - deltaKeyTrans[0]) * ARCBALL_SENSITIVITY;
    deltaKeyTrans[1] +=
      (addKeyTrans[1] - deltaKeyTrans[1]) * ARCBALL_SENSITIVITY;

    // create an additional rotation based on the key events
    quat4.fromEuler(deltaKeyRot[0], deltaKeyRot[1], 0, deltaRot);

    // create an additional translation based on the key events
    deltaTrans[0] = deltaKeyTrans[0];
    deltaTrans[1] = deltaKeyTrans[1];
    deltaTrans[2] = 0;

    // return the current rotation and translation
    return {
      rotation: quat4.multiply(deltaRot, currentRot),
      translation: vec3.add(deltaTrans, currentTrans)
    };
  },

  /**
   * Function handling the mouseDown event.
   * Call this when the mouse was pressed.
   *
   * @param {Number} x
   *                 the current horizontal coordinate of the mouse
   * @param {Number} y
   *                 the current vertical coordinate of the mouse
   * @param {Number} aButton
   *                 which mouse button was pressed
   */
  mouseDown: function TVA_mouseDown(x, y, aButton)
  {
    // save the mouse down state and prepare for rotations or translations
    this._mousePress[0] = x;
    this._mousePress[1] = y;
    this._mouseButton = aButton;
    this._save();

    // find the sphere coordinates of the mouse positions
    this.pointToSphere(
      x, y, this.width, this.height, this.radius, this._startVec);

    quat4.set(this._currentRot, this._lastRot);
  },

  /**
   * Function handling the mouseUp event.
   * Call this when a mouse button was released.
   *
   * @param {Number} x
   *                 the current horizontal coordinate of the mouse
   * @param {Number} y
   *                 the current vertical coordinate of the mouse
   */
  mouseUp: function TVA_mouseUp(x, y)
  {
    // save the mouse up state and prepare for rotations or translations
    this._mouseRelease[0] = x;
    this._mouseRelease[1] = y;
    this._mouseButton = -1;
  },

  /**
   * Function handling the mouseMove event.
   * Call this when the mouse was moved.
   *
   * @param {Number} x
   *                 the current horizontal coordinate of the mouse
   * @param {Number} y
   *                 the current vertical coordinate of the mouse
   */
  mouseMove: function TVA_mouseMove(x, y)
  {
    // save the mouse move state and prepare for rotations or translations
    // only if the mouse is pressed
    if (this._mouseButton !== -1) {
      this._mouseMove[0] = x;
      this._mouseMove[1] = y;
    }
  },

  /**
   * Function handling the mouseOver event.
   * Call this when the mouse enters the context bounds.
   */
  mouseOver: function TVA_mouseOver()
  {
    // if the mouse just entered the parent bounds, stop the animation
    this._mouseButton = -1;
  },

  /**
   * Function handling the mouseOut event.
   * Call this when the mouse leaves the context bounds.
   */
  mouseOut: function TVA_mouseOut()
  {
    // if the mouse leaves the parent bounds, stop the animation
    this._mouseButton = -1;
  },

  /**
   * Function handling the arcball zoom amount.
   * Call this, for example, when the mouse wheel was scrolled or zoom keys
   * were pressed.
   *
   * @param {Number} aZoom
   *                 the zoom direction and speed
   */
  zoom: function TVA_zoom(aZoom)
  {
    this._zoomAmount = TiltMath.clamp(this._zoomAmount - aZoom,
      ARCBALL_ZOOM_MIN, ARCBALL_ZOOM_MAX);
  },

  /**
   * Function handling the keyDown event.
   * Call this when a key was pressed.
   *
   * @param {Number} aCode
   *                 the code corresponding to the key pressed
   */
  keyDown: function TVA_keyDown(aCode)
  {
    this._keyCode[aCode] = true;
  },

  /**
   * Function handling the keyUp event.
   * Call this when a key was released.
   *
   * @param {Number} aCode
   *                 the code corresponding to the key released
   */
  keyUp: function TVA_keyUp(aCode)
  {
    this._keyCode[aCode] = false;
  },

  /**
   * Maps the 2d coordinates of the mouse location to a 3d point on a sphere.
   *
   * @param {Number} x
   *                 the current horizontal coordinate of the mouse
   * @param {Number} y
   *                 the current vertical coordinate of the mouse
   * @param {Number} aWidth
   *                 the width of canvas
   * @param {Number} aHeight
   *                 the height of canvas
   * @param {Number} aRadius
   *                 optional, the radius of the arcball
   * @param {Array} aSphereVec
   *                a 3d vector to store the sphere coordinates
   */
  pointToSphere: function TVA_pointToSphere(
    x, y, aWidth, aHeight, aRadius, aSphereVec)
  {
    // adjust point coords and scale down to range of [-1..1]
    x = (x - aWidth * 0.5) / aRadius;
    y = (y - aHeight * 0.5) / aRadius;

    // compute the square length of the vector to the point from the center
    let normal = 0;
    let sqlength = x * x + y * y;

    // if the point is mapped outside of the sphere
    if (sqlength > 1) {
      // calculate the normalization factor
      normal = 1 / Math.sqrt(sqlength);

      // set the normalized vector (a point on the sphere)
      aSphereVec[0] = x * normal;
      aSphereVec[1] = y * normal;
      aSphereVec[2] = 0;
    } else {
      // set the vector to a point mapped inside the sphere
      aSphereVec[0] = x;
      aSphereVec[1] = y;
      aSphereVec[2] = Math.sqrt(1 - sqlength);
    }
  },

  /**
   * Resize this implementation to use different bounds.
   * This function is automatically called when the arcball is created.
   *
   * @param {Number} newWidth
   *                 the new width of canvas
   * @param {Number} newHeight
   *                 the new  height of canvas
   * @param {Number} newRadius
   *                 optional, the new radius of the arcball
   */
  resize: function TVA_resize(newWidth, newHeight, newRadius)
  {
    if (!newWidth || !newHeight) {
      return;
    }

    // set the new width, height and radius dimensions
    this.width = newWidth;
    this.height = newHeight;
    this.radius = newRadius ? newRadius : Math.min(newWidth, newHeight);
    this._save();
  },

  /**
   * Saves the current arcball state, typically after resize or mouse events.
   */
  _save: function TVA__save()
  {
    let x = this._mousePress[0];
    let y = this._mousePress[1];

    this._mouseMove[0] = x;
    this._mouseMove[1] = y;
    this._mouseRelease[0] = x;
    this._mouseRelease[1] = y;
    this._mouseLerp[0] = x;
    this._mouseLerp[1] = y;
  }
};

/**
 * Tilt configuration preferences.
 */
TiltVisualizer.Prefs = {

  /**
   * Specifies if Tilt is enabled or not.
   */
  get enabled()
  {
    return this._enabled;
  },

  set enabled(value)
  {
    TiltUtils.Preferences.set("enabled", "boolean", value);
    this._enabled = value;
  },

  /**
   * Specifies if Tilt should be enabled even if WebGL may not be available.
   */
  get forceEnabled()
  {
    return this._forceEnabled;
  },

  set forceEnabled(value)
  {
    TiltUtils.Preferences.set("force-enabled", "boolean", value);
    this._forceEnabled = value;
  },

  /**
   * Loads the preferences.
   */
  load: function TVC_load()
  {
    let prefs = TiltUtils.Preferences;

    TiltVisualizer.Prefs._enabled = prefs.get("enabled", "boolean");
    TiltVisualizer.Prefs._forceEnabled = prefs.get("force-enabled", "boolean");
  }
};

/**
 * A custom visualization shader.
 *
 * @param {Attribute} vertexPosition: the vertex position
 * @param {Attribute} vertexTexCoord: texture coordinates used by the sampler
 * @param {Attribute} vertexColor: specific [r, g, b] color for each vertex
 * @param {Uniform} mvMatrix: the model view matrix
 * @param {Uniform} projMatrix: the projection matrix
 * @param {Uniform} sampler: the texture sampler to fetch the pixels from
 */
TiltVisualizer.MeshShader = {

  /**
   * Vertex shader.
   */
  vs: [
    "attribute vec3 vertexPosition;",
    "attribute vec2 vertexTexCoord;",
    "attribute vec3 vertexColor;",

    "uniform mat4 mvMatrix;",
    "uniform mat4 projMatrix;",

    "varying vec2 texCoord;",
    "varying vec3 color;",

    "void main() {",
    "  gl_Position = projMatrix * mvMatrix * vec4(vertexPosition, 1.0);",
    "  texCoord = vertexTexCoord;",
    "  color = vertexColor;",
    "}"
  ].join("\n"),

  /**
   * Fragment shader.
   */
  fs: [
    "#ifdef GL_ES",
    "precision lowp float;",
    "#endif",

    "uniform sampler2D sampler;",

    "varying vec2 texCoord;",
    "varying vec3 color;",

    "void main() {",
    "  if (texCoord.x < 0.0) {",
    "    gl_FragColor = vec4(color, 1.0);",
    "  } else {",
    "    gl_FragColor = vec4(texture2D(sampler, texCoord).rgb, 1.0);",
    "  }",
    "}"
  ].join("\n")
};
