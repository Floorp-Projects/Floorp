============
DevTools API
============

.. warning::
  Deprecated: This feature is no longer recommended. Though some browsers might still support it, it may have already been removed from the relevant web standards, may be in the process of being dropped, or may only be kept for compatibility purposes. Avoid using it, and update existing code if possible; see the compatibility table at the bottom of this page to guide your decision. Be aware that this feature may cease to work at any time.

.. warning::
  The DevTools API is still WIP. If you notice any inconsistency, please let The Firefox Developer Tools Team know.


While this api is currently a work-in-progress, there are usable portions of :doc:`page inspector <../page_inspector/index>` and :doc:`debugger <../debugger/index>` that may be used currently.


Introduction
************

The DevTools API provides a way to register and access developer tools in Firefox.

In terms of User Interface, each registered tool lives in its own tab (we call one tab a **panel**). These tabs are located in a box we call a **Toolbox**. A toolbox can be *hosted* within a browser tab (at the bottom or on the side), or in its own window (we say that the toolbox is *undocked*). A Toolbox (and all the tools it contains) is linked to a **Target**, which is the object the tools are debugging. A target is usually a web page (a tab), but can be other things (a chrome window, a remote tab,…).

In terms of code, each tool has to provide a **ToolDefinition** object. A definition is a JS light object that exposes different information about the tool (like its name and its icon), and a *build* method that will be used later-on to start an instance of this tool. The **gDevTools** global object provides methods to register a tool definition and to access tool instances. An instance of a tool is called a **ToolPanel**. The ToolPanel is built only when the tool is selected (not when the toolbox is opened). There is no way to "close/destroy" a ToolPanel. The only way to close a toolPanel is to close its containing toolbox. All these objects implement the **EventEmitter** interface.


API
***

gDevTools
---------

The ``gDevTools`` API can be used to register new tools, themes and handle toolboxes for different tabs and windows. To use the ``gDevTools`` API from an add-on, it can be imported with following snippet

.. code-block:: JavaScript

  const { gDevTools } = require("resource:///modules/devtools/gDevTools.jsm");


Methods
~~~~~~~

``registerTool(toolDefinition)``
  Registers a new tool and adds a tab to each existing toolbox.

  **Parameters:**

  - ``toolDefinition {ToolDefinition}`` - An object that contains information about the tool. See :ref:`ToolDefinition <devtoolsapi-tool-definition>` for details.

``unregisterTool(tool)``
  Unregisters the given tool and removes it from all toolboxes.

  **Parameters:**

  - ``tool {ToolDefinition|String}`` - The tool definition object or the id of the tool to unregister.

``registerTheme(themeDefinition)``
  Registers a new theme.

  **Parameters:**

  - ``themeDefinition {ThemeDefinition}`` - An object that contains information about the theme.

``unregisterTheme(theme)``
  Unregisters the given theme.

  **Parameters:**

  - ``theme {ThemeDefinition|String}`` - The theme definition object or the theme identifier.

``showToolbox(commands[, toolId [, hostType [, hostOptions]]])``
  Opens a toolbox for given target either by creating a new one or activating an existing one.

  **Parameters:**

  - ``commands {Object}`` - The commands object designating which debugging context the toolbox will debug.
  - ``toolId {String}`` - The tool that should be activated. If unspecified the previously active tool is shown.
  - ``hostType {String}`` - The position the toolbox will be placed. One of ``bottom``, ``side``, ``window``, ``custom``. See :ref:`HostType <devtoolsapi-host-type>` for details.
  - ``hostOptions {Object}`` - An options object passed to the selected host. See :ref:`HostType <devtoolsapi-host-type>` for details.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is fulfilled with the :ref:`Toolbox <devtoolsapi-toolbox>` instance once it has been initialized and the selected tool is loaded.

``getToolbox(target)``
  Fetch the :ref:`Toolbox <devtoolsapi-toolbox>` object for the given target.

  **Parameters:**

  - ``target {Target}`` - The target the toolbox is debugging.

  **Return value:**
  :ref:`Toolbox <devtoolsapi-toolbox>` object or undefined if there's no toolbox for the given target..

``closeToolbox(target)``
  Closes the toolbox for given target.

  **Parameters:**

  - ``target {Target}`` - The target of the toolbox that should be closed.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is fulfilled once the toolbox has been destroyed.

``getDefaultTools()``
  Returns an `Array <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array>`_ of :ref:`ToolDefinition <devtoolsapi-tool-definition>` objects for the built-in tools.

``getAdditionalTools()``
  Returns an `Array <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array>`_ of :ref:`ToolDefinition <devtoolsapi-tool-definition>` objects for tools added by addons.

``getToolDefinition(toolId)``
  Fetch the :ref:`ToolDefinition <devtoolsapi-tool-definition>` object for a tool if it exists and is enabled.

  **Parameters:**

  - ``toolId {String}`` - The ID of the tool.

  **Return value:**
  A :ref:`ToolDefinition <devtoolsapi-tool-definition>` if a tool with the given ID exists and is enabled, null otherwise.

``getToolDefinitionMap()``
  Returns a toolId → :ref:`ToolDefinition <devtoolsapi-tool-definition>` map for tools that are enabled.

``getToolDefinitionArray()``
  Returns an `Array <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array>`_ of :ref:`ToolDefinition <devtoolsapi-tool-definition>` objects for enabled tools sorted by the order they appear in the toolbox.

``getThemeDefinition(themeId)``
  Fetch the ``ThemeDefinition`` object for the theme with the given id.

  **Parameters:**

  - ``themeId {String}`` - The ID of the theme.

  **Return value:**
  A ``ThemeDefinition`` object if the theme exists, null otherwise.

``getThemeDefinitionMap()``
  Returns a toolId → ``ThemeDefinition`` map for available themes.

``getThemeDefinitionArray()``
  Returns an `Array <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array>`_ of ``ThemeDefinition`` objects for available themes.


Events
~~~~~~

Following events are emitted by the ``gDevTools`` object via the :ref:`EventEmitter <devtoolsapi-event-emitter>` interface.


``tool-registered (toolId)``
  A new tool has been registered.

``tool-unregistered(tool)``
  A tool has been unregistered. The parameter is a :ref:`ToolDefinition <devtoolsapi-tool-definition>` object.

``theme-registered(themeId)``
  A new theme has been registered.

``theme-unregistered(theme)``
  A theme has been unregistered. The parameter is a ``ThemeDefinition`` object.

``toolbox-ready(toolbox)``
  A new toolbox has been created and is ready to use. The parameter is a :ref:`Toolbox <devtoolsapi-toolbox>` object instance.

``toolbox-destroy(target)``
  The toolbox for the specified target is about to be destroyed.

``toolbox-destroyed(target)``
  The toolbox for the specified target has been destroyed.

``{toolId}-init(toolbox, iframe)``
  A tool with the given ID has began to load in the given toolbox to the given frame.

``{toolId}-build(toolbox, panel)``
  A tool with the given ID has began to initialize in the given toolbox. The panel is the object returned by the ``ToolDefinition.build()`` method.

``{toolId}-ready(toolbox, panel)``
  A tool with the given ID has finished its initialization and is ready to be used. The panel is the object returned by the ``ToolDefinition.build()`` method.

``{toolId}-destroy(toolbox, panel)``
  A tool with the given ID is about to be destroyed. The panel is the object returned by the ``ToolDefinition.build()`` method.


.. _devtoolsapi-toolbox:

Toolbox
-------

A Toolbox is a frame for the :ref:`ToolPanel <devtoolsapi-tool-panel>` that is debugging a specific target.


Properties
~~~~~~~~~~


``target``
  **Target**. The Target this toolbox is debugging.


``hostType``
  **Toolbox.HostType**. The type of the host this Toolbox is docked to. The value is one of the ``Toolbox.HostType`` constants.

``zoomValue``
  The current zoom level of the Toolbox.


Constants
~~~~~~~~~

The Toolbox constructor contains following constant properties.


``Toolbox.HostType.BOTTOM``
  Host type for the default toolbox host at the bottom of the browser window.

``Toolbox.HostType.SIDE``
  Host type for the host at the side of the browser window.

``Toolbox.HostType.WINDOW``
  Host type for the separate Toolbox window.

``Toolbox.HostType.CUSTOM``
  Host type for a custom frame host.


Methods
~~~~~~~

``getCurrentPanel()``
  Get the currently active :ref:`ToolPanel <devtoolsapi-tool-panel>`.

  **Return value:**
  The :ref:`ToolPanel <devtoolsapi-tool-panel>` object that was returned from ``ToolPanel.build()``.

``getPanel(toolId)``
  Get the :ref:`ToolPanel <devtoolsapi-tool-panel>` for given tool.

  **Parameters:**

  - ``toolId {String}`` - The tool identifier.

  **Return value:**
  The :ref:`ToolPanel <devtoolsapi-tool-panel>` object if the tool with the given ``toolId`` is active, otherwise ``undefined``.

``getPanelWhenReady(toolId)``
  Similar to ``getPanel()`` but waits for the tool to load first. If the tool is not already loaded or currently loading the returned `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ won't be fulfilled until something triggers the tool to load.

  **Parameters:**

  - ``toolId {String}`` - The tool identifier.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is fulfilled with the :ref:`ToolPanel <devtoolsapi-tool-panel>` object once the tool has finished loading.

``getToolPanels()``
  Returns a ``toolId`` → :ref:`ToolPanel <devtoolsapi-tool-panel>` `Map <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map>`_ for currently loaded tools.

``getNotificationBox()``
  Returns a ``XULElem("notificationbox")`` element for the Toolbox that can be used to display notifications to the user.

``loadTool(toolId)``
  Loads the tool with the given ``toolId`` in the background but does not activate it.

  **Parameters:**

  - ``toolId {String}`` - The tool identifier.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is fulfilled with the :ref:`ToolPanel <devtoolsapi-tool-panel>` object of the loaded panel once the tool has loaded.

``selectTool(toolId)``
  Selects the tool with the given ``toolId``.

  **Parameters:**

  - ``toolId {String}`` - The tool identifier.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is fulfilled with the :ref:`ToolPanel <devtoolsapi-tool-panel>` object of the selected panel once the tool has loaded and activated.

``selectNextTool()``
  Selects the next tool in the ``Toolbox``.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is fulfilled with the :ref:`ToolPanel <devtoolsapi-tool-panel>` object of the selected panel.

``selectPreviousTool()``
  Selects the previous tool in the ``Toolbox``.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is fulfilled with the :ref:`ToolPanel <devtoolsapi-tool-panel>` object of the selected panel.

``highlightTool(toolId)``
  Highlights the tab for the given tool.

  **Parameters:**

  - ``toolId {String}`` - The tool to highlight.

``unhighlightTool(toolId)``
  Unhighlights the tab for the given tool.

  **Parameters:**

  - ``toolId {String}`` - The tool to unhighlight.

``openSplitConsole()``
  Opens the split Console to the bottom of the toolbox.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is fulfilled once the Console has loaded.

``closeSplitConsole()``
  Closes the split console.

``toggleSplitConsole()``
  Toggles the state of the split console.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is fulfilled once the operation has finished.

``switchHost(hostType)``
  Switches the location of the toolbox

  **Parameters:**

  - ``hostType {Toolbox.HostType}`` - The type of the new host.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is fulfilled once the new host is ready.

``reloadTarget(force)``
  Reloads the current target of the toolbox.

  **Parameters:**

  - ``force {Boolean} -`` If true the target is shift-reloaded i.e. the cache is bypassed during the reload.

``zoomIn()``
  Increases the zoom level of the ``Toolbox`` document.

``zoomOut()``
  Decreases the zoom level of the ``Toolbox`` document.

``zoomReset()``
  Resets the zoom level of the ``Toolbox`` document.

``setZoom(value)``
  Set the zoom level to an arbitrary value.

  **Parameters:**

  - ``value {Number}`` - The zoom level such as ``1.2``.

``destroy()``
  Closes the toolbox.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is resolved once the ``Toolbox`` is destroyed.


Events
~~~~~~

The Toolbox object emits following events via the :ref:`EventEmitter <devtoolsapi-event-emitter>` interface.


``host-changed``
  The Host for this Toolbox has changed.

``ready``
  The ``Toolbox`` is ready to use.

``select(toolId)``
  A tool has been selected. This event is emitted before the corresponding ``{toolId}-selected`` event.

``{toolId}-init(frame)``
  A tool is about to be loaded. The frame is the `iframe <https://developer.mozilla.org/en-US/docs/Web/HTML/Element/iframe>`_ element that has been created for the tool.

``{toolId}-build(panel)``
  The frame for a tool has loaded and the ``ToolPanel.build()`` method has been called but the asynchronous initialization has not started. The parameter is a :ref:`ToolPanel <devtoolsapi-tool-panel>` object.

``{toolId}-ready(panel)``
  The asynchronous initialization for a tool has completed and it is ready to be used. The parameter is a :ref:`ToolPanel <devtoolsapi-tool-panel>` object.

``{toolId}-selected(panel)``
  A tool has been selected. The parameter is a :ref:`ToolPanel <devtoolsapi-tool-panel>` object.

``{toolId}-destroy(panel)``
  A tool is about to be destroyed. The parameter is a :ref:`ToolPanel <devtoolsapi-tool-panel>` object.

``destroy``
  The ``Toolbox`` is about to be destroyed.

``destroyed``
  The ``Toolbox`` has been destroyed.


.. _devtoolsapi-tool-definition:

ToolDefinition
--------------

A ``ToolDefinition`` object contains all the required information for a tool to be shown in the toolbox.


Methods
~~~~~~~

``isToolSupported(toolbox)``
  A method that is called during toolbox construction to check if the tool supports debugging the given target of the given toolbox.

  **Parameters:**

  - ``toolbox {Toolbox}`` - The toolbox where the tool is going to be displayed, if supported.

  **Return value:**
  A boolean indicating if the tool supports the given toolbox's target.

``build(window, toolbox)``
  A method that builds the :ref:`ToolPanel <devtoolsapi-tool-panel>` for this tool.

  **Parameters:**

  - ``window {Window}`` - The `Window <https://developer.mozilla.org/en-US/docs/Web/API/Window>`_ object for frame the tool is being built into.
  - ``toolbox {Toolbox}`` - The :ref:`Toolbox <devtoolsapi-toolbox>` the tool is being built for.

  **Return value:**
  A :ref:`ToolPanel <devtoolsapi-tool-panel>` for the tool.


``onKey(panel, toolbox)``
  **Optional.** A method that is called when the keyboard shortcut for the tool is activated while the tool is the active tool.

  **Parameters:**

  - ``panel {ToolPanel}`` - The :ref:`ToolPanel <devtoolsapi-tool-panel>` for the tool.
  - ``toolbox {Toolbox}`` - The toolbox for the shortcut was triggered for.

  **Return value:**
  Undefined.


Properties
~~~~~~~~~~

The ToolDefinition object can contain following properties. Most of them are optional and can be used to customize the presence of the tool in the Browser and the Toolbox.


``id``
  **String, required.** An unique identifier for the tool. It must be a valid id for an HTML `Element <https://developer.mozilla.org/en-US/docs/Web/API/Element>`_.

``url``
  **String, required.** An URL of the panel document.

``label``
  **String, optional.** The tool's name. If undefined the ``icon`` should be specified.

``tooltip``
  **String, optional.** The tooltip for the tool's tab.

``panelLabel``
  **String, optional.** An accessibility label for the panel.

``ordinal``
  **Integer, optional.** The position of the tool's tab within the toolbox. **Default:** 99

``visibilityswitch``
  **String, optional.** A preference name that controls the visibility of the tool. **Default:** ``devtools.{id}.enabled``

``icon``
  **String, optional.** An URL for the icon to show in the toolbox tab. If undefined the label should be defined.

``highlightedicon``
  **String, optional.** An URL for an icon that is to be used when the tool is highlighted (see e.g. paused, inactive debugger). **Default:** ``{icon}``

``iconOnly``
  **Boolean, optional.** If true, the label won't be shown in the tool's tab. **Default:** false

``invertIconForLightTheme``
  **Boolean, optional.** If true the colors of the icon will be inverted for the light theme. **Default:** false

``key``
  **String, optional.** The key used for keyboard shortcut. Either ``key`` or ``keycode`` value.

``modifiers``
  **String, optional.** ``modifiers`` for the keyboard shortcut.

``preventClosingOnKey``
  **Boolean, optional.** If true the tool won't close if its keybinding is pressed while it is active. **Default:** false

``inMenu``
  **Boolean, optional.** If true the tool will be shown in the Developer Menu. **Default:** false

``menuLabel``
  **String, optional.** A label for the Developer Menu item. **Default:** ``{label}``

``accesskey``
  **String, optional.** ``accesskey`` for the Developer Menu ``xul:menuitem``.


Example
~~~~~~~

Here's a minimal definition for a tool.

.. code-block:: JavaScript

  let def = {
    id: "my-tool",
    label: "My Tool",
    icon: "chrome://browser/skin/devtools/tool-webconsole.svg",
    url: "about:blank",
    isToolSupported: toolbox => true,
    build: (window, toolbox) => new MyToolPanel(window, toolbox)
  };

  // Register it.
  gDevTools.registerTool(def);


.. _devtoolsapi-target-type:

TargetType
----------

FIXME:


.. _devtoolsapi-host-type:

HostType
--------

FIXME


.. _devtoolsapi-tool-panel:

ToolPanel
---------

The ToolPanel is an interface the toolbox uses to manage the panel of a tool. The object that ``ToolDefinition.build()`` returns should implement the methods described below.

Methods
~~~~~~~


``open()``
  **Optional**. A method that can be used to perform asynchronous initialization. If the method returns a `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_, many operations (e.g. ``gDevTools.showToolbox()`` or ``toolbox.selectTool()``) and events (e.g. ``toolbox-ready`` are delayed until the promise has been fulfilled.

  **Return value:**
  The method should return a `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is resolved with the ``ToolPanel`` object once it's ready to be used.

``destroy()``
  A method that is called when the toolbox is closed or the tool is unregistered. If the tool needs to perform asynchronous operations during destruction the method should return a `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is resolved once the process is complete.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ if the function performs asynchronous operations, otherwise ``undefined``.


Example
~~~~~~~

Here's a basic template for a ToolPanel implementation.

.. code-block:: JavaScript

  // In the ToolDefinition object, do
  //   build: (window, target) => new MyPanel(window, target),

  function MyPanel(window, target) {
    // The window object that has loaded the URL defined in the ToolDefinition
    this.window = window;
    // The Target this toolbox is debugging.
    this.target = target;

    // Do synchronous initialization here.
    window.document.body.addEventListener("click", this.handleClick);
  }

  MyPanel.prototype = {
    open: function() {
      // Any asynchronous operations should be done here.
      return this.doSomethingAsynchronous()
        .then(() => this);
    },

    destroy: function() {
      // Synchronous destruction.
      this.window.document.body.removeEventListener("click", this.handleClick);

      // Async destruction.
      return this.destroySomethingAsynchronously()
        .then(() => console.log("destroyed"));
    },

    handleClick: function(event) {
      console.log("Clicked", event.originalTarget);
    },
  };


.. _devtoolsapi-event-emitter:

EventEmitter
------------

``EventEmitter`` is an interface many Developer Tool classes and objects implement and use to notify others about changes in their internal state.

When an event is emitted on the ``EventEmitter``, the listeners will be called with the event name as the first argument and the extra arguments are spread as the remaining parameters.

.. note::
  Some components use Add-on SDK event module instead of the DevTools EventEmitter. Unfortunately, their API's are a bit different and it's not always evident which one a certain component is using. The main differences between the two modules are that the first parameter for Add-on SDK events is the first payload argument instead of the event name and the ``once`` method does not return a Promise. The work for unifying the event paradigms is ongoing in `bug 952653 <https://bugzilla.mozilla.org/show_bug.cgi?id=952653>`_.


Methods
~~~~~~~

The following methods are available on objects that have been decorated with the ``EventEmitter`` interface.

``emit(eventName, ...extraArguments)``
  Emits an event with the given name to this object.

  **Parameters:**

  - ``eventName {String}`` - The name of the event.
  - ``extraArguments {...Any}`` - Extra arguments that are passed to the listeners.

``on(eventName, listener)``
  Adds a listener for the given event.

``off(eventName, listener)``
  Removes the previously added listener from the event.

``once(eventName, listener)``
  Adds a listener for the event that is removed after it has been emitted once.

  **Return value:**
  A `Promise <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise>`_ that is fulfilled with the first extra argument for the event when then event is emitted. If the event contains multiple payload arguments, the rest are discarded and can only be received by providing the listener function to this method.


Examples
~~~~~~~~

Here's a few examples using the ``gDevTools`` object.

.. code-block:: JavaScript

  let onInit = (eventName, toolbox, netmonitor) => console.log("Netmonitor initialized!");

  // Attach a listener.
  gDevTools.on("netmonitor-init", onInit);

  // Remove a listener.
  gDevTools.off("netmonitor-init", onInit);

  // Attach a one time listener.
  gDevTools.once("netmonitor-init", (eventName, toolbox, netmonitor) => {
    console.log("Network Monitor initialized once!", toolbox, netmonitor);
  });

  // Use the Promise returned by the once method.
  gDevTools.once("netmonitor-init").then(toolbox => {
    // Note that the second argument is not available here.
    console.log("Network Monitor initialized to toolbox", toolbox);
  });


ToolSidebar
-----------

To build a sidebar in your tool, first, add a xul:tabbox where you want the sidebar to live:

.. code-block:: xml

  <splitter class="devtools-side-splitter"/>
  <tabbox id="mytool-sidebar" class="devtools-sidebar-tabs" hidden="true">
    <tabs/>
    <tabpanels flex="1"/>
  </tabbox>

A sidebar is composed of tabs. Each tab will hold an iframe. For example, in the Inspector, there are 3 tabs (Computed View, Rule View, Layout View). The user can select the tab they want to see.

If the availability of the tabs depends on some tool-related conditions, we might want to not let the user select a tab. This API provides methods to hide the tabstripe. For example, in the Web Console, there are 2 views (Network View and Object View). These views are only available in certain conditions controlled by the WebConsole code. So it's up the WebConsole the hide and show the sidebar, and select the correct tab.

If the loaded document exposes a ``window.setPanel(ToolPanel)`` function, the sidebar will call it once the document is loaded.

.. list-table:: Methods
  :widths: 70 30
  :header-rows: 1

  * - Method
    - Description

  * - ``new ToolSidebar(xul:tabbox, ToolPanel, uid, showTabstripe=true)``
    - ToolSidebar constructor

  * - ``void addTab(tabId, url, selected=false)``
    - Add a tab in the sidebar

  * - ``void select(tabId)``
    - Select a tab

  * - ``void hide()``
    - Hide the sidebar

  * - ``void show()``
    - Show the sidebar

  * - ``void toggle()``
    - Toggle the sidebar

  * - ``void getWindowForTab(tabId)``
    - Get the iframe containing the tab content

  * - ``tabId getCurrentTabID()``
    - Return the id of tabId of the current tab

  * - ``tabbox getTab(tabId)``
    - Return a tab given its id

  * - ``destroy()``
    - Destroy the ToolSidebar object

.. list-table:: Events
  :widths: 70 30
  :header-rows: 1

  * - Events
    - Description

  * - ``new-tab-registered``
    - A new tab has been added

  * - ``{tabId}-ready``
    - Tab is loaded and can be used

  * - ``{tabId}-selected``
    - Tab has been selected and is visible

  * - ``{tabId}-unselected``
    - Tab has been unselected and is not visible

  * - ``show``
    - The sidebar has been opened.

  * - ``hide``
    - The sidebar has been closed.


Examples
--------

Register a tool

.. code-block:: JavaScript

  gDevTools.registerTool({
    // FIXME: missing key related properties.
    id: "inspector",
    icon: "chrome://browser/skin/devtools/inspector-icon.png",
    url: "chrome://browser/content/devtools/inspector/inspector.xul",
    get label() {
      let strings = Services.strings.createBundle("chrome://browser/locale/devtools/inspector.properties");
      return strings.GetStringFromName("inspector.label");
    },

    isToolSupported: function(toolbox) {
      return toolbox.commands.descriptorFront.isLocalTab;
    },

    build: function(iframeWindow, toolbox, node) {
      return new InspectorPanel(iframeWindow, toolbox, node);
    }
  });


Open a tool, or select it if the toolbox is already open:

.. code-block:: JavaScript

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = gDevTools.openToolbox(target, null, "inspector");

  toolbox.once("inspector-ready", function(event, panel) {
    let inspector = toolbox.getToolPanels().get("inspector");
    inspector.selection.setNode(target, "browser-context-menu");
  });


Add a sidebar to an existing tool:

.. code-block:: JavaScript

  let sidebar = new ToolSidebar(xulTabbox, toolPanel, "toolId");
  sidebar.addTab("tab1", "chrome://browser/content/.../tab1.xhtml", true);
  sidebar.addTab("tab2", "chrome://browser/content/.../tab2.xhtml", false);
  sidebar.show();
