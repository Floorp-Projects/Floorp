/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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

// a node is represented in the visualization mesh as a rectangular stack
// of 5 quads composed of 12 vertices; we draw these as triangles using an
// index buffer of 12 unsigned int elements, obviously one for each vertex;
// if a webpage has enough nodes to overflow the index buffer elements size,
// weird things may happen; thus, when necessary, we'll split into groups
const MAX_GROUP_NODES = Math.pow(2, Uint16Array.BYTES_PER_ELEMENT * 8) / 12 - 1;

const WIREFRAME_COLOR = [0, 0, 0, 0.25];
const INTRO_TRANSITION_DURATION = 1000;
const OUTRO_TRANSITION_DURATION = 800;
const INITIAL_Z_TRANSLATION = 400;
const MOVE_INTO_VIEW_ACCURACY = 50;

const MOUSE_CLICK_THRESHOLD = 10;
const MOUSE_INTRO_DELAY = 200;
const ARCBALL_SENSITIVITY = 0.5;
const ARCBALL_ROTATION_STEP = 0.15;
const ARCBALL_TRANSLATION_STEP = 35;
const ARCBALL_ZOOM_STEP = 0.1;
const ARCBALL_ZOOM_MIN = -3000;
const ARCBALL_ZOOM_MAX = 500;
const ARCBALL_RESET_SPHERICAL_FACTOR = 0.1;
const ARCBALL_RESET_LINEAR_FACTOR = 0.01;

const TILT_CRAFTER = "resource:///modules/devtools/TiltWorkerCrafter.js";
const TILT_PICKER = "resource:///modules/devtools/TiltWorkerPicker.js";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/gDevTools.jsm");
Cu.import("resource:///modules/devtools/Target.jsm");
Cu.import("resource:///modules/devtools/TiltGL.jsm");
Cu.import("resource:///modules/devtools/TiltMath.jsm");
Cu.import("resource:///modules/devtools/TiltUtils.jsm");
Cu.import("resource:///modules/devtools/TiltVisualizerStyle.jsm");

this.EXPORTED_SYMBOLS = ["TiltVisualizer"];

/**
 * Initializes the visualization presenter and controller.
 *
 * @param {Object} aProperties
 *                 an object containing the following properties:
 *        {Window} chromeWindow: a reference to the top level window
 *        {Window} contentWindow: the content window holding the visualized doc
 *       {Element} parentNode: the parent node to hold the visualization
 *        {Object} notifications: necessary notifications for Tilt
 *      {Function} onError: optional, function called if initialization failed
 *      {Function} onLoad: optional, function called if initialization worked
 */
this.TiltVisualizer = function TiltVisualizer(aProperties)
{
  // make sure the properties parameter is a valid object
  aProperties = aProperties || {};

  /**
   * Save a reference to the top-level window.
   */
  this.chromeWindow = aProperties.chromeWindow;

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
    aProperties.chromeWindow,
    aProperties.contentWindow,
    aProperties.notifications,
    aProperties.onError || null,
    aProperties.onLoad || null);

  this.bindToInspector(aProperties.tab);

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
    this.unbindInspector();

    if (this.controller) {
      TiltUtils.destroyObject(this.controller);
    }
    if (this.presenter) {
      TiltUtils.destroyObject(this.presenter);
    }

    let chromeWindow = this.chromeWindow;

    TiltUtils.destroyObject(this);
    TiltUtils.clearCache();
    TiltUtils.gc(chromeWindow);
  },

  /**
   * Listen to the inspector activity.
   */
  bindToInspector: function TV_bindToInspector(aTab)
  {
    this._browserTab = aTab;

    this.onNewNodeFromInspector = this.onNewNodeFromInspector.bind(this);
    this.onNewNodeFromTilt = this.onNewNodeFromTilt.bind(this);
    this.onInspectorReady = this.onInspectorReady.bind(this);
    this.onToolboxDestroyed = this.onToolboxDestroyed.bind(this);

    gDevTools.on("inspector-ready", this.onInspectorReady);
    gDevTools.on("toolbox-destroyed", this.onToolboxDestroyed);

    Services.obs.addObserver(this.onNewNodeFromTilt,
                             this.presenter.NOTIFICATIONS.HIGHLIGHTING,
                             false);
    Services.obs.addObserver(this.onNewNodeFromTilt,
                             this.presenter.NOTIFICATIONS.UNHIGHLIGHTING,
                             false);

    let target = TargetFactory.forTab(aTab);
    let toolbox = gDevTools.getToolbox(target);
    if (toolbox) {
      let panel = toolbox.getPanel("inspector");
      if (panel) {
        this.inspector = panel;
        this.inspector.selection.on("new-node", this.onNewNodeFromInspector);
        this.onNewNodeFromInspector();
      }
    }
  },

  /**
   * Unregister inspector event listeners.
   */
  unbindInspector: function TV_unbindInspector()
  {
    this._browserTab = null;

    if (this.inspector) {
      if (this.inspector.selection) {
        this.inspector.selection.off("new-node", this.onNewNodeFromInspector);
      }
      this.inspector = null;
    }

    gDevTools.off("inspector-ready", this.onInspectorReady);
    gDevTools.off("toolbox-destroyed", this.onToolboxDestroyed);

    Services.obs.removeObserver(this.onNewNodeFromTilt,
                                this.presenter.NOTIFICATIONS.HIGHLIGHTING);
    Services.obs.removeObserver(this.onNewNodeFromTilt,
                                this.presenter.NOTIFICATIONS.UNHIGHLIGHTING);
  },

  /**
   * When a new inspector is started.
   */
  onInspectorReady: function TV_onInspectorReady(event, toolbox, panel)
  {
    if (toolbox.target.tab === this._browserTab) {
      this.inspector = panel;
      this.inspector.selection.on("new-node", this.onNewNodeFromInspector);
      this.onNewNodeFromTilt();
    }
  },

  /**
   * When the toolbox, therefor the inspector, is closed.
   */
  onToolboxDestroyed: function TV_onToolboxDestroyed(event, tab)
  {
    if (tab === this._browserTab &&
        this.inspector) {
      if (this.inspector.selection) {
        this.inspector.selection.off("new-node", this.onNewNodeFromInspector);
      }
      this.inspector = null;
    }
  },

  /**
   * When a new node is selected in the inspector.
   */
  onNewNodeFromInspector: function TV_onNewNodeFromInspector()
  {
    if (this.inspector &&
        this.inspector.selection.reason != "tilt") {
      let selection = this.inspector.selection;
      let canHighlightNode = selection.isNode() &&
                              selection.isConnected() &&
                              selection.isElementNode();
      if (canHighlightNode) {
        this.presenter.highlightNode(selection.node);
      } else {
        this.presenter.highlightNodeFor(-1);
      }
    }
  },

  /**
   * When a new node is selected in Tilt.
   */
  onNewNodeFromTilt: function TV_onNewNodeFromTilt()
  {
    if (!this.inspector) {
      return;
    }
    let nodeIndex = this.presenter._currentSelection;
    if (nodeIndex < 0) {
      this.inspector.selection.setNode(null, "tilt");
    }
    let node = this.presenter._traverseData.nodes[nodeIndex];
    this.inspector.selection.setNode(node, "tilt");
  },
};

/**
 * This object manages the visualization logic and drawing loop.
 *
 * @param {HTMLCanvasElement} aCanvas
 *                            the canvas element used for rendering
 * @param {Window} aChromeWindow
 *                 a reference to the top-level window
 * @param {Window} aContentWindow
 *                 the content window holding the document to be visualized
 * @param {Object} aNotifications
 *                 necessary notifications for Tilt
 * @param {Function} onError
 *                   function called if initialization failed
 * @param {Function} onLoad
 *                   function called if initialization worked
 */
TiltVisualizer.Presenter = function TV_Presenter(
  aCanvas, aChromeWindow, aContentWindow, aNotifications, onError, onLoad)
{
  /**
   * A canvas overlay used for drawing the visualization.
   */
  this.canvas = aCanvas;

  /**
   * Save a reference to the top-level window, to access Tilt.
   */
  this.chromeWindow = aChromeWindow;

  /**
   * The content window generating the visualization
   */
  this.contentWindow = aContentWindow;

  /**
   * Shortcut for accessing notifications strings.
   */
  this.NOTIFICATIONS = aNotifications;

  /**
   * Create the renderer, containing useful functions for easy drawing.
   */
  this._renderer = new TiltGL.Renderer(aCanvas, onError, onLoad);

  /**
   * A custom shader used for drawing the visualization mesh.
   */
  this._visualizationProgram = null;

  /**
   * The combined mesh representing the document visualization.
   */
  this._texture = null;
  this._meshData = null;
  this._meshStacks = null;
  this._meshWireframe = null;
  this._traverseData = null;

  /**
   * A highlight quad drawn over a stacked dom node.
   */
  this._highlight = {
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
    zoom: 1,
    offset: vec3.create(),      // mesh offset, aligned to the viewport center
    translation: vec3.create(), // scene translation, on the [x, y, z] axis
    rotation: quat4.create()    // scene rotation, expressed as a quaternion
  };

  /**
   * Variables holding information about the initial and current node selected.
   */
  this._currentSelection = -1; // the selected node index
  this._initialMeshConfiguration = false; // true if the 3D mesh was configured

  /**
   * Variable specifying if the scene should be redrawn.
   * This should happen usually when the visualization is translated/rotated.
   */
  this._redraw = true;

  /**
   * Total time passed since the rendering started.
   * If the rendering is paused, this property won't get updated.
   */
  this._time = 0;

  /**
   * Frame delta time (the ammount of time passed for each frame).
   * This is used to smoothly interpolate animation transfroms.
   */
  this._delta = 0;
  this._prevFrameTime = 0;
  this._currFrameTime = 0;


  this._setup();
  this._loop();
};

TiltVisualizer.Presenter.prototype = {

  /**
   * The initialization logic.
   */
  _setup: function TVP__setup()
  {
    let renderer = this._renderer;

    // if the renderer was destroyed, don't continue setup
    if (!renderer || !renderer.context) {
      return;
    }

    // create the visualization shaders and program to draw the stacks mesh
    this._visualizationProgram = new renderer.Program({
      vs: TiltVisualizer.MeshShader.vs,
      fs: TiltVisualizer.MeshShader.fs,
      attributes: ["vertexPosition", "vertexTexCoord", "vertexColor"],
      uniforms: ["mvMatrix", "projMatrix", "sampler"]
    });

    // get the document zoom to properly scale the visualization
    this.transforms.zoom = this._getPageZoom();

    // bind the owner object to the necessary functions
    TiltUtils.bindObjectFunc(this, "^_on");
    TiltUtils.bindObjectFunc(this, "_loop");

    this._setupTexture();
    this._setupMeshData();
    this._setupEventListeners();
    this.canvas.focus();
  },

  /**
   * Get page zoom factor.
   * @return {Number}
   */
  _getPageZoom: function TVP__getPageZoom() {
    return this.contentWindow
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils)
      .fullZoom;
  },

  /**
   * The animation logic.
   */
  _loop: function TVP__loop()
  {
    let renderer = this._renderer;

    // if the renderer was destroyed, don't continue rendering
    if (!renderer || !renderer.context) {
      return;
    }

    // prepare for the next frame of the animation loop
    this.chromeWindow.mozRequestAnimationFrame(this._loop);

    // only redraw if we really have to
    if (this._redraw) {
      this._redraw = false;
      this._drawVisualization();
    }

    // update the current presenter transfroms from the controller
    if ("function" === typeof this._controllerUpdate) {
      this._controllerUpdate(this._time, this._delta);
    }

    this._handleFrameDelta();
    this._handleKeyframeNotifications();
  },

  /**
   * Calculates the current frame delta time.
   */
  _handleFrameDelta: function TVP__handleFrameDelta()
  {
    this._prevFrameTime = this._currFrameTime;
    this._currFrameTime = this.chromeWindow.mozAnimationStartTime;
    this._delta = this._currFrameTime - this._prevFrameTime;
  },

  /**
   * Draws the visualization mesh and highlight quad.
   */
  _drawVisualization: function TVP__drawVisualization()
  {
    let renderer = this._renderer;
    let transforms = this.transforms;
    let w = renderer.width;
    let h = renderer.height;
    let ih = renderer.initialHeight;

    // if the mesh wasn't created yet, don't continue rendering
    if (!this._meshStacks || !this._meshWireframe) {
      return;
    }

    // clear the context to an opaque black background
    renderer.clear();
    renderer.perspective();

    // apply a transition transformation using an ortho and perspective matrix
    let ortho = mat4.ortho(0, w, h, 0, -1000, 1000);

    if (!this._isExecutingDestruction) {
      let f = this._time / INTRO_TRANSITION_DURATION;
      renderer.lerp(renderer.projMatrix, ortho, f, 8);
    } else {
      let f = this._time / OUTRO_TRANSITION_DURATION;
      renderer.lerp(renderer.projMatrix, ortho, 1 - f, 8);
    }

    // apply the preliminary transformations to the model view
    renderer.translate(w * 0.5, ih * 0.5, -INITIAL_Z_TRANSLATION);

    // calculate the camera matrix using the rotation and translation
    renderer.translate(transforms.translation[0], 0,
                       transforms.translation[2]);

    renderer.transform(quat4.toMat4(transforms.rotation));

    // offset the visualization mesh to center
    renderer.translate(transforms.offset[0],
                       transforms.offset[1] + transforms.translation[1], 0);

    renderer.scale(transforms.zoom, transforms.zoom);

    // draw the visualization mesh
    renderer.strokeWeight(2);
    renderer.depthTest(true);
    this._drawMeshStacks();
    this._drawMeshWireframe();
    this._drawHighlight();

    // make sure the initial transition is drawn until finished
    if (this._time < INTRO_TRANSITION_DURATION ||
        this._time < OUTRO_TRANSITION_DURATION) {
      this._redraw = true;
    }
    this._time += this._delta;
  },

  /**
   * Draws the meshStacks object.
   */
  _drawMeshStacks: function TVP__drawMeshStacks()
  {
    let renderer = this._renderer;
    let mesh = this._meshStacks;

    let visualizationProgram = this._visualizationProgram;
    let texture = this._texture;
    let mvMatrix = renderer.mvMatrix;
    let projMatrix = renderer.projMatrix;

    // use the necessary shader
    visualizationProgram.use();

    for (let i = 0, len = mesh.length; i < len; i++) {
      let group = mesh[i];

      // bind the attributes and uniforms as necessary
      visualizationProgram.bindVertexBuffer("vertexPosition", group.vertices);
      visualizationProgram.bindVertexBuffer("vertexTexCoord", group.texCoord);
      visualizationProgram.bindVertexBuffer("vertexColor", group.color);

      visualizationProgram.bindUniformMatrix("mvMatrix", mvMatrix);
      visualizationProgram.bindUniformMatrix("projMatrix", projMatrix);
      visualizationProgram.bindTexture("sampler", texture);

      // draw the vertices as TRIANGLES indexed elements
      renderer.drawIndexedVertices(renderer.context.TRIANGLES, group.indices);
    }

    // save the current model view and projection matrices
    mesh.mvMatrix = mat4.create(mvMatrix);
    mesh.projMatrix = mat4.create(projMatrix);
  },

  /**
   * Draws the meshWireframe object.
   */
  _drawMeshWireframe: function TVP__drawMeshWireframe()
  {
    let renderer = this._renderer;
    let mesh = this._meshWireframe;

    for (let i = 0, len = mesh.length; i < len; i++) {
      let group = mesh[i];

      // use the necessary shader
      renderer.useColorShader(group.vertices, WIREFRAME_COLOR);

      // draw the vertices as LINES indexed elements
      renderer.drawIndexedVertices(renderer.context.LINES, group.indices);
    }
  },

  /**
   * Draws a highlighted quad around a currently selected node.
   */
  _drawHighlight: function TVP__drawHighlight()
  {
    // check if there's anything to highlight (i.e any node is selected)
    if (!this._highlight.disabled) {

      // set the corresponding state to draw the highlight quad
      let renderer = this._renderer;
      let highlight = this._highlight;

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
  _setupTexture: function TVP__setupTexture()
  {
    let renderer = this._renderer;

    // destroy any previously created texture
    TiltUtils.destroyObject(this._texture); this._texture = null;

    // if the renderer was destroyed, don't continue setup
    if (!renderer || !renderer.context) {
      return;
    }

    // get the maximum texture size
    this._maxTextureSize =
      renderer.context.getParameter(renderer.context.MAX_TEXTURE_SIZE);

    // use a simple shim to get the image representation of the document
    // this will be removed once the MOZ_window_region_texture bug #653656
    // is finished; currently just converting the document image to a texture
    // applied to the mesh
    this._texture = new renderer.Texture({
      source: TiltGL.TextureUtils.createContentImage(this.contentWindow,
                                                     this._maxTextureSize),
      format: "RGB"
    });

    if ("function" === typeof this._onSetupTexture) {
      this._onSetupTexture();
      this._onSetupTexture = null;
    }
  },

  /**
   * Create the combined mesh representing the document visualization by
   * traversing the document & adding a stack for each node that is drawable.
   *
   * @param {Object} aMeshData
   *                 object containing the necessary mesh verts, texcoord etc.
   */
  _setupMesh: function TVP__setupMesh(aMeshData)
  {
    let renderer = this._renderer;

    // destroy any previously created mesh
    TiltUtils.destroyObject(this._meshStacks); this._meshStacks = [];
    TiltUtils.destroyObject(this._meshWireframe); this._meshWireframe = [];

    // if the renderer was destroyed, don't continue setup
    if (!renderer || !renderer.context) {
      return;
    }

    // save the mesh data for future use
    this._meshData = aMeshData;

    // create a sub-mesh for each group in the mesh data
    for (let i = 0, len = aMeshData.groups.length; i < len; i++) {
      let group = aMeshData.groups[i];

      // create the visualization mesh using the vertices, texture coordinates
      // and indices computed when traversing the document object model
      this._meshStacks.push({
        vertices: new renderer.VertexBuffer(group.vertices, 3),
        texCoord: new renderer.VertexBuffer(group.texCoord, 2),
        color: new renderer.VertexBuffer(group.color, 3),
        indices: new renderer.IndexBuffer(group.stacksIndices)
      });

      // additionally, create a wireframe representation to make the
      // visualization a bit more pretty
      this._meshWireframe.push({
        vertices: this._meshStacks[i].vertices,
        indices: new renderer.IndexBuffer(group.wireframeIndices)
      });
    }

    // configure the required mesh transformations and background only once
    if (!this._initialMeshConfiguration) {
      this._initialMeshConfiguration = true;

      // set the necessary mesh offsets
      this.transforms.offset[0] = -renderer.width * 0.5;
      this.transforms.offset[1] = -renderer.height * 0.5;

      // make sure the canvas is opaque now that the initialization is finished
      this.canvas.style.background = TiltVisualizerStyle.canvas.background;

      this._drawVisualization();
      this._redraw = true;
    }

    if ("function" === typeof this._onSetupMesh) {
      this._onSetupMesh();
      this._onSetupMesh = null;
    }
  },

  /**
   * Computes the mesh vertices, texture coordinates etc. by groups of nodes.
   */
  _setupMeshData: function TVP__setupMeshData()
  {
    let renderer = this._renderer;

    // if the renderer was destroyed, don't continue setup
    if (!renderer || !renderer.context) {
      return;
    }

    // traverse the document and get the depths, coordinates and local names
    this._traverseData = TiltUtils.DOM.traverse(this.contentWindow, {
      invisibleElements: INVISIBLE_ELEMENTS,
      minSize: ELEMENT_MIN_SIZE,
      maxX: this._texture.width,
      maxY: this._texture.height
    });

    let worker = new ChromeWorker(TILT_CRAFTER);

    worker.addEventListener("message", function TVP_onMessage(event) {
      this._setupMesh(event.data);
    }.bind(this), false);

    // calculate necessary information regarding vertices, texture coordinates
    // etc. in a separate thread, as this process may take a while
    worker.postMessage({
      maxGroupNodes: MAX_GROUP_NODES,
      style: TiltVisualizerStyle.nodes,
      texWidth: this._texture.width,
      texHeight: this._texture.height,
      nodesInfo: this._traverseData.info
    });
  },

  /**
   * Sets up event listeners necessary for the presenter.
   */
  _setupEventListeners: function TVP__setupEventListeners()
  {
    this.contentWindow.addEventListener("resize", this._onResize, false);
  },

  /**
   * Called when the content window of the current browser is resized.
   */
  _onResize: function TVP_onResize(e)
  {
    let zoom = this._getPageZoom();
    let width = e.target.innerWidth * zoom;
    let height = e.target.innerHeight * zoom;

    // handle aspect ratio changes to update the projection matrix
    this._renderer.width = width;
    this._renderer.height = height;

    this._redraw = true;
  },

  /**
   * Highlights a specific node.
   *
   * @param {Element} aNode
   *                  the html node to be highlighted
   * @param {String} aFlags
   *                 flags specifying highlighting options
   */
  highlightNode: function TVP_highlightNode(aNode, aFlags)
  {
    this.highlightNodeFor(this._traverseData.nodes.indexOf(aNode), aFlags);
  },

  /**
   * Picks a stacked dom node at the x and y screen coordinates and highlights
   * the selected node in the mesh.
   *
   * @param {Number} x
   *                 the current horizontal coordinate of the mouse
   * @param {Number} y
   *                 the current vertical coordinate of the mouse
   * @param {Object} aProperties
   *                 an object containing the following properties:
   *      {Function} onpick: function to be called after picking succeeded
   *      {Function} onfail: function to be called after picking failed
   */
  highlightNodeAt: function TVP_highlightNodeAt(x, y, aProperties)
  {
    // make sure the properties parameter is a valid object
    aProperties = aProperties || {};

    // try to pick a mesh node using the current x, y coordinates
    this.pickNode(x, y, {

      /**
       * Mesh picking failed (nothing was found for the picked point).
       */
      onfail: function TVP_onHighlightFail()
      {
        this.highlightNodeFor(-1);

        if ("function" === typeof aProperties.onfail) {
          aProperties.onfail();
        }
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

        if ("function" === typeof aProperties.onpick) {
          aProperties.onpick();
        }
      }.bind(this)
    });
  },

  /**
   * Sets the corresponding highlight coordinates and color based on the
   * information supplied.
   *
   * @param {Number} aNodeIndex
   *                 the index of the node in the this._traverseData array
   * @param {String} aFlags
   *                 flags specifying highlighting options
   */
  highlightNodeFor: function TVP_highlightNodeFor(aNodeIndex, aFlags)
  {
    this._redraw = true;

    // if the node was already selected, don't do anything
    if (this._currentSelection === aNodeIndex) {
      return;
    }

    // if an invalid or nonexisted node is specified, disable the highlight
    if (aNodeIndex < 0) {
      this._currentSelection = -1;
      this._highlight.disabled = true;

      Services.obs.notifyObservers(null, this.NOTIFICATIONS.UNHIGHLIGHTING, null);
      return;
    }

    let highlight = this._highlight;
    let info = this._traverseData.info[aNodeIndex];
    let style = TiltVisualizerStyle.nodes;

    highlight.disabled = false;
    highlight.fill = style[info.name] || style.highlight.defaultFill;
    highlight.stroke = style.highlight.defaultStroke;
    highlight.strokeWeight = style.highlight.defaultStrokeWeight;

    let x = info.coord.left;
    let y = info.coord.top;
    let w = info.coord.width;
    let h = info.coord.height;
    let z = info.coord.depth + info.coord.thickness;

    vec3.set([x,     y,     z], highlight.v0);
    vec3.set([x + w, y,     z], highlight.v1);
    vec3.set([x + w, y + h, z], highlight.v2);
    vec3.set([x,     y + h, z], highlight.v3);

    this._currentSelection = aNodeIndex;

    // if something is highlighted, make sure it's inside the current viewport;
    // the point which should be moved into view is considered the center [x, y]
    // position along the top edge of the currently selected node

    if (aFlags && aFlags.indexOf("moveIntoView") !== -1)
    {
      this.controller.arcball.moveIntoView(vec3.lerp(
        vec3.scale(this._highlight.v0, this.transforms.zoom, []),
        vec3.scale(this._highlight.v1, this.transforms.zoom, []), 0.5));
    }

    Services.obs.notifyObservers(null, this.NOTIFICATIONS.HIGHLIGHTING, null);
  },

  /**
   * Deletes a node from the visualization mesh.
   *
   * @param {Number} aNodeIndex
   *                 the index of the node in the this._traverseData array;
   *                 if not specified, it will default to the current selection
   */
  deleteNode: function TVP_deleteNode(aNodeIndex)
  {
    // we probably don't want to delete the html or body node.. just sayin'
    if ((aNodeIndex = aNodeIndex || this._currentSelection) < 1) {
      return;
    }

    let renderer = this._renderer;

    let groupIndex = parseInt(aNodeIndex / MAX_GROUP_NODES);
    let nodeIndex = parseInt((aNodeIndex + (groupIndex ? 1 : 0)) % MAX_GROUP_NODES);
    let group = this._meshStacks[groupIndex];
    let vertices = group.vertices.components;

    for (let i = 0, k = 36 * nodeIndex; i < 36; i++) {
      vertices[i + k] = 0;
    }

    group.vertices = new renderer.VertexBuffer(vertices, 3);
    this._highlight.disabled = true;
    this._redraw = true;

    Services.obs.notifyObservers(null, this.NOTIFICATIONS.NODE_REMOVED, null);
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
    if (!this._meshStacks || !this._meshWireframe) {
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

    let zoom = this._getPageZoom();
    let width = this._renderer.width * zoom;
    let height = this._renderer.height * zoom;
    x *= zoom;
    y *= zoom;

    // create a ray following the mouse direction from the near clipping plane
    // to the far clipping plane, to check for intersections with the mesh,
    // and do all the heavy lifting in a separate thread
    worker.postMessage({
      vertices: this._meshData.allVertices,

      // create the ray destined for 3D picking
      ray: vec3.createRay([x, y, 0], [x, y, 1], [0, 0, width, height],
        this._meshStacks.mvMatrix,
        this._meshStacks.projMatrix)
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
      this._redraw = true;
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
      this._redraw = true;
    }
  },

  /**
   * Handles notifications at specific frame counts.
   */
  _handleKeyframeNotifications: function TV__handleKeyframeNotifications()
  {
    if (!TiltVisualizer.Prefs.introTransition && !this._isExecutingDestruction) {
      this._time = INTRO_TRANSITION_DURATION;
    }
    if (!TiltVisualizer.Prefs.outroTransition && this._isExecutingDestruction) {
      this._time = OUTRO_TRANSITION_DURATION;
    }

    if (this._time >= INTRO_TRANSITION_DURATION &&
       !this._isInitializationFinished &&
       !this._isExecutingDestruction) {

      this._isInitializationFinished = true;
      Services.obs.notifyObservers(null, this.NOTIFICATIONS.INITIALIZED, null);

      if ("function" === typeof this._onInitializationFinished) {
        this._onInitializationFinished();
      }
    }

    if (this._time >= OUTRO_TRANSITION_DURATION &&
       !this._isDestructionFinished &&
        this._isExecutingDestruction) {

      this._isDestructionFinished = true;
      Services.obs.notifyObservers(null, this.NOTIFICATIONS.BEFORE_DESTROYED, null);

      if ("function" === typeof this._onDestructionFinished) {
        this._onDestructionFinished();
      }
    }
  },

  /**
   * Starts executing the destruction sequence and issues a callback function
   * when finished.
   *
   * @param {Function} aCallback
   *                   the destruction finished callback
   */
  executeDestruction: function TV_executeDestruction(aCallback)
  {
    if (!this._isExecutingDestruction) {
      this._isExecutingDestruction = true;
      this._onDestructionFinished = aCallback;

      // if we execute the destruction after the initialization finishes,
      // proceed normally; otherwise, skip everything and immediately issue
      // the callback

      if (this._time > OUTRO_TRANSITION_DURATION) {
        this._time = 0;
        this._redraw = true;
      } else {
        aCallback();
      }
    }
  },

  /**
   * Checks if this object was initialized properly.
   *
   * @return {Boolean} true if the object was initialized properly
   */
  isInitialized: function TVP_isInitialized()
  {
    return this._renderer && this._renderer.context;
  },

  /**
   * Function called when this object is destroyed.
   */
  _finalize: function TVP__finalize()
  {
    TiltUtils.destroyObject(this._visualizationProgram);
    TiltUtils.destroyObject(this._texture);

    if (this._meshStacks) {
      this._meshStacks.forEach(function(group) {
        TiltUtils.destroyObject(group.vertices);
        TiltUtils.destroyObject(group.texCoord);
        TiltUtils.destroyObject(group.color);
        TiltUtils.destroyObject(group.indices);
      });
    }
    if (this._meshWireframe) {
      this._meshWireframe.forEach(function(group) {
        TiltUtils.destroyObject(group.indices);
      });
    }

    TiltUtils.destroyObject(this._renderer);

    // Closing the tab would result in contentWindow being a dead object,
    // so operations like removing event listeners won't work anymore.
    if (this.contentWindow == this.chromeWindow.content) {
      this.contentWindow.removeEventListener("resize", this._onResize, false);
    }
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
  /**
   * A canvas overlay on which mouse and keyboard event listeners are attached.
   */
  this.canvas = aCanvas;

  /**
   * Save a reference to the presenter to modify its model-view transforms.
   */
  this.presenter = aPresenter;
  this.presenter.controller = this;

  /**
   * The initial controller dimensions and offset, in pixels.
   */
  this._zoom = aPresenter.transforms.zoom;
  this._left = (aPresenter.contentWindow.pageXOffset || 0) * this._zoom;
  this._top = (aPresenter.contentWindow.pageYOffset || 0) * this._zoom;
  this._width = aCanvas.width;
  this._height = aCanvas.height;

  /**
   * Arcball used to control the visualization using the mouse.
   */
  this.arcball = new TiltVisualizer.Arcball(
    this.presenter.chromeWindow, this._width, this._height, 0,
    [
      this._width + this._left < aPresenter._maxTextureSize ? -this._left : 0,
      this._height + this._top < aPresenter._maxTextureSize ? -this._top : 0
    ]);

  /**
   * Object containing the rotation quaternion and the translation amount.
   */
  this._coordinates = null;

  // bind the owner object to the necessary functions
  TiltUtils.bindObjectFunc(this, "_update");
  TiltUtils.bindObjectFunc(this, "^_on");

  // add the necessary event listeners
  this.addEventListeners();

  // attach this controller's update function to the presenter ondraw event
  this.presenter._controllerUpdate = this._update;
};

TiltVisualizer.Controller.prototype = {

  /**
   * Adds events listeners required by this controller.
   */
  addEventListeners: function TVC_addEventListeners()
  {
    let canvas = this.canvas;
    let presenter = this.presenter;

    // bind commonly used mouse and keyboard events with the controller
    canvas.addEventListener("mousedown", this._onMouseDown, false);
    canvas.addEventListener("mouseup", this._onMouseUp, false);
    canvas.addEventListener("mousemove", this._onMouseMove, false);
    canvas.addEventListener("mouseover", this._onMouseOver, false);
    canvas.addEventListener("mouseout", this._onMouseOut, false);
    canvas.addEventListener("MozMousePixelScroll", this._onMozScroll, false);
    canvas.addEventListener("keydown", this._onKeyDown, false);
    canvas.addEventListener("keyup", this._onKeyUp, false);
    canvas.addEventListener("keypress", this._onKeyPress, true);
    canvas.addEventListener("blur", this._onBlur, false);

    // handle resize events to change the arcball dimensions
    presenter.contentWindow.addEventListener("resize", this._onResize, false);
  },

  /**
   * Removes all added events listeners required by this controller.
   */
  removeEventListeners: function TVC_removeEventListeners()
  {
    let canvas = this.canvas;
    let presenter = this.presenter;

    canvas.removeEventListener("mousedown", this._onMouseDown, false);
    canvas.removeEventListener("mouseup", this._onMouseUp, false);
    canvas.removeEventListener("mousemove", this._onMouseMove, false);
    canvas.removeEventListener("mouseover", this._onMouseOver, false);
    canvas.removeEventListener("mouseout", this._onMouseOut, false);
    canvas.removeEventListener("MozMousePixelScroll", this._onMozScroll, false);
    canvas.removeEventListener("keydown", this._onKeyDown, false);
    canvas.removeEventListener("keyup", this._onKeyUp, false);
    canvas.removeEventListener("keypress", this._onKeyPress, true);
    canvas.removeEventListener("blur", this._onBlur, false);

    // Closing the tab would result in contentWindow being a dead object,
    // so operations like removing event listeners won't work anymore.
    if (presenter.contentWindow == presenter.chromeWindow.content) {
      presenter.contentWindow.removeEventListener("resize", this._onResize, false);
    }
  },

  /**
   * Function called each frame, updating the visualization camera transforms.
   *
   * @param {Number} aTime
   *                 total time passed since rendering started
   * @param {Number} aDelta
   *                 the current animation frame delta
   */
  _update: function TVC__update(aTime, aDelta)
  {
    this._time = aTime;
    this._coordinates = this.arcball.update(aDelta);

    this.presenter.setRotation(this._coordinates.rotation);
    this.presenter.setTranslation(this._coordinates.translation);
  },

  /**
   * Called once after every time a mouse button is pressed.
   */
  _onMouseDown: function TVC__onMouseDown(e)
  {
    e.target.focus();
    e.preventDefault();
    e.stopPropagation();

    if (this._time < MOUSE_INTRO_DELAY) {
      return;
    }

    // calculate x and y coordinates using using the client and target offset
    let button = e.which;
    this._downX = e.clientX - e.target.offsetLeft;
    this._downY = e.clientY - e.target.offsetTop;

    this.arcball.mouseDown(this._downX, this._downY, button);
  },

  /**
   * Called every time a mouse button is released.
   */
  _onMouseUp: function TVC__onMouseUp(e)
  {
    e.preventDefault();
    e.stopPropagation();

    if (this._time < MOUSE_INTRO_DELAY) {
      return;
    }

    // calculate x and y coordinates using using the client and target offset
    let button = e.which;
    let upX = e.clientX - e.target.offsetLeft;
    let upY = e.clientY - e.target.offsetTop;

    // a click in Tilt is issued only when the mouse pointer stays in
    // relatively the same position
    if (Math.abs(this._downX - upX) < MOUSE_CLICK_THRESHOLD &&
        Math.abs(this._downY - upY) < MOUSE_CLICK_THRESHOLD) {

      this.presenter.highlightNodeAt(upX, upY);
    }

    this.arcball.mouseUp(upX, upY, button);
  },

  /**
   * Called every time the mouse moves.
   */
  _onMouseMove: function TVC__onMouseMove(e)
  {
    e.preventDefault();
    e.stopPropagation();

    if (this._time < MOUSE_INTRO_DELAY) {
      return;
    }

    // calculate x and y coordinates using using the client and target offset
    let moveX = e.clientX - e.target.offsetLeft;
    let moveY = e.clientY - e.target.offsetTop;

    this.arcball.mouseMove(moveX, moveY);
  },

  /**
   * Called when the mouse leaves the visualization bounds.
   */
  _onMouseOver: function TVC__onMouseOver(e)
  {
    e.preventDefault();
    e.stopPropagation();

    this.arcball.mouseOver();
  },

  /**
   * Called when the mouse leaves the visualization bounds.
   */
  _onMouseOut: function TVC__onMouseOut(e)
  {
    e.preventDefault();
    e.stopPropagation();

    this.arcball.mouseOut();
  },

  /**
   * Called when the mouse wheel is used.
   */
  _onMozScroll: function TVC__onMozScroll(e)
  {
    e.preventDefault();
    e.stopPropagation();

    this.arcball.zoom(e.detail);
  },

  /**
   * Called when a key is pressed.
   */
  _onKeyDown: function TVC__onKeyDown(e)
  {
    let code = e.keyCode || e.which;

    if (!e.altKey && !e.ctrlKey && !e.metaKey && !e.shiftKey) {
      e.preventDefault();
      e.stopPropagation();
      this.arcball.keyDown(code);
    } else {
      this.arcball.cancelKeyEvents();
    }
  },

  /**
   * Called when a key is released.
   */
  _onKeyUp: function TVC__onKeyUp(e)
  {
    let code = e.keyCode || e.which;

    if (code === e.DOM_VK_X) {
      this.presenter.deleteNode();
    }
    if (code === e.DOM_VK_F) {
      let highlight = this.presenter._highlight;
      let zoom = this.presenter.transforms.zoom;

      this.arcball.moveIntoView(vec3.lerp(
        vec3.scale(highlight.v0, zoom, []),
        vec3.scale(highlight.v1, zoom, []), 0.5));
    }
    if (!e.altKey && !e.ctrlKey && !e.metaKey && !e.shiftKey) {
      e.preventDefault();
      e.stopPropagation();
      this.arcball.keyUp(code);
    }
  },

  /**
   * Called when a key is pressed.
   */
  _onKeyPress: function TVC__onKeyPress(e)
  {
    if (e.keyCode === e.DOM_VK_ESCAPE) {
      let mod = {};
      Cu.import("resource:///modules/devtools/Tilt.jsm", mod);
      let tilt =
        mod.TiltManager.getTiltForBrowser(this.presenter.chromeWindow);
      e.preventDefault();
      e.stopPropagation();
      tilt.destroy(tilt.currentWindowId, true);
    }
  },

  /**
   * Called when the canvas looses focus.
   */
  _onBlur: function TVC__onBlur(e) {
    this.arcball.cancelKeyEvents();
  },

  /**
   * Called when the content window of the current browser is resized.
   */
  _onResize: function TVC__onResize(e)
  {
    let zoom = this.presenter._getPageZoom();
    let width = e.target.innerWidth * zoom;
    let height = e.target.innerHeight * zoom;

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
  _finalize: function TVC__finalize()
  {
    TiltUtils.destroyObject(this.arcball);
    TiltUtils.destroyObject(this._coordinates);

    this.removeEventListeners();
    this.presenter.controller = null;
    this.presenter._controllerUpdate = null;
  }
};

/**
 * This is a general purpose 3D rotation controller described by Ken Shoemake
 * in the Graphics Interface ’92 Proceedings. It features good behavior
 * easy implementation, cheap execution.
 *
 * @param {Window} aChromeWindow
 *                 a reference to the top-level window
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
  aChromeWindow, aWidth, aHeight, aRadius, aInitialTrans, aInitialRot)
{
  /**
   * Save a reference to the top-level window to set/remove intervals.
   */
  this.chromeWindow = aChromeWindow;

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
  this._additionalRot = vec3.create();
  this._additionalTrans = vec3.create();
  this._deltaAdditionalRot = quat4.create();
  this._deltaAdditionalTrans = vec3.create();

  // load the keys controlling the arcball
  this._loadKeys();

  // set the current dimensions of the arcball
  this.resize(aWidth, aHeight, aRadius);
};

TiltVisualizer.Arcball.prototype = {

  /**
   * Call this function whenever you need the updated rotation quaternion
   * and the zoom amount. These values will be returned as "rotation" and
   * "translation" properties inside an object.
   *
   * @param {Number} aDelta
   *                 the current animation frame delta
   *
   * @return {Object} the rotation quaternion and the translation amount
   */
  update: function TVA_update(aDelta)
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
      this._pointToSphere(x, y, this.width, this.height, this.radius, endVec);

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

    let additionalRot = this._additionalRot;
    let additionalTrans = this._additionalTrans;
    let deltaAdditionalRot = this._deltaAdditionalRot;
    let deltaAdditionalTrans = this._deltaAdditionalTrans;

    let rotateKeys = this.rotateKeys;
    let panKeys = this.panKeys;
    let zoomKeys = this.zoomKeys;
    let resetKey = this.resetKey;

    // handle additional rotation and translation by the keyboard
    if (keyCode[rotateKeys.left]) {
      additionalRot[0] -= ARCBALL_SENSITIVITY * ARCBALL_ROTATION_STEP;
    }
    if (keyCode[rotateKeys.right]) {
      additionalRot[0] += ARCBALL_SENSITIVITY * ARCBALL_ROTATION_STEP;
    }
    if (keyCode[rotateKeys.up]) {
      additionalRot[1] += ARCBALL_SENSITIVITY * ARCBALL_ROTATION_STEP;
    }
    if (keyCode[rotateKeys.down]) {
      additionalRot[1] -= ARCBALL_SENSITIVITY * ARCBALL_ROTATION_STEP;
    }
    if (keyCode[panKeys.left]) {
      additionalTrans[0] -= ARCBALL_SENSITIVITY * ARCBALL_TRANSLATION_STEP;
    }
    if (keyCode[panKeys.right]) {
      additionalTrans[0] += ARCBALL_SENSITIVITY * ARCBALL_TRANSLATION_STEP;
    }
    if (keyCode[panKeys.up]) {
      additionalTrans[1] -= ARCBALL_SENSITIVITY * ARCBALL_TRANSLATION_STEP;
    }
    if (keyCode[panKeys.down]) {
      additionalTrans[1] += ARCBALL_SENSITIVITY * ARCBALL_TRANSLATION_STEP;
    }
    if (keyCode[zoomKeys["in"][0]] ||
        keyCode[zoomKeys["in"][1]] ||
        keyCode[zoomKeys["in"][2]]) {
      this.zoom(-ARCBALL_TRANSLATION_STEP);
    }
    if (keyCode[zoomKeys["out"][0]] ||
        keyCode[zoomKeys["out"][1]]) {
      this.zoom(ARCBALL_TRANSLATION_STEP);
    }
    if (keyCode[zoomKeys["unzoom"]]) {
      this._zoomAmount = 0;
    }
    if (keyCode[resetKey]) {
      this.reset();
    }

    // update the delta key rotations and translations
    deltaAdditionalRot[0] +=
      (additionalRot[0] - deltaAdditionalRot[0]) * ARCBALL_SENSITIVITY;
    deltaAdditionalRot[1] +=
      (additionalRot[1] - deltaAdditionalRot[1]) * ARCBALL_SENSITIVITY;
    deltaAdditionalRot[2] +=
      (additionalRot[2] - deltaAdditionalRot[2]) * ARCBALL_SENSITIVITY;

    deltaAdditionalTrans[0] +=
      (additionalTrans[0] - deltaAdditionalTrans[0]) * ARCBALL_SENSITIVITY;
    deltaAdditionalTrans[1] +=
      (additionalTrans[1] - deltaAdditionalTrans[1]) * ARCBALL_SENSITIVITY;

    // create an additional rotation based on the key events
    quat4.fromEuler(
      deltaAdditionalRot[0],
      deltaAdditionalRot[1],
      deltaAdditionalRot[2], deltaRot);

    // create an additional translation based on the key events
    vec3.set([deltaAdditionalTrans[0], deltaAdditionalTrans[1], 0], deltaTrans);

    // handle the reset animation steps if necessary
    if (this._resetInProgress) {
      this._nextResetStep(aDelta || 1);
    }

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
    this._cancelReset();
    this._save();

    // find the sphere coordinates of the mouse positions
    this._pointToSphere(
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
    this._cancelReset();
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
    this._cancelReset();
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
  _pointToSphere: function TVA__pointToSphere(
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
   * Cancels all pending transformations caused by key events.
   */
  cancelKeyEvents: function TVA_cancelKeyEvents()
  {
    this._keyCode = {};
  },

  /**
   * Cancels all pending transformations caused by mouse events.
   */
  cancelMouseEvents: function TVA_cancelMouseEvents()
  {
    this._rotating = false;
    this._mouseButton = -1;
  },

  /**
   * Incremental translation method.
   *
   * @param {Array} aTranslation
   *                the translation ammount on the [x, y] axis
   */
  translate: function TVP_translate(aTranslation)
  {
    this._additionalTrans[0] += aTranslation[0];
    this._additionalTrans[1] += aTranslation[1];
  },

  /**
   * Incremental rotation method.
   *
   * @param {Array} aRotation
   *                the rotation ammount along the [x, y, z] axis
   */
  rotate: function TVP_rotate(aRotation)
  {
    // explicitly rotate along y, x, z values because they're eulerian angles
    this._additionalRot[0] += TiltMath.radians(aRotation[1]);
    this._additionalRot[1] += TiltMath.radians(aRotation[0]);
    this._additionalRot[2] += TiltMath.radians(aRotation[2]);
  },

  /**
   * Moves a target point into view only if it's outside the currently visible
   * area bounds (in which case it also resets any additional transforms).
   *
   * @param {Arary} aPoint
   *                the [x, y] point which should be brought into view
   */
  moveIntoView: function TVA_moveIntoView(aPoint) {
    let visiblePointX = -(this._currentTrans[0] + this._additionalTrans[0]);
    let visiblePointY = -(this._currentTrans[1] + this._additionalTrans[1]);

    if (aPoint[1] - visiblePointY - MOVE_INTO_VIEW_ACCURACY > this.height ||
        aPoint[1] - visiblePointY + MOVE_INTO_VIEW_ACCURACY < 0 ||
        aPoint[0] - visiblePointX > this.width ||
        aPoint[0] - visiblePointX < 0) {
      this.reset([0, -aPoint[1]]);
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
   * Starts an animation resetting the arcball transformations to identity.
   *
   * @param {Array} aFinalTranslation
   *                optional, final vector translation
   * @param {Array} aFinalRotation
   *                optional, final quaternion rotation
   */
  reset: function TVA_reset(aFinalTranslation, aFinalRotation)
  {
    if ("function" === typeof this._onResetStart) {
      this._onResetStart();
      this._onResetStart = null;
    }

    this.cancelMouseEvents();
    this.cancelKeyEvents();
    this._cancelReset();

    this._save();
    this._resetFinalTranslation = vec3.create(aFinalTranslation);
    this._resetFinalRotation = quat4.create(aFinalRotation);
    this._resetInProgress = true;
  },

  /**
   * Cancels the current arcball reset animation if there is one.
   */
  _cancelReset: function TVA__cancelReset()
  {
    if (this._resetInProgress) {
      this._resetInProgress = false;
      this._save();

      if ("function" === typeof this._onResetFinish) {
        this._onResetFinish();
        this._onResetFinish = null;
        this._onResetStep = null;
      }
    }
  },

  /**
   * Executes the next step in the arcball reset animation.
   *
   * @param {Number} aDelta
   *                 the current animation frame delta
   */
  _nextResetStep: function TVA__nextResetStep(aDelta)
  {
    // a very large animation frame delta (in case of seriously low framerate)
    // would cause all the interpolations to become highly unstable
    aDelta = TiltMath.clamp(aDelta, 1, 100);

    let fNearZero = EPSILON * EPSILON;
    let fInterpLin = ARCBALL_RESET_LINEAR_FACTOR * aDelta;
    let fInterpSph = ARCBALL_RESET_SPHERICAL_FACTOR;
    let fTran = this._resetFinalTranslation;
    let fRot = this._resetFinalRotation;

    let t = vec3.create(fTran);
    let r = quat4.multiply(quat4.inverse(quat4.create(this._currentRot)), fRot);

    // reset the rotation quaternion and translation vector
    vec3.lerp(this._currentTrans, t, fInterpLin);
    quat4.slerp(this._currentRot, r, fInterpSph);

    // also reset any additional transforms by the keyboard or mouse
    vec3.scale(this._additionalTrans, fInterpLin);
    vec3.scale(this._additionalRot, fInterpLin);
    this._zoomAmount *= fInterpLin;

    // clear the loop if the all values are very close to zero
    if (vec3.length(vec3.subtract(this._lastRot, fRot, [])) < fNearZero &&
        vec3.length(vec3.subtract(this._deltaRot, fRot, [])) < fNearZero &&
        vec3.length(vec3.subtract(this._currentRot, fRot, [])) < fNearZero &&
        vec3.length(vec3.subtract(this._lastTrans, fTran, [])) < fNearZero &&
        vec3.length(vec3.subtract(this._deltaTrans, fTran, [])) < fNearZero &&
        vec3.length(vec3.subtract(this._currentTrans, fTran, [])) < fNearZero &&
        vec3.length(this._additionalRot) < fNearZero &&
        vec3.length(this._additionalTrans) < fNearZero) {

      this._cancelReset();
    }

    if ("function" === typeof this._onResetStep) {
      this._onResetStep();
    }
  },

  /**
   * Loads the keys to control this arcball.
   */
  _loadKeys: function TVA__loadKeys()
  {
    this.rotateKeys = {
      "up": Ci.nsIDOMKeyEvent["DOM_VK_W"],
      "down": Ci.nsIDOMKeyEvent["DOM_VK_S"],
      "left": Ci.nsIDOMKeyEvent["DOM_VK_A"],
      "right": Ci.nsIDOMKeyEvent["DOM_VK_D"],
    };
    this.panKeys = {
      "up": Ci.nsIDOMKeyEvent["DOM_VK_UP"],
      "down": Ci.nsIDOMKeyEvent["DOM_VK_DOWN"],
      "left": Ci.nsIDOMKeyEvent["DOM_VK_LEFT"],
      "right": Ci.nsIDOMKeyEvent["DOM_VK_RIGHT"],
    };
    this.zoomKeys = {
      "in": [
        Ci.nsIDOMKeyEvent["DOM_VK_I"],
        Ci.nsIDOMKeyEvent["DOM_VK_ADD"],
        Ci.nsIDOMKeyEvent["DOM_VK_EQUALS"],
      ],
      "out": [
        Ci.nsIDOMKeyEvent["DOM_VK_O"],
        Ci.nsIDOMKeyEvent["DOM_VK_SUBTRACT"],
      ],
      "unzoom": Ci.nsIDOMKeyEvent["DOM_VK_0"]
    };
    this.resetKey = Ci.nsIDOMKeyEvent["DOM_VK_R"];
  },

  /**
   * Saves the current arcball state, typically after resize or mouse events.
   */
  _save: function TVA__save()
  {
    if (this._mousePress) {
      let x = this._mousePress[0];
      let y = this._mousePress[1];

      this._mouseMove[0] = x;
      this._mouseMove[1] = y;
      this._mouseRelease[0] = x;
      this._mouseRelease[1] = y;
      this._mouseLerp[0] = x;
      this._mouseLerp[1] = y;
    }
  },

  /**
   * Function called when this object is destroyed.
   */
  _finalize: function TVA__finalize()
  {
    this._cancelReset();
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

  get introTransition()
  {
    return this._introTransition;
  },

  set introTransition(value)
  {
    TiltUtils.Preferences.set("intro_transition", "boolean", value);
    this._introTransition = value;
  },

  get outroTransition()
  {
    return this._outroTransition;
  },

  set outroTransition(value)
  {
    TiltUtils.Preferences.set("outro_transition", "boolean", value);
    this._outroTransition = value;
  },

  /**
   * Loads the preferences.
   */
  load: function TVC_load()
  {
    let prefs = TiltVisualizer.Prefs;
    let get = TiltUtils.Preferences.get;

    prefs._enabled = get("enabled", "boolean");
    prefs._introTransition = get("intro_transition", "boolean");
    prefs._outroTransition = get("outro_transition", "boolean");
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
