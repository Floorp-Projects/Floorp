/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Developer edition promo preferences
pref("devtools.devedition.promo.shown", false);
pref("devtools.devedition.promo.url", "https://www.mozilla.org/firefox/developer/?utm_source=firefox-dev-tools&utm_medium=firefox-browser&utm_content=betadoorhanger");

// Only potentially show in beta release
#if MOZ_UPDATE_CHANNEL == beta
  pref("devtools.devedition.promo.enabled", true);
#else
  pref("devtools.devedition.promo.enabled", false);
#endif

// DevTools development workflow
pref("devtools.loader.hotreload", false);

// Developer toolbar preferences
pref("devtools.toolbar.enabled", true);
pref("devtools.toolbar.visible", false);

// Enable DevTools WebIDE by default
pref("devtools.webide.enabled", true);

// Toolbox preferences
pref("devtools.toolbox.footer.height", 250);
pref("devtools.toolbox.sidebar.width", 500);
pref("devtools.toolbox.host", "bottom");
pref("devtools.toolbox.previousHost", "side");
pref("devtools.toolbox.selectedTool", "webconsole");
pref("devtools.toolbox.sideEnabled", true);
pref("devtools.toolbox.zoomValue", "1");
pref("devtools.toolbox.splitconsoleEnabled", false);
pref("devtools.toolbox.splitconsoleHeight", 100);

// Toolbox Button preferences
pref("devtools.command-button-pick.enabled", true);
pref("devtools.command-button-frames.enabled", true);
pref("devtools.command-button-splitconsole.enabled", true);
pref("devtools.command-button-paintflashing.enabled", false);
pref("devtools.command-button-scratchpad.enabled", false);
pref("devtools.command-button-responsive.enabled", true);
pref("devtools.command-button-screenshot.enabled", false);
pref("devtools.command-button-rulers.enabled", false);
pref("devtools.command-button-measure.enabled", false);
pref("devtools.command-button-noautohide.enabled", false);

// Inspector preferences
// Enable the Inspector
pref("devtools.inspector.enabled", true);
// What was the last active sidebar in the inspector
pref("devtools.inspector.activeSidebar", "ruleview");
pref("devtools.inspector.remote", false);
// Collapse pseudo-elements by default in the rule-view
pref("devtools.inspector.show_pseudo_elements", false);
// The default size for image preview tooltips in the rule-view/computed-view/markup-view
pref("devtools.inspector.imagePreviewTooltipSize", 300);
// Enable user agent style inspection in rule-view
pref("devtools.inspector.showUserAgentStyles", false);
// Show all native anonymous content (like controls in <video> tags)
pref("devtools.inspector.showAllAnonymousContent", false);
// Enable the MDN docs tooltip
pref("devtools.inspector.mdnDocsTooltip.enabled", false);
// Enable the new color widget
pref("devtools.inspector.colorWidget.enabled", false);
// Enable the CSS shapes highlighter
pref("devtools.inspector.shapesHighlighter.enabled", false);

// Enable the Font Inspector
pref("devtools.fontinspector.enabled", true);

// Counter to promote the inspector layout view.
// @remove after release 56 (See Bug 1355747)
pref("devtools.promote.layoutview", 1);
// Whether or not to show the promote bar in the layout view
// @remove after release 56 (See Bug 1355747)
pref("devtools.promote.layoutview.showPromoteBar", true);

// Grid highlighter preferences
pref("devtools.gridinspector.gridOutlineMaxColumns", 50);
pref("devtools.gridinspector.gridOutlineMaxRows", 50);
pref("devtools.gridinspector.showGridAreas", false);
pref("devtools.gridinspector.showGridLineNumbers", false);
pref("devtools.gridinspector.showInfiniteLines", false);

// Whether or not the box model panel is opened in the computed view
pref("devtools.computed.boxmodel.opened", true);
// Whether or not the box model panel is opened in the layout view
pref("devtools.layout.boxmodel.opened", true);
// Whether or not the grid inspector panel is opened in the layout view
pref("devtools.layout.grid.opened", true);

// By how many times eyedropper will magnify pixels
pref("devtools.eyedropper.zoom", 6);

// Enable to collapse attributes that are too long.
pref("devtools.markup.collapseAttributes", true);

// Length to collapse attributes
pref("devtools.markup.collapseAttributeLength", 120);

// DevTools default color unit
pref("devtools.defaultColorUnit", "authored");

// Enable the Responsive UI tool
pref("devtools.responsiveUI.no-reload-notification", false);

// Enable the Memory tools
pref("devtools.memory.enabled", true);

pref("devtools.memory.custom-census-displays", "{}");
pref("devtools.memory.custom-label-displays", "{}");
pref("devtools.memory.custom-tree-map-displays", "{}");

pref("devtools.memory.max-individuals", 1000);
pref("devtools.memory.max-retaining-paths", 10);

// Enable the Performance tools
pref("devtools.performance.enabled", true);

// The default Performance UI settings
pref("devtools.performance.memory.sample-probability", "0.05");
// Can't go higher than this without causing internal allocation overflows while
// serializing the allocations data over the RDP.
pref("devtools.performance.memory.max-log-length", 125000);
pref("devtools.performance.timeline.hidden-markers",
  "[\"Composite\",\"CompositeForwardTransaction\"]");
pref("devtools.performance.profiler.buffer-size", 10000000);
pref("devtools.performance.profiler.sample-frequency-khz", 1);
pref("devtools.performance.ui.invert-call-tree", true);
pref("devtools.performance.ui.invert-flame-graph", false);
pref("devtools.performance.ui.flatten-tree-recursion", true);
pref("devtools.performance.ui.show-platform-data", false);
pref("devtools.performance.ui.show-idle-blocks", true);
pref("devtools.performance.ui.enable-memory", false);
pref("devtools.performance.ui.enable-allocations", false);
pref("devtools.performance.ui.enable-framerate", true);
pref("devtools.performance.ui.show-jit-optimizations", false);
pref("devtools.performance.ui.show-triggers-for-gc-types",
  "TOO_MUCH_MALLOC ALLOC_TRIGGER LAST_DITCH EAGER_ALLOC_TRIGGER");

// Temporary pref disabling memory flame views
// TODO remove once we have flame charts via bug 1148663
pref("devtools.performance.ui.enable-memory-flame", false);

// Enable experimental options in the UI only in Nightly
#if defined(NIGHTLY_BUILD)
pref("devtools.performance.ui.experimental", true);
#else
pref("devtools.performance.ui.experimental", false);
#endif

// The default cache UI setting
pref("devtools.cache.disabled", false);

// The default service workers UI setting
pref("devtools.serviceWorkers.testing.enabled", false);

// Enable the Network Monitor
pref("devtools.netmonitor.enabled", true);

// The default Network Monitor UI settings
pref("devtools.netmonitor.panes-network-details-width", 550);
pref("devtools.netmonitor.panes-network-details-height", 450);
pref("devtools.netmonitor.filters", "[\"all\"]");
pref("devtools.netmonitor.visibleColumns",
  "[\"status\",\"method\",\"file\",\"domain\",\"cause\",\"type\",\"transferred\",\"contentSize\",\"waterfall\"]"
);

// The default Network monitor HAR export setting
pref("devtools.netmonitor.har.defaultLogDir", "");
pref("devtools.netmonitor.har.defaultFileName", "Archive %date");
pref("devtools.netmonitor.har.jsonp", false);
pref("devtools.netmonitor.har.jsonpCallback", "");
pref("devtools.netmonitor.har.includeResponseBodies", true);
pref("devtools.netmonitor.har.compress", false);
pref("devtools.netmonitor.har.forceExport", false);
pref("devtools.netmonitor.har.pageLoadedTimeout", 1500);
pref("devtools.netmonitor.har.enableAutoExportToFile", false);

// Scratchpad settings
// - recentFileMax: The maximum number of recently-opened files
//                  stored. Setting this preference to 0 will not
//                  clear any recent files, but rather hide the
//                  'Open Recent'-menu.
// - lineNumbers: Whether to show line numbers or not.
// - wrapText: Whether to wrap text or not.
// - showTrailingSpace: Whether to highlight trailing space or not.
// - editorFontSize: Editor font size configuration.
// - enableAutocompletion: Whether to enable JavaScript autocompletion.
pref("devtools.scratchpad.recentFilesMax", 10);
pref("devtools.scratchpad.lineNumbers", true);
pref("devtools.scratchpad.wrapText", false);
pref("devtools.scratchpad.showTrailingSpace", false);
pref("devtools.scratchpad.editorFontSize", 12);
pref("devtools.scratchpad.enableAutocompletion", true);

// Enable the Storage Inspector
pref("devtools.storage.enabled", true);

// Enable the Style Editor.
pref("devtools.styleeditor.enabled", true);
pref("devtools.styleeditor.source-maps-enabled", true);
pref("devtools.styleeditor.autocompletion-enabled", true);
pref("devtools.styleeditor.showMediaSidebar", true);
pref("devtools.styleeditor.mediaSidebarWidth", 238);
pref("devtools.styleeditor.navSidebarWidth", 245);
pref("devtools.styleeditor.transitions", true);

// Screenshot Option Settings.
pref("devtools.screenshot.clipboard.enabled", false);
pref("devtools.screenshot.audio.enabled", true);

// Enable the Shader Editor.
pref("devtools.shadereditor.enabled", false);

// Enable the Canvas Debugger.
pref("devtools.canvasdebugger.enabled", false);

// Enable the Web Audio Editor
pref("devtools.webaudioeditor.enabled", false);

// Enable Scratchpad
pref("devtools.scratchpad.enabled", false);

// Make sure the DOM panel is hidden by default
pref("devtools.dom.enabled", false);

// Web Audio Editor Inspector Width should be a preference
pref("devtools.webaudioeditor.inspectorWidth", 300);

// Web console filters
pref("devtools.webconsole.filter.error", true);
pref("devtools.webconsole.filter.warn", true);
pref("devtools.webconsole.filter.info", true);
pref("devtools.webconsole.filter.log", true);
pref("devtools.webconsole.filter.debug", true);
pref("devtools.webconsole.filter.css", false);
pref("devtools.webconsole.filter.net", false);
pref("devtools.webconsole.filter.netxhr", false);
// Deprecated - old console frontend
pref("devtools.webconsole.filter.network", true);
pref("devtools.webconsole.filter.networkinfo", false);
pref("devtools.webconsole.filter.netwarn", true);
pref("devtools.webconsole.filter.csserror", true);
pref("devtools.webconsole.filter.cssparser", false);
pref("devtools.webconsole.filter.csslog", false);
pref("devtools.webconsole.filter.exception", true);
pref("devtools.webconsole.filter.jswarn", true);
pref("devtools.webconsole.filter.jslog", false);
pref("devtools.webconsole.filter.secerror", true);
pref("devtools.webconsole.filter.secwarn", true);
pref("devtools.webconsole.filter.serviceworkers", true);
pref("devtools.webconsole.filter.sharedworkers", false);
pref("devtools.webconsole.filter.windowlessworkers", false);
pref("devtools.webconsole.filter.servererror", false);
pref("devtools.webconsole.filter.serverwarn", false);
pref("devtools.webconsole.filter.serverinfo", false);
pref("devtools.webconsole.filter.serverlog", false);

// Remember the Browser Console filters
pref("devtools.browserconsole.filter.network", true);
pref("devtools.browserconsole.filter.networkinfo", false);
pref("devtools.browserconsole.filter.netwarn", true);
pref("devtools.browserconsole.filter.netxhr", false);
pref("devtools.browserconsole.filter.csserror", true);
pref("devtools.browserconsole.filter.cssparser", false);
pref("devtools.browserconsole.filter.csslog", false);
pref("devtools.browserconsole.filter.exception", true);
pref("devtools.browserconsole.filter.jswarn", true);
pref("devtools.browserconsole.filter.jslog", true);
pref("devtools.browserconsole.filter.error", true);
pref("devtools.browserconsole.filter.warn", true);
pref("devtools.browserconsole.filter.info", true);
pref("devtools.browserconsole.filter.log", true);
pref("devtools.browserconsole.filter.secerror", true);
pref("devtools.browserconsole.filter.secwarn", true);
pref("devtools.browserconsole.filter.serviceworkers", true);
pref("devtools.browserconsole.filter.sharedworkers", true);
pref("devtools.browserconsole.filter.windowlessworkers", true);
pref("devtools.browserconsole.filter.servererror", false);
pref("devtools.browserconsole.filter.serverwarn", false);
pref("devtools.browserconsole.filter.serverinfo", false);
pref("devtools.browserconsole.filter.serverlog", false);

// Web console filter settings bar
pref("devtools.webconsole.ui.filterbar", false);

// Max number of inputs to store in web console history.
pref("devtools.webconsole.inputHistoryCount", 50);

// Persistent logging: |true| if you want the Web Console to keep all of the
// logged messages after reloading the page, |false| if you want the output to
// be cleared each time page navigation happens.
pref("devtools.webconsole.persistlog", false);

// Web Console timestamp: |true| if you want the logs and instructions
// in the Web Console to display a timestamp, or |false| to not display
// any timestamps.
pref("devtools.webconsole.timestampMessages", false);

// Web Console automatic multiline mode: |true| if you want incomplete statements
// to automatically trigger multiline editing (equivalent to shift + enter).
pref("devtools.webconsole.autoMultiline", true);

// Enable the new webconsole frontend
#if defined(NIGHTLY_BUILD) || defined(MOZ_DEV_EDITION)
pref("devtools.webconsole.new-frontend-enabled", true);
#else
pref("devtools.webconsole.new-frontend-enabled", false);
#endif

// Enable client-side mapping service for source maps
pref("devtools.source-map.client-service.enabled", true);

// The number of lines that are displayed in the web console.
pref("devtools.hud.loglimit", 1000);

// The number of lines that are displayed in the web console for the Net,
// CSS, JS and Web Developer categories. These defaults should be kept in sync
// with DEFAULT_LOG_LIMIT in the webconsole frontend.
pref("devtools.hud.loglimit.network", 1000);
pref("devtools.hud.loglimit.cssparser", 1000);
pref("devtools.hud.loglimit.exception", 1000);
pref("devtools.hud.loglimit.console", 1000);

// The developer tools editor configuration:
// - tabsize: how many spaces to use when a Tab character is displayed.
// - expandtab: expand Tab characters to spaces.
// - keymap: which keymap to use (can be 'default', 'emacs' or 'vim')
// - autoclosebrackets: whether to permit automatic bracket/quote closing.
// - detectindentation: whether to detect the indentation from the file
// - enableCodeFolding: Whether to enable code folding or not.
pref("devtools.editor.tabsize", 2);
pref("devtools.editor.expandtab", true);
pref("devtools.editor.keymap", "default");
pref("devtools.editor.autoclosebrackets", true);
pref("devtools.editor.detectindentation", true);
pref("devtools.editor.enableCodeFolding", true);
pref("devtools.editor.autocomplete", true);

// Pref to store the browser version at the time of a telemetry ping for an
// opened developer tool. This allows us to ping telemetry just once per browser
// version for each user.
pref("devtools.telemetry.tools.opened.version", "{}");

// Enable the HTML responsive design mode for all channels.
pref("devtools.responsive.html.enabled", true);
