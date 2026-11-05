var qaKan24 = (() => {
  var __defProp = Object.defineProperty;
  var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
  var __getOwnPropNames = Object.getOwnPropertyNames;
  var __hasOwnProp = Object.prototype.hasOwnProperty;
  var __export = (target, all) => {
    for (var name in all)
      __defProp(target, name, { get: all[name], enumerable: true });
  };
  var __copyProps = (to, from, except, desc) => {
    if (from && typeof from === "object" || typeof from === "function") {
      for (let key of __getOwnPropNames(from))
        if (!__hasOwnProp.call(to, key) && key !== except)
          __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
    }
    return to;
  };
  var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

  // <stdin>
  var stdin_exports = {};
  __export(stdin_exports, {
    addVerticalTabsObserver: () => addVerticalTabsObserver,
    getDesignSettings: () => getDesignSettings,
    rpc: () => rpc,
    saveDesignSettings: () => saveDesignSettings,
    usesSettingsActor: () => usesSettingsActor
  });

  // qa-kan24-pr2950/browser-features/chrome/common/designs/utils/tabbar-style-sync.ts
  var VERTICAL_TABS_PREF = "sidebar.verticalTabs";
  function normalizeTabbarStyle(value) {
    return value === "horizontal" || value === "vertical" || value === "multirow" ? value : "horizontal";
  }
  function resolveTabbarStyleFromNative(isVertical, current) {
    if (isVertical) return "vertical";
    return current === "vertical" ? "horizontal" : current;
  }

  // qa-kan24-pr2950/browser-features/chrome/common/designs/utils/vertical-tabs-observer.ts
  function addVerticalTabsObserver(getStyle, applyStyle) {
    const onPrefChange = () => {
      try {
        if (Services.prefs.getPrefType(VERTICAL_TABS_PREF) !== Services.prefs.PREF_BOOL) {
          return;
        }
        const isVertical = Services.prefs.getBoolPref(VERTICAL_TABS_PREF, false);
        const current = getStyle();
        const next = resolveTabbarStyleFromNative(isVertical, current);
        if (next !== current) {
          applyStyle(next);
        }
      } catch (e) {
        console.error(
          "[Designs]",
          "Failed to sync the tab bar style from the native vertical tabs pref:",
          e
        );
      }
    };
    Services.prefs.addObserver(VERTICAL_TABS_PREF, onPrefChange);
    return () => {
      try {
        Services.prefs.removeObserver(VERTICAL_TABS_PREF, onPrefChange);
      } catch (e) {
        console.error(
          "[Designs]",
          "Failed to remove the vertical tabs observer:",
          e
        );
      }
    };
  }

  // Floorp/node_modules/.deno/birpc@4.0.0/node_modules/birpc/dist/index.mjs
  var TYPE_REQUEST = "q";
  var TYPE_RESPONSE = "s";
  function createPromiseWithResolvers() {
    let resolve;
    let reject;
    return {
      promise: new Promise((res, rej) => {
        resolve = res;
        reject = rej;
      }),
      resolve,
      reject
    };
  }
  var random = Math.random.bind(Math);
  var urlAlphabet = "useandom-26T198340PX75pxJACKVERYMINDBUSHWOLF_GQZbfghjklqvwyzrict";
  function nanoid(size = 21) {
    let id = "";
    let i = size;
    while (i--) id += urlAlphabet[random() * 64 | 0];
    return id;
  }
  var DEFAULT_TIMEOUT = 6e4;
  var defaultSerialize = (i) => i;
  var defaultDeserialize = defaultSerialize;
  var { clearTimeout, setTimeout } = globalThis;
  function createBirpc($functions, options) {
    const { post, on, off = () => {
    }, eventNames = [], serialize = defaultSerialize, deserialize = defaultDeserialize, resolver, bind = "rpc", timeout = DEFAULT_TIMEOUT, proxify = true } = options;
    let $closed = false;
    const _rpcPromiseMap = /* @__PURE__ */ new Map();
    let _promiseInit;
    let rpc2;
    async function _call(method, args, event, optional) {
      if ($closed) throw new Error(`[birpc] rpc is closed, cannot call "${method}"`);
      const req = {
        m: method,
        a: args,
        t: TYPE_REQUEST
      };
      if (optional) req.o = true;
      const send = async (_req) => post(serialize(_req));
      if (event) {
        await send(req);
        return;
      }
      if (_promiseInit) try {
        await _promiseInit;
      } finally {
        _promiseInit = void 0;
      }
      let { promise, resolve, reject } = createPromiseWithResolvers();
      const id = nanoid();
      req.i = id;
      let timeoutId;
      async function handler(newReq = req) {
        if (timeout >= 0) {
          timeoutId = setTimeout(() => {
            try {
              if (options.onTimeoutError?.call(rpc2, method, args) !== true) throw new Error(`[birpc] timeout on calling "${method}"`);
            } catch (e) {
              reject(e);
            }
            _rpcPromiseMap.delete(id);
          }, timeout);
          if (typeof timeoutId === "object") timeoutId = timeoutId.unref?.();
        }
        _rpcPromiseMap.set(id, {
          resolve,
          reject,
          timeoutId,
          method
        });
        await send(newReq);
        return promise;
      }
      try {
        if (options.onRequest) await options.onRequest.call(rpc2, req, handler, resolve);
        else await handler();
      } catch (e) {
        if (options.onGeneralError?.call(rpc2, e) !== true) throw e;
        return;
      } finally {
        clearTimeout(timeoutId);
        _rpcPromiseMap.delete(id);
      }
      return promise;
    }
    const builtinMethods = {
      $call: (method, ...args) => _call(method, args, false),
      $callOptional: (method, ...args) => _call(method, args, false, true),
      $callEvent: (method, ...args) => _call(method, args, true),
      $callRaw: (options$1) => _call(options$1.method, options$1.args, options$1.event, options$1.optional),
      $rejectPendingCalls,
      get $closed() {
        return $closed;
      },
      get $meta() {
        return options.meta;
      },
      $close,
      $functions
    };
    if (proxify) rpc2 = new Proxy({}, { get(_, method) {
      if (Object.prototype.hasOwnProperty.call(builtinMethods, method)) return builtinMethods[method];
      if (method === "then" && !eventNames.includes("then") && !("then" in $functions)) return void 0;
      const sendEvent = (...args) => _call(method, args, true);
      if (eventNames.includes(method)) {
        sendEvent.asEvent = sendEvent;
        return sendEvent;
      }
      const sendCall = (...args) => _call(method, args, false);
      sendCall.asEvent = sendEvent;
      return sendCall;
    } });
    else rpc2 = builtinMethods;
    function $close(customError) {
      $closed = true;
      _rpcPromiseMap.forEach(({ reject, method }) => {
        const error = /* @__PURE__ */ new Error(`[birpc] rpc is closed, cannot call "${method}"`);
        if (customError) {
          customError.cause ??= error;
          return reject(customError);
        }
        reject(error);
      });
      _rpcPromiseMap.clear();
      off(onMessage);
    }
    function $rejectPendingCalls(handler) {
      const handlerResults = Array.from(_rpcPromiseMap.values()).map(({ method, reject }) => {
        if (!handler) return reject(/* @__PURE__ */ new Error(`[birpc]: rejected pending call "${method}".`));
        return handler({
          method,
          reject
        });
      });
      _rpcPromiseMap.clear();
      return handlerResults;
    }
    async function onMessage(data, ...extra) {
      let msg;
      try {
        msg = deserialize(data);
      } catch (e) {
        if (options.onGeneralError?.call(rpc2, e) !== true) throw e;
        return;
      }
      if (msg.t === TYPE_REQUEST) {
        const { m: method, a: args, o: optional } = msg;
        let result, error;
        let fn = await (resolver ? resolver.call(rpc2, method, $functions[method]) : $functions[method]);
        if (optional) fn ||= () => void 0;
        if (!fn) error = /* @__PURE__ */ new Error(`[birpc] function "${method}" not found`);
        else try {
          result = await fn.apply(bind === "rpc" ? rpc2 : $functions, args);
        } catch (e) {
          error = e;
        }
        if (msg.i) {
          if (error && options.onFunctionError) {
            if (options.onFunctionError.call(rpc2, error, method, args) === true) return;
          }
          if (!error) try {
            await post(serialize({
              t: TYPE_RESPONSE,
              i: msg.i,
              r: result
            }), ...extra);
            return;
          } catch (e) {
            error = e;
            if (options.onGeneralError?.call(rpc2, e, method, args) !== true) throw e;
          }
          try {
            await post(serialize({
              t: TYPE_RESPONSE,
              i: msg.i,
              e: error
            }), ...extra);
          } catch (e) {
            if (options.onGeneralError?.call(rpc2, e, method, args) !== true) throw e;
          }
        }
      } else {
        const { i: ack, r: result, e: error } = msg;
        const promise = _rpcPromiseMap.get(ack);
        if (promise) {
          clearTimeout(promise.timeoutId);
          if (error) promise.reject(error);
          else promise.resolve(result);
        }
        _rpcPromiseMap.delete(ack);
      }
    }
    _promiseInit = on(onMessage);
    return rpc2;
  }

  // qa-kan24-pr2950/libs/ui/settings-rpc-origin.ts
  function usesSettingsActor(pageUrl, developmentPort) {
    const url = new URL(pageUrl);
    return url.protocol === "http:" && url.port === developmentPort && (url.hostname === "localhost" || url.hostname === "127.0.0.1");
  }

  // qa-kan24-pr2950/browser-features/pages-settings/src/lib/rpc/rpc.ts
  function compareAndSetDirectPreference(prefName, expectedValue, prefValue, prefType, read, write) {
    const currentType = Services.prefs.getPrefType(prefName);
    if (currentType !== Services.prefs.PREF_INVALID && currentType !== prefType) {
      return Promise.resolve({
        updated: false,
        currentValue: null,
        typeMismatch: true
      });
    }
    const currentValue = currentType === prefType ? read() : null;
    if (currentValue !== expectedValue) {
      return Promise.resolve({ updated: false, currentValue });
    }
    write(prefValue);
    return Promise.resolve({ updated: true, currentValue: prefValue });
  }
  function readDirectPreference(prefName, prefType, read) {
    const currentType = Services.prefs.getPrefType(prefName);
    return Promise.resolve({
      value: currentType === prefType ? read() : null,
      typeMismatch: currentType !== Services.prefs.PREF_INVALID && currentType !== prefType
    });
  }
  var SETTINGS_BRIDGE_TIMEOUT_MS = 15e3;
  var SETTINGS_BRIDGE_POLL_INTERVAL_MS = 25;
  function waitForSettingsBridge() {
    const startedAt = Date.now();
    return new Promise((resolve, reject) => {
      const poll = () => {
        const page = globalThis;
        if (typeof page.NRSettingsSend === "function" && typeof page.NRSettingsRegisterReceiveCallback === "function") {
          resolve(page);
          return;
        }
        if (Date.now() - startedAt >= SETTINGS_BRIDGE_TIMEOUT_MS) {
          reject(new Error("NRSettings page RPC bridge did not initialize"));
          return;
        }
        globalThis.setTimeout(poll, SETTINGS_BRIDGE_POLL_INTERVAL_MS);
      };
      poll();
    });
  }
  var isLocalhost5183 = usesSettingsActor(globalThis.location.href, "5183");
  function createSettingsBridgeTransport(resolveBridge = waitForSettingsBridge) {
    let receiveCallback = null;
    let receiverReady = null;
    const ensureReceiver = () => {
      if (receiverReady) return receiverReady;
      if (!receiveCallback) {
        return Promise.reject(
          new Error("NRSettings page RPC receiver was not initialized")
        );
      }
      const attempt = resolveBridge().then((page) => {
        if (!receiveCallback) {
          throw new Error("NRSettings page RPC receiver was not initialized");
        }
        page.NRSettingsRegisterReceiveCallback(receiveCallback);
        return page;
      });
      receiverReady = attempt;
      void attempt.catch(() => {
        if (receiverReady === attempt) receiverReady = null;
      });
      return attempt;
    };
    return {
      on: (callback) => {
        receiveCallback = callback;
        receiverReady = null;
        return ensureReceiver().then(() => void 0);
      },
      post: (data) => ensureReceiver().then((page) => page.NRSettingsSend(data))
    };
  }
  var settingsBridgeTransport = createSettingsBridgeTransport();
  function registerSettingsBridgeReceiver(callback) {
    return settingsBridgeTransport.on(callback);
  }
  function sendSettingsBridgeMessage(data) {
    return settingsBridgeTransport.post(data);
  }
  var directServicesFunctions = {
    getWebAppLifecycleSettings: () => {
      const { NativeAppRuntime } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/NativeAppRuntime.sys.mjs"
      );
      const { AppLifecycle } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/AppLifecycle.sys.mjs"
      );
      NativeAppRuntime.isAvailable();
      return Promise.resolve(AppLifecycle.getSettings());
    },
    getContextMenuCatalog: () => {
      const { ContextMenuCatalogService } = ChromeUtils.importESModule(
        "resource://noraneko/modules/context-menu/ContextMenuCatalogService.sys.mjs"
      );
      return Promise.resolve(ContextMenuCatalogService.getSnapshot());
    },
    getContextMenuCatalogRevision: () => {
      const { ContextMenuCatalogService } = ChromeUtils.importESModule(
        "resource://noraneko/modules/context-menu/ContextMenuCatalogService.sys.mjs"
      );
      return Promise.resolve(ContextMenuCatalogService.getRevision());
    },
    getBoolPref: (prefName) => {
      if (Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_BOOL) {
        return Promise.resolve(null);
      }
      return Promise.resolve(Services.prefs.getBoolPref(prefName));
    },
    getIntPref: (prefName) => {
      if (Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_INT) {
        return Promise.resolve(null);
      }
      return Promise.resolve(Services.prefs.getIntPref(prefName));
    },
    getStringPref: (prefName) => {
      if (Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_STRING) {
        return Promise.resolve(null);
      }
      return Promise.resolve(Services.prefs.getStringPref(prefName));
    },
    getBoolPrefState: (prefName) => readDirectPreference(
      prefName,
      Services.prefs.PREF_BOOL,
      () => Services.prefs.getBoolPref(prefName)
    ),
    getStringPrefState: (prefName) => readDirectPreference(
      prefName,
      Services.prefs.PREF_STRING,
      () => Services.prefs.getStringPref(prefName)
    ),
    setBoolPref: (prefName, value) => {
      Services.prefs.setBoolPref(prefName, value);
      return Promise.resolve();
    },
    setIntPref: (prefName, value) => {
      Services.prefs.setIntPref(prefName, value);
      return Promise.resolve();
    },
    setStringPref: (prefName, value) => {
      Services.prefs.setStringPref(prefName, value);
      return Promise.resolve();
    },
    compareAndSetBoolPref: (prefName, expectedValue, prefValue) => compareAndSetDirectPreference(
      prefName,
      expectedValue,
      prefValue,
      Services.prefs.PREF_BOOL,
      () => Services.prefs.getBoolPref(prefName),
      (value) => Services.prefs.setBoolPref(prefName, value)
    ),
    compareAndSetStringPref: (prefName, expectedValue, prefValue) => compareAndSetDirectPreference(
      prefName,
      expectedValue,
      prefValue,
      Services.prefs.PREF_STRING,
      () => Services.prefs.getStringPref(prefName),
      (value) => Services.prefs.setStringPref(prefName, value)
    )
  };
  var rpc = isLocalhost5183 ? createBirpc(
    {},
    {
      // birpc waits for the value returned by `on` before sending its first
      // request. Return the bridge promises so an early catalog request cannot
      // race the actor callback installation and be silently dropped.
      post: sendSettingsBridgeMessage,
      on: registerSettingsBridgeReceiver,
      serialize: (v) => JSON.stringify(v),
      deserialize: (v) => JSON.parse(v)
    }
  ) : directServicesFunctions;

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/autohide-back-button.css?inline
  var autohide_back_button_default = '/* Floorp chrome-extras: autohideBackButton\n * Ported from vendored Lepton (leptonChrome.css 26076-26091, 2 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp\'s dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n *\n * Lepton wraps these in\n *\n * `@-moz-document url("chrome://browser/content/browser.xhtml")`; the\n *\n * wrapper is dropped because Floorp injects into that document directly.\n */\n\n/* leptonChrome.css:26076-26082 */\n:root:not([customizing="true"]) #back-button[disabled="true"] {\n  margin-left: var(--uc-toolbarbutton-hide-size) !important;\n  opacity: 0 !important;\n  pointer-events: none;\n}\n\n/* leptonChrome.css:26083-26091 */\n@media (prefers-reduced-motion: no-preference) {\n  #back-button {\n    transition-property: background-color, opacity, margin-left !important;\n  }\n  #back-button[disabled="true"] {\n    transition: background-color 1s var(--animation-easing-function), opacity 1s var(--animation-easing-function),\n      margin-left 0.5s ease !important;\n  }\n}\n';

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/autohide-forward-button.css?inline
  var autohide_forward_button_default = '/* Floorp chrome-extras: autohideForwardButton\n * Ported from vendored Lepton (leptonChrome.css 26092-26107, 2 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp\'s dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n */\n\n/* leptonChrome.css:26092-26098 */\n:root:not([customizing="true"]) #forward-button[disabled="true"] {\n  margin-left: var(--uc-toolbarbutton-hide-size) !important;\n  opacity: 0 !important;\n  pointer-events: none;\n}\n\n/* leptonChrome.css:26099-26107 */\n@media (prefers-reduced-motion: no-preference) {\n  #forward-button {\n    transition-property: background-color, opacity, margin-left !important;\n  }\n  #forward-button[disabled="true"] {\n    transition: background-color 1s var(--animation-easing-function), opacity 1s var(--animation-easing-function),\n      margin-left 0.5s ease !important;\n  }\n}\n';

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/autohide-navbar.css?inline
  var autohide_navbar_default = "/* Floorp chrome-extras: autohideNavbar\n *\n * Floorp-native implementation \u2014 NOT a port.\n *\n * Lepton implements this by shrinking `#nav-bar` with a negative\n * `margin-bottom` derived from `--uc-navbar-height`, inside ~2,500 lines of\n * nested `-moz-pref(...)` blocks that also require `--uc-tabbar-height`,\n * `--uc-menubar-height`, `--uc-bm-height` and cross-references to eight other\n * Lepton toggles. Transporting that whole layout system is not feasible, so the\n * same *behaviour* (hide the toolbar until the pointer or focus returns to the\n * toolbox) is expressed directly with the Lepton variables inlined.\n *\n * Works on every design because it only touches `#navigator-toolbox` and\n * `#nav-bar`, which all designs share.\n */\n\n#navigator-toolbox {\n  position: relative;\n}\n\n/* Hovering the toolbox must sit above the page. */\n#navigator-toolbox:is(:hover, :focus-within) {\n  z-index: 2;\n}\n\n#navigator-toolbox:is(:hover, :focus-within) #PersonalToolbar {\n  z-index: -1;\n}\n\n/* `--uc-navbar-height-default` is\n * `calc(16px + 2 * (inner + outer padding))` \u2014 inlined below.\n * `margin-bottom: 0` is what Lepton uses when `hiddenNavbar` is also on. */\n#nav-bar:not([customizing]) {\n  margin-bottom: calc(\n    -1 * (16px + 2 * (var(--toolbarbutton-inner-padding) + var(--toolbarbutton-outer-padding)))\n  );\n  opacity: 0;\n  will-change: margin-bottom, opacity;\n}\n\n#navigator-toolbox:is(:hover, :focus-within) #nav-bar:not([customizing]) {\n  margin-bottom: 0;\n  opacity: 1;\n}\n\n/* A hidden toolbar must not swallow clicks meant for the page. */\n#navigator-toolbox:not(:hover, :focus-within) #nav-bar:not([customizing]) #urlbar {\n  pointer-events: none;\n}\n\n/* Keep the window draggable where the toolbar used to be. */\n#mainPopupSet:not(:hover) ~ #navigator-toolbox:not(:hover, :focus-within) #nav-bar:not([customizing]) {\n  -moz-window-dragging: no-drag;\n}\n\n@media (prefers-reduced-motion: no-preference) {\n  #nav-bar:not([customizing]) {\n    transition:\n      margin-bottom 0.25s var(--animation-easing-function) 600ms,\n      opacity 0.25s var(--animation-easing-function) 600ms !important;\n  }\n\n  #navigator-toolbox:is(:hover, :focus-within) #nav-bar:not([customizing]) {\n    transition-delay: 0s !important;\n  }\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/autohide-page-action.css?inline
  var autohide_page_action_default = "/* Floorp chrome-extras: autohidePageAction\n * Ported from vendored Lepton (leptonChrome.css 26109-26139, 3 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp's dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n *\n * The extra transition block gated on `userChrome.decoration.animate` is\n *\n * not ported (Lepton-only sub-option).\n */\n\n/* leptonChrome.css:26109-26120 */\n#page-action-buttons > .urlbar-page-action {\n  margin-inline-end: calc(-16px - 2 * var(--urlbar-icon-padding));\n  opacity: 0;\n}\n#urlbar-container:is(:hover, :focus-within) #page-action-buttons > .urlbar-page-action,\n.urlbar-page-action[open],\n.urlbar-page-action[open] ~ .urlbar-page-action {\n  margin-inline-end: 0px !important;\n  opacity: 1;\n}\n\n/* leptonChrome.css:26121-26126 */\n@media (prefers-reduced-motion: no-preference) {\n  #page-action-buttons > .urlbar-page-action {\n    transition: margin-inline-end 50ms var(--animation-easing-function) 900ms,\n      opacity 1.5s var(--animation-easing-function) 600ms !important;\n  }\n}\n\n/* leptonChrome.css:26133-26139 */\n@media (prefers-reduced-motion: no-preference) {\n  #urlbar-container:is(:hover, :focus-within) #page-action-buttons > .urlbar-page-action,\n  .urlbar-page-action[open],\n  .urlbar-page-action[open] ~ .urlbar-page-action {\n    transition-delay: 0s !important;\n  }\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/autohide-sidebar.css?inline
  var autohide_sidebar_default = '/* Keep the pre-existing standalone auto-hide setting for profiles that do not\n * enable overlay. The native overlay controller owns the combined mode. */\n:root:not([floorp-firefox-sidebar-overlay]) #sidebar-box {\n  min-width: var(--uc-sidebar-width) !important;\n  max-width: var(--uc-sidebar-width) !important;\n  position: relative !important;\n  z-index: var(--browser-area-z-index-sidebar-splitter, 3) !important;\n}\n:root:not([floorp-firefox-sidebar-overlay])[inFullscreen="true"] #sidebar-box {\n  min-width: var(--uc-sidebar-activate-width) !important;\n  max-width: var(--uc-sidebar-activate-width) !important;\n}\n:root:not([floorp-firefox-sidebar-overlay]) #sidebar-splitter {\n  display: none !important;\n}\n/* Hover keeps the selected panel loaded and reveals it through a separate\n * accessible edge control. Focus, menus, and resizing retain the open panel. */\n:root[floorp-firefox-sidebar-hover] #sidebar-box:not([data-floorp-sidebar-expanded]) {\n  visibility: hidden !important;\n  pointer-events: none !important;\n}\n\n#floorp-firefox-sidebar-edge-toggle {\n  position: absolute;\n  left: var(--floorp-firefox-sidebar-trigger-left, 0px);\n  top: calc(var(--floorp-firefox-sidebar-top, 0px) + 30%);\n  width: 8px;\n  height: 72px;\n  margin: 0;\n  padding: 0;\n  border: none;\n  border-radius: 4px;\n  background: color-mix(in srgb, var(--toolbar-color, currentColor) 35%,\n    transparent);\n  color: var(--toolbar-color);\n  cursor: pointer;\n  z-index: 8;\n}\n#floorp-firefox-sidebar-edge-toggle:is(:hover, :focus-visible, [aria-pressed="true"]) {\n  background: var(--focus-outline-color);\n}\n#floorp-firefox-sidebar-edge-toggle:focus-visible {\n  outline: 2px solid var(--focus-outline-color);\n  outline-offset: 2px;\n}\n#floorp-firefox-sidebar-edge-toggle[hidden],\n:root[inDOMFullscreen] #floorp-firefox-sidebar-edge-toggle,\n:root[inDOMFullscreen] #floorp-firefox-sidebar-overlay-resizer {\n  display: none !important;\n}\n';

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/autohide-tab.css?inline
  var autohide_tab_default = "/* Floorp chrome-extras: autohideTab\n * Ported from vendored Lepton (leptonChrome.css 26140-26169, 3 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp's dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n *\n * Only the `margin-bottom` behaviour is ported. The `autohide.tab.opacity`\n *\n * and `autohide.tab.blur` variants are separate Lepton prefs that Floorp\n *\n * does not expose.\n */\n\n/* leptonChrome.css:26140-26144 */\n.tabbrowser-tab:not([multiselected]) .tab-content:not(:hover):not([selected]) {\n  margin-bottom: -64px;\n}\n\n/* leptonChrome.css:26155-26164 */\n@media (prefers-reduced-motion: no-preference) {\n  .tabbrowser-tab:not([multiselected]) .tab-content:not(:hover):not([selected]) {\n    transition-timing-function: var(--animation-easing-function);\n    transition-duration: 0.2s;\n    transition-delay: 600ms;\n  }\n  .tabbrowser-tab:not([multiselected]) .tab-content:not(:hover):not([selected]):is(:hover, :focus-within) {\n    transition-delay: 0s;\n  }\n}\n\n/* leptonChrome.css:26165-26169 */\n@media (prefers-reduced-motion: no-preference) {\n  .tabbrowser-tab:not([multiselected]) .tab-content:not(:hover):not([selected]) {\n    transition-property: margin-bottom;\n  }\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/centered-bookmarkbar.css?inline
  var centered_bookmarkbar_default = "/* Floorp chrome-extras: centeredBookmarkbar\n * Ported from vendored Lepton (leptonChrome.css 26060-26065, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp's dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n */\n\n/* leptonChrome.css:26060-26065 */\n#PlacesToolbarItems {\n  display: flex !important;\n  justify-content: safe center !important;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/centered-tab.css?inline
  var centered_tab_default = "/* Floorp chrome-extras: centeredTab\n * Ported from vendored Lepton (leptonChrome.css 26021-26029, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp's dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n *\n * The `centered.tab.label`, `tab.close_button_at_hover` and `counter.tab`\n *\n * variants are Lepton-only sub-options and are not ported.\n */\n\n/* leptonChrome.css:26021-26029 */\n.tab-icon-stack {\n  margin-inline-start: auto;\n}\n.tab-label-container {\n  max-width: min-content;\n  margin-inline-end: auto;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/centered-urlbar.css?inline
  var centered_urlbar_default = "/* Floorp chrome-extras: centeredUrlbar\n * Ported from vendored Lepton (leptonChrome.css 26053-26058, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp's dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n */\n\n/* leptonChrome.css:26053-26058 */\n#urlbar:not([focused]) #urlbar-input,\n#urlbar:not([focused]) .urlbar-input {\n  text-align: center !important;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-bookmarkbar-icon.css?inline
  var hidden_bookmarkbar_icon_default = "/* Floorp chrome-extras: hiddenBookmarkbarIcon\n * Ported from vendored Lepton (leptonChrome.css 27224-27228, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp's dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n */\n\n/* leptonChrome.css:27224-27228 */\n#PlacesToolbarItems .bookmark-item > .toolbarbutton-icon {\n  display: none;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-bookmarkbar-label.css?inline
  var hidden_bookmarkbar_label_default = "/* Floorp chrome-extras: hiddenBookmarkbarLabel\n * Ported from vendored Lepton (leptonChrome.css 27229-27233, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp's dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n */\n\n/* leptonChrome.css:27229-27233 */\n#PlacesToolbarItems .bookmark-item > .toolbarbutton-text {\n  visibility: collapse; /* display: none is not work */\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-disabled-menu.css?inline
  var hidden_disabled_menu_default = '/* Floorp chrome-extras: hiddenDisabledMenu\n * Ported from vendored Lepton (leptonChrome.css 27235-27247, 2 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp\'s dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n *\n * The `widget.*.native-context-menus` branches are kept verbatim \u2014 those\n *\n * are genuine Gecko prefs, not Lepton toggles.\n */\n\n/* leptonChrome.css:27235-27240 */\nmenu[disabled="true"],\nmenuitem:not(#context-back, #context-forward)[disabled="true"] {\n  visibility: collapse !important;\n}\n\n/* leptonChrome.css:27241-27247 */\n@media (-moz-pref("widget.macos.native-context-menus")), (-moz-pref("widget.gtk.native-context-menus")) {\n  #context-back[disabled="true"],\n  #context-forward[disabled="true"] {\n    visibility: collapse !important;\n  }\n}\n';

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-navbar.css?inline
  var hidden_navbar_default = "/* Floorp chrome-extras: hiddenNavbar\n * Ported from vendored Lepton (leptonChrome.css 27141-27145, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp's dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n */\n\n/* leptonChrome.css:27141-27145 */\n#nav-bar {\n  visibility: collapse; /* display: none is not work */\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-sidebar-header.css?inline
  var hidden_sidebar_header_default = "/* Floorp chrome-extras: hiddenSidebarHeader\n * Ported from vendored Lepton (leptonChrome.css 27177-27181, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp's dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n *\n * The `hidden.sidebar_header.vertical_tab_only` variant is not ported\n *\n * (Lepton-only sub-option).\n */\n\n/* leptonChrome.css:27177-27181 */\n#sidebar-header {\n  display: none !important;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-tab-attention-indicator.css?inline
  var hidden_tab_attention_indicator_default = "/* This Source Code Form is subject to the terms of the Mozilla Public\n * License, v. 2.0. If a copy of the MPL was not distributed with this\n * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n\n/* Floorp chrome-extras: hiddenTabAttentionIndicator. Removes the dot on tabs\n * that want attention and the bold title on unpinned ones. */\n#tabbrowser-tabs .tab-content:is([attention], [titlechanged]) {\n  background-image: none !important;\n}\n#tabbrowser-tabs .tab-label[attention] {\n  font-weight: inherit !important;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-tab-audio-indicator.css?inline
  var hidden_tab_audio_indicator_default = "/* This Source Code Form is subject to the terms of the Mozilla Public\n * License, v. 2.0. If a copy of the MPL was not distributed with this\n * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n\n/* Floorp chrome-extras: hiddenTabAudioIndicator. Keeps the crashed-tab badge.\n * The ID outranks Lepton's `!important` audio button rule. */\n#tabbrowser-tabs .tab-audio-button:not([crashed]),\n#tabbrowser-tabs .tab-icon-overlay:not([crashed]):is([soundplaying], [muted], [activemedia-blocked]) {\n  display: none !important;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-tab-close-button.css?inline
  var hidden_tab_close_button_default = "/* This Source Code Form is subject to the terms of the Mozilla Public\n * License, v. 2.0. If a copy of the MPL was not distributed with this\n * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n\n/* Floorp chrome-extras: hiddenTabCloseButton. Lepton's clipped-tab hover\n * rule uses two IDs and !important. Include both native tab containers under\n * the browser window so this wins in horizontal and vertical layouts. */\n#tabbrowser-tabs .tab-close-button,\n#main-window #tabbrowser-tabs :is(#tabbrowser-arrowscrollbox, #pinned-tabs-container) .tab-close-button {\n  display: none !important;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-tab-icon.css?inline
  var hidden_tab_icon_default = '/* Floorp chrome-extras: hiddenTabIcon\n * Ported from vendored Lepton (leptonChrome.css 27156-27160, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp\'s dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n *\n * The `hidden.tab_icon.always` variant (hides .tab-icon-stack instead of\n *\n * .tab-icon-image) is not ported (Lepton-only sub-option).\n */\n\n/* leptonChrome.css:27156-27160 */\n.tabbrowser-tab:not([pinned="true"]) .tab-icon-image {\n  display: none;\n}\n';

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-tab-note-icon.css?inline
  var hidden_tab_note_icon_default = "/* This Source Code Form is subject to the terms of the Mozilla Public\n * License, v. 2.0. If a copy of the MPL was not distributed with this\n * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n\n/* Floorp chrome-extras: hiddenTabNoteIcon */\n#tabbrowser-tabs :is(.tab-note-icon, .tab-note-icon-overlay) {\n  display: none !important;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-tab-sharing-indicator.css?inline
  var hidden_tab_sharing_indicator_default = "/* This Source Code Form is subject to the terms of the Mozilla Public\n * License, v. 2.0. If a copy of the MPL was not distributed with this\n * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n\n/* Floorp chrome-extras: hiddenTabSharingIndicator. The address bar still\n * shows camera, microphone, and screen sharing. */\n#tabbrowser-tabs .tab-sharing-icon-overlay {\n  display: none !important;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-tabbar.css?inline
  var hidden_tabbar_default = "/* Floorp chrome-extras: hiddenTabbar\n * Ported from vendored Lepton (leptonChrome.css 27136-27140, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp's dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n */\n\n/* leptonChrome.css:27136-27140 */\n#TabsToolbar {\n  display: none !important;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/hidden-urlbar-iconbox.css?inline
  var hidden_urlbar_iconbox_default = '/* Floorp chrome-extras: hiddenUrlbarIconbox\n * Ported from vendored Lepton (leptonChrome.css 27192-27201, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp\'s dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n *\n * The `hidden.urlbar_iconbox.label_only` variant is not ported\n *\n * (Lepton-only sub-option).\n */\n\n/* leptonChrome.css:27192-27201 */\n#identity-box[pageproxystate="valid"].notSecureText #identity-icon-label,\n#identity-box[pageproxystate="valid"].chromeUI #identity-icon-box,\n#identity-box[pageproxystate="valid"].extensionPage #identity-icon-box {\n  display: none;\n}\n#identity-box[pageproxystate="valid"].notSecureText #identity-icon-box {\n  padding-inline-end: 5px; /* Original: 8px */\n}\n';

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/icon-disabled.css?inline
  var icon_disabled_default = '/* Floorp chrome-extras: iconDisabled\n * Ported from vendored Lepton (leptonChrome.css 27249-31348, 153 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp\'s dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n *\n * This sheet is applied while icons are ENABLED, i.e. when the toggle is\n *\n * OFF \u2014 Lepton\'s gate was `@media not (-moz-bool-pref:\n *\n * "userChrome.icon.disabled")`. The `icon.library` and\n *\n * `tab.connect_to_window` variants are Lepton-only sub-options and are not\n *\n * ported.\n *\n *\n *\n * `icon.panel` blocks ARE kept: they carry the Floorp-only panel menu IDs\n *\n * (`#rebootappmenu`, `#appMenu-ssb-button`, ...) that used to live in\n *\n * `FLOORP_ICON_PATCHES` and would otherwise be lost.\n */\n\n/* leptonChrome.css:27249-27274 */\n/** Icons - List **************************************************************/\n:root {\n  --uc-folder-icon: url("chrome://global/skin/icons/folder.svg");\n  --uc-tab-icon: url("../icons/tab.svg");\n  --uc-tab-copy-icon: url("../icons/tab-copy.svg");\n  --uc-tab-unload-icon: url("../icons/tab-unload.svg");\n  --uc-tab-multiple-icon: url("../icons/tab-desktop-multiple.svg");\n  --uc-tab-close-icon: url("../icons/close-tab.svg");\n  --uc-tab-close-duplicate-icon: url("../icons/close-duplicate-tabs.svg");\n  --uc-new-tab-icon: url("../icons/new-tab.svg");\n  --uc-new-tab-forward-icon: url("../icons/new-tab-forward.svg");\n  --uc-new-tab-skip-forward-icon: url("../icons/new-tab-skip-forward.svg");\n  --uc-new-tab-multiple-icon: url("../icons/new-tab-multiple.svg");\n  --uc-sidebar-icon: url("chrome://browser/skin/sidebars.svg");\n  --uc-sidebar-icon-reverse: url("chrome://browser/skin/sidebars-right.svg");\n}\n/** Library - Icons Replace ***************************************************/\n/** Panel - Icons *************************************************************/\n/** Menu - Icons Layout *******************************************************/\n/** Icons - 1.25px Stroke Width ***********************************************/\n/** Icons for fork browsers ***************************************************/\n/*= Waterfox =================================================================*/\n/*= Tor Browser ==============================================================*/\n/*= Floorp Browser ===========================================================*/\n\n/* leptonChrome.css:27293-27300 */\n@supports selector(:has(a)) {\n  :root:has(#sidebar-box:is([positionend], [sidebar-positionend])) {\n    --uc-sidebar-icon: url("chrome://browser/skin/sidebars-right.svg");\n    --uc-sidebar-icon-reverse: url("chrome://browser/skin/sidebars.svg");\n  }\n}\n\n/* leptonChrome.css:27301-27305 */\n#TabsToolbar #new-tab-button {\n  --uc-new-tab-icon: url(chrome://global/skin/icons/plus.svg);\n}\n\n/* leptonChrome.css:27306-27311 */\n.urlbarView-row[source="tabs"] > .urlbarView-row-inner > .urlbarView-no-wrap > .urlbarView-favicon,\n#urlbar-engine-one-off-item-tabs {\n  list-style-image: var(--uc-tab-icon) !important;\n}\n\n/* leptonChrome.css:27484-27530 \u2014 extra conditions dropped: userChrome.icon.panel */\n/*= Padding ==================================================================*/\n:root {\n  --arrowpanel-menuicon-padding: 8px;\n  --arrowpanel-menublank-padding: calc(\n    var(--arrowpanel-menuicon-padding) * 2 + var(--arrowpanel-menuitem-padding-inline)\n  ) !important;\n  --arrowpanel-menuimageblank-padding: calc(var(--arrowpanel-menuitem-padding-inline) - 2px) !important;\n  --arrowpanel-menuimageblank-padding-block: calc(var(--arrowpanel-menuitem-padding-block) - 2px) !important;\n}\n.subviewbutton > .toolbarbutton-icon {\n  width: 16px;\n}\n/* Icons Color */\n/*= Panel - Main =============================================================*/\n/*= Panel - Account ==========================================================*/\n/* Default */\n/* Change Separator */\n/*= Panel - Bookmark =========================================================*/\n/*= Panel - History ==========================================================*/\n/*= Panel - More tools =======================================================*/\n/* Web Developer Tools */\n/* Task Manager */\n/* Remote Debugging - Edge bug.svg */\n/* Browser Toolbox - Edge webdeveloper.svg */\n/* Browser Content Toolbox */\n/* Browser Console */\n/* Responsive Design Mode */\n/* Eyedropper */\n/* Page Source - Edge file-search.svg */\n/* Extensions for Devel */\n/*= Panel - Help =============================================================*/\n/*= Panel - Library ==========================================================*/\n/*= Panel - Downloads ========================================================*/\n/*= Toolbar - Overflow Menu ==================================================*/\n/*= Tabbar - All Tab Menu ====================================================*/\n/*= BMB_bookmarksPopup =======================================================*/\n/* BMB_bookmarksToolbarPopup */\n/*= protections-popup ========================================================*/\n/*= identity-popup ===========================================================*/\n/*= sidebarMenu-popup ========================================================*/\n/*= sidebar-context-menu =====================================================*/\n/*= chrome://browser/content/syncedtabs/sidebar.xhtml ========================*/\n/*= unified-extensions-view ===================================================*/\n/*= Compatibility ============================================================*/\n/*= Tab Mix Plus =============================================================*/\n\n/* leptonChrome.css:27531-27544 \u2014 extra conditions dropped: userChrome.icon.panel */\n:root {\n  /* Global */\n  --arrowpanel-menuicon-paddingx2: calc(var(--arrowpanel-menuicon-padding) * 2);\n  /* General Panel */\n  --arrowpanel-menublank-padding: calc(var(--arrowpanel-menuicon-paddingx2) + 8px) !important;\n  --arrowpanel-menuimageblank-padding: calc(var(--arrowpanel-menuitem-padding-inline) - 2px) !important;\n  /* Blank Menu Left Padding */\n  --arrowpanel-menuimageblank-padding-horizontal: calc(\n    var(--arrowpanel-menuicon-paddingx2) + var(--arrowpanel-menuitem-padding-inline)\n  );\n  --arrowpanel-menuitemblank-padding: calc(var(--arrowpanel-menuitem-padding-block) + 1px);\n}\n\n/* leptonChrome.css:27545-27550 \u2014 extra conditions dropped: userChrome.icon.panel */\n#downloadsHistory .button-text,\n.subviewbutton > .toolbarbutton-text {\n  padding-inline-start: var(--arrowpanel-menuicon-padding) !important;\n}\n\n/* leptonChrome.css:27551-27557 \u2014 extra conditions dropped: userChrome.icon.panel */\n.toolbaritem-combined-buttons:not(.unified-extensions-item)\n  > .subviewbutton:not(.subviewbutton-iconic)\n  > .toolbarbutton-text {\n  padding-inline-start: 0 !important;\n}\n\n/* leptonChrome.css:27558-27564 \u2014 extra conditions dropped: userChrome.icon.panel */\n#panelMenu_bookmarksMenu .subviewbutton[disabled="true"],\n#appMenu_historyMenu .subviewbutton[disabled="true"] {\n  margin-inline-start: 0 !important;\n  padding-inline-start: 0 !important;\n}\n\n/* leptonChrome.css:27565-27570 \u2014 extra conditions dropped: userChrome.icon.panel */\n#panelMenu_bookmarksMenu .subviewbutton[disabled="true"] .toolbarbutton-text,\n#appMenu_historyMenu .subviewbutton[disabled="true"] .toolbarbutton-text {\n  padding-inline-start: var(--arrowpanel-menublank-padding) !important;\n}\n\n/* leptonChrome.css:27571-27576 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-proton-update-banner .toolbarbutton-text {\n  margin-inline-start: 0 !important;\n  padding-inline-start: 0 !important; /* FF v107 */\n}\n\n/* leptonChrome.css:27577-27586 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-multiView .subviewbutton::before,\n#appMenu-proton-update-banner::before {\n  display: inline-flex;\n  display: -moz-inline-box;\n  margin-inline-end: var(--arrowpanel-menuicon-padding);\n  width: 16px;\n  height: 16px;\n}\n\n/* leptonChrome.css:27587-27592 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-proton-update-banner {\n  margin-bottom: 2px !important;\n  padding-inline-start: var(--arrowpanel-menuitem-padding-inline) !important;\n}\n\n/* leptonChrome.css:27593-27601 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-fxa-status2,\n#appMenu-zoom-controls2,\n#appMenu-zoom-controls {\n  align-items: center;\n  padding-top: var(--arrowpanel-menuimageblank-padding-block) !important;\n  padding-bottom: var(--arrowpanel-menuimageblank-padding-block) !important;\n}\n\n/* leptonChrome.css:27602-27607 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-zoom-controls2::before,\n#appMenu-zoom-controls::before {\n  margin-inline-end: 0 !important;\n}\n\n/* leptonChrome.css:27608-27615 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-zoomReduce-button2,\n#appMenu-zoomReset-button2,\n#appMenu-zoomEnlarge-button2,\n#appMenu-fullscreen-button2 {\n  --arrowpanel-menuitem-padding-block: 0px;\n}\n\n/* leptonChrome.css:27616-27620 \u2014 extra conditions dropped: userChrome.icon.panel */\n.subviewbutton[type="checkbox"]:not([checked="true"], #allTabsMenu_sortTabsButton) > .toolbarbutton-text {\n  margin-left: 16px !important;\n}\n\n/* leptonChrome.css:27621-27630 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-multiView .subviewbutton::before,\n#appMenu-proton-update-banner::before,\n#downloadsHistory .button-icon,\n.subviewbutton:not(#appMenu-proton-update-banner) > image {\n  fill: currentColor !important;\n  fill-opacity: var(--toolbarbutton-icon-fill-opacity) !important;\n  -moz-context-properties: fill, fill-opacity, stroke !important;\n}\n\n/* leptonChrome.css:27631-27636 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-zoomReduce-button2 > .toolbarbutton-icon,\n#appMenu-zoomEnlarge-button2 > .toolbarbutton-icon {\n  stroke: var(--zoom-controls-bgcolor, var(--button-bgcolor, ButtonFace)) !important;\n}\n\n/* leptonChrome.css:27637-27642 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-zoomReduce-button2:not([disabled], [open], :active):is(:hover) > .toolbarbutton-icon,\n#appMenu-zoomEnlarge-button2:not([disabled], [open], :active):is(:hover) > .toolbarbutton-icon {\n  stroke: var(--button-hover-bgcolor) !important;\n}\n\n/* leptonChrome.css:27643-27648 \u2014 extra conditions dropped: userChrome.icon.panel */\n.subviewbutton[disabled="true"] > image {\n  /* Ghost icons when disabled */\n  opacity: 0.4;\n}\n\n/* leptonChrome.css:27649-27657 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-proton-addon-banners > .addon-banner-item > .toolbarbutton-icon {\n  display: inline-flex !important;\n  display: -moz-inline-box !important;\n  margin-inline-start: var(--arrowpanel-menuicon-padding);\n  order: -1 !important;\n  -moz-box-ordinal-group: 0 !important;\n}\n\n/* leptonChrome.css:27658-27662 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-addon-banners > .addon-banner-item > .toolbarbutton-icon {\n  display: inline-flex !important;\n}\n\n/* leptonChrome.css:27663-27669 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-update-banner::before,\n#appMenu-proton-update-banner::before {\n  content: url("../icons/whatsnew.svg");\n  margin-inline-end: 0px !important;\n}\n\n/* leptonChrome.css:27670-27675 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-fxa-status2::before {\n  /* Don\'t exist img tag */\n  content: url("chrome://browser/skin/fxa/avatar-empty.svg");\n}\n\n/* leptonChrome.css:27676-27680 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-fxa-status2:is([fxastatus="signedin"], [fxastatus="unverified"], [fxastatus="login-failed"])::before {\n  display: none;\n}\n\n/* leptonChrome.css:27681-27690 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-fxa-status2:is([fxastatus="signedin"], [fxastatus="unverified"], [fxastatus="login-failed"])\n  #appMenu-fxa-label2::before {\n  /* url("https://profile.accounts.firefox.com/v1/avatar/a") */\n  content: "";\n  border-radius: 50% !important;\n  background-size: 16px !important;\n  background-image: var(--avatar-image-url) !important;\n}\n\n/* leptonChrome.css:27691-27696 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-profiles-button > image,\n#appMenu-empty-profiles-button > image {\n  fill: transparent !important;\n}\n\n/* leptonChrome.css:27697-27702 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-empty-profiles-button,\n#PanelUI-fxa-menu-empty-profiles-button {\n  list-style-image: url("chrome://browser/content/profiles/assets/16_heart.svg");\n}\n\n/* leptonChrome.css:27703-27707 \u2014 extra conditions dropped: userChrome.icon.panel */\n#profiles-manage-profiles-button {\n  list-style-image: url("chrome://global/skin/icons/settings.svg");\n}\n\n/* leptonChrome.css:27708-27712 \u2014 extra conditions dropped: userChrome.icon.panel */\n#new-tab-button {\n  list-style-image: var(--uc-new-tab-icon) !important;\n}\n\n/* leptonChrome.css:27731-27747 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-new-tab-button2,\n#appMenu-passwords-button,\n#appMenu-extensions-themes-button,\n#appMenu-save-file-button2,\n#appMenu-find-button2,\n#appMenu-more-button2,\n#appMenu-help-button2,\n#appMenu-quit-button2 {\n  padding-top: var(--arrowpanel-menuitemblank-padding-block) !important;\n  padding-bottom: var(--arrowpanel-menuitemblank-padding-block) !important;\n}\n#appMenu-zoom-controls2,\n#appMenu-zoom-controls {\n  padding-inline-start: var(--arrowpanel-menuimageblank-padding-horizontal) !important;\n}\n\n/* leptonChrome.css:27748-27752 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-new-window-button2 {\n  list-style-image: url("chrome://browser/skin/window.svg");\n}\n\n/* leptonChrome.css:27753-27757 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-new-private-window-button2 {\n  list-style-image: url("chrome://browser/skin/privateBrowsing.svg");\n}\n\n/* leptonChrome.css:27758-27762 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-bookmarks-button {\n  list-style-image: url("chrome://browser/skin/bookmark.svg");\n}\n\n/* leptonChrome.css:27763-27767 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-history-button {\n  list-style-image: url("chrome://browser/skin/history.svg");\n}\n\n/* leptonChrome.css:27768-27772 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-downloads-button {\n  list-style-image: url("chrome://browser/skin/downloads/downloads.svg");\n}\n\n/* leptonChrome.css:27782-27786 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-print-button2 {\n  list-style-image: url("chrome://global/skin/icons/print.svg");\n}\n\n/* leptonChrome.css:27802-27806 \u2014 extra conditions dropped: userChrome.icon.panel */\n#save-page-button {\n  list-style-image: url("../icons/toolbarButton-download.svg") !important;\n}\n\n/* leptonChrome.css:27807-27811 \u2014 extra conditions dropped: userChrome.icon.panel */\n#open-file-button {\n  list-style-image: url("../icons/toolbarButton-upload.svg") !important;\n}\n\n/* leptonChrome.css:27812-27816 \u2014 extra conditions dropped: userChrome.icon.panel */\n#cut-button {\n  list-style-image: url("../icons/edit-cut.svg") !important;\n}\n\n/* leptonChrome.css:27817-27821 \u2014 extra conditions dropped: userChrome.icon.panel */\n#copy-button {\n  list-style-image: url("../icons/edit-copy.svg") !important;\n}\n\n/* leptonChrome.css:27822-27826 \u2014 extra conditions dropped: userChrome.icon.panel */\n#paste-button {\n  list-style-image: url("../icons/edit-paste.svg") !important;\n}\n\n/* leptonChrome.css:27827-27831 \u2014 extra conditions dropped: userChrome.icon.panel */\n#characterencoding-button {\n  list-style-image: url("../icons/characterEncoding.svg") !important;\n}\n\n/* leptonChrome.css:27832-27836 \u2014 extra conditions dropped: userChrome.icon.panel */\n#screenshot-button {\n  list-style-image: url("../icons/screenshot-1.svg") !important;\n}\n\n/* leptonChrome.css:27837-27841 \u2014 extra conditions dropped: userChrome.icon.panel */\n#email-link-button {\n  list-style-image: url("../icons/mail.svg") !important;\n}\n\n/* leptonChrome.css:27842-27846 \u2014 extra conditions dropped: userChrome.icon.panel */\n#panic-button {\n  list-style-image: url("../icons/forget.svg") !important;\n}\n\n/* leptonChrome.css:27847-27851 \u2014 extra conditions dropped: userChrome.icon.panel */\n#profiler-button-button > .toolbarbutton-icon {\n  list-style-image: url("../icons/performance.svg") !important;\n}\n\n/* leptonChrome.css:27852-27856 \u2014 extra conditions dropped: userChrome.icon.panel */\n#developer-button {\n  list-style-image: url("../icons/wrench-filled.svg") !important;\n}\n\n/* leptonChrome.css:27857-27861 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-settings-button {\n  list-style-image: url("chrome://global/skin/icons/settings.svg");\n}\n\n/* leptonChrome.css:27876-27882 \u2014 extra conditions dropped: userChrome.icon.panel */\n#PanelUI-fxa-menu-connect-device-button .toolbarbutton-icon,\n#PanelUI-fxa-menu-account-signout-button .toolbarbutton-icon {\n  width: 16px !important;\n  height: 16px !important;\n}\n\n/* leptonChrome.css:27883-27895 \u2014 extra conditions dropped: userChrome.icon.panel */\n#fxa-manage-account-button::before {\n  content: "";\n  display: inline-flex;\n  display: -moz-inline-box;\n  width: 32px !important;\n  height: 32px !important;\n  border-radius: 50%;\n  background-size: 32px;\n  background-image: var(--avatar-image-url);\n  margin-inline-end: var(--arrowpanel-menuicon-padding);\n}\n\n/* leptonChrome.css:27908-27915 \u2014 extra conditions dropped: userChrome.icon.panel */\n.syncNowBtn {\n  visibility: visible !important;\n  order: -1 !important;\n  -moz-box-ordinal-group: 0 !important;\n  margin-inline-end: var(--arrowpanel-menuicon-padding);\n}\n\n/* leptonChrome.css:27916-27920 \u2014 extra conditions dropped: userChrome.icon.panel */\n#PanelUI-fxa-menu-setup-sync-button {\n  list-style-image: url("chrome://browser/skin/sync.svg");\n}\n\n/* leptonChrome.css:27921-27927 \u2014 extra conditions dropped: userChrome.icon.panel */\n@media (prefers-reduced-motion: reduce) {\n  #PanelUI-fxa-menu-syncnow-button[syncstatus="active"] > .toolbarbutton-icon,\n  #PanelUI-remotetabs-syncnow[syncstatus="active"] > .toolbarbutton-icon,\n  .syncNowBtn[syncstatus="active"] {\n    list-style-image: url("chrome://browser/skin/tabbrowser/hourglass.svg") !important;\n  }\n}\n\n/* leptonChrome.css:27928-27932 \u2014 extra conditions dropped: userChrome.icon.panel */\n#PanelUI-fxa-menu-connect-device-button {\n  list-style-image: url("../icons/add-device.svg");\n}\n\n/* leptonChrome.css:27933-27937 \u2014 extra conditions dropped: userChrome.icon.panel */\n#PanelUI-fxa-menu-sendtab-button {\n  list-style-image: url("../icons/send-to-device.svg");\n}\n\n/* leptonChrome.css:27938-27942 \u2014 extra conditions dropped: userChrome.icon.panel */\n#PanelUI-fxa-menu-sync-prefs-button {\n  list-style-image: url("chrome://global/skin/icons/settings.svg");\n}\n\n/* leptonChrome.css:27943-27947 \u2014 extra conditions dropped: userChrome.icon.panel */\n#PanelUI-fxa-menu-account-signout-button {\n  list-style-image: url("../icons/sign-out.svg");\n}\n\n/* leptonChrome.css:27948-27964 \u2014 extra conditions dropped: userChrome.icon.panel */\n#PanelUI-remotetabs-view-managedevices::before {\n  /* Box */\n  content: "";\n  padding-inline-end: 16px;\n  padding-block: 1px;\n  margin-inline-end: var(--arrowpanel-menuicon-padding);\n  /* Color */\n  fill: currentColor;\n  fill-opacity: var(--toolbarbutton-icon-fill-opacity);\n  -moz-context-properties: fill, fill-opacity, stroke;\n  background-size: 16px;\n  background-repeat: no-repeat;\n  background-position: left center;\n  background-image: url("chrome://global/skin/icons/settings.svg");\n}\n\n/* leptonChrome.css:27965-27972 \u2014 extra conditions dropped: userChrome.icon.panel */\n.PanelUI-remotetabs-notabsforclient-label {\n  margin-inline-start: calc(\n    var(--arrowpanel-menuicon-padding) + var(--arrowpanel-menuitem-padding-inline)\n  ) !important;\n  padding-inline-start: var(--arrowpanel-menublank-padding) !important;\n}\n\n/* leptonChrome.css:27973-27982 \u2014 extra conditions dropped: userChrome.icon.panel */\n#PanelUI-fxa-menu::before {\n  content: "";\n  display: flex;\n  display: -moz-box;\n  border-bottom: 1px solid var(--panel-separator-color);\n  margin: var(--panel-separator-margin);\n  padding: 0;\n}\n\n/* leptonChrome.css:27983-27987 \u2014 extra conditions dropped: userChrome.icon.panel */\n#PanelUI-signedin-panel[hidden="true"] + #PanelUI-fxa-cta-menu #PanelUI-products-separator {\n  display: none;\n}\n\n/* leptonChrome.css:27988-27994 \u2014 extra conditions dropped: userChrome.icon.panel */\n#PanelUI-fxa-menu > :first-child,\n#fxa-manage-account-button {\n  order: -1;\n  -moz-box-ordinal-group: 0;\n}\n\n/* leptonChrome.css:27995-27999 \u2014 extra conditions dropped: userChrome.icon.panel */\n#PanelUI-sign-out-separator {\n  display: none;\n}\n\n/* leptonChrome.css:28000-28005 \u2014 extra conditions dropped: userChrome.icon.panel */\n.pageAction-sendToDevice-device.subviewbutton.sync-menuitem.sendtab-target[clientType=""],\n.sendToDevice-device.subviewbutton.sync-menuitem.sendtab-target[clientType=""] {\n  list-style-image: url("../icons/send-to-device.svg");\n}\n\n/* leptonChrome.css:28006-28011 \u2014 extra conditions dropped: userChrome.icon.panel */\n.pageAction-sendToDevice-device.subviewbutton.sync-menuitem.sendtab-target:not([clientType]),\n.sendToDevice-device.subviewbutton.sync-menuitem.sendtab-target:not([clientType]) {\n  list-style-image: url("chrome://global/skin/icons/settings.svg");\n}\n\n/* leptonChrome.css:28012-28028 \u2014 extra conditions dropped: userChrome.icon.panel */\n.sendToDevice-device[clientType="phone"] {\n  list-style-image: url("../icons/device-phone.svg") !important;\n}\n.sendToDevice-device[clientType="tablet"] {\n  list-style-image: url("../icons/device-tablet.svg") !important;\n}\n.sendToDevice-device[clientType="desktop"] {\n  list-style-image: url("../icons/device-desktop.svg") !important;\n}\n.sendToDevice-device[clientType="tv"] {\n  list-style-image: url("../icons/device-tv.svg") !important;\n}\n.sendToDevice-device[clientType="vr"] {\n  list-style-image: url("../icons/device-vr.svg") !important;\n}\n\n/* leptonChrome.css:28029-28033 \u2014 extra conditions dropped: userChrome.icon.panel */\n#panelMenuBookmarkThisPage {\n  list-style-image: url("chrome://browser/skin/bookmark-hollow.svg");\n}\n\n/* leptonChrome.css:28034-28038 \u2014 extra conditions dropped: userChrome.icon.panel */\npanelMenuBookmarkThisPage[starred] {\n  list-style-image: url("chrome://browser/skin/bookmark.svg");\n}\n\n/* leptonChrome.css:28039-28043 \u2014 extra conditions dropped: userChrome.icon.panel */\n#panelMenu_searchBookmarks {\n  list-style-image: url("chrome://global/skin/icons/search-glass.svg");\n}\n\n/* leptonChrome.css:28044-28048 \u2014 extra conditions dropped: userChrome.icon.panel */\n#panelMenu_viewBookmarksToolbar {\n  list-style-image: url("../icons/bookmarks-toolbar-alt.svg");\n}\n\n/* leptonChrome.css:28049-28053 \u2014 extra conditions dropped: userChrome.icon.panel */\n#panelMenu_showAllBookmarks {\n  list-style-image: url("chrome://browser/skin/bookmark-star-on-tray.svg");\n}\n\n/* leptonChrome.css:28054-28058 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenuRecentlyClosedTabs {\n  list-style-image: var(--uc-tab-icon);\n}\n\n/* leptonChrome.css:28059-28063 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenuRecentlyClosedWindows {\n  list-style-image: url("chrome://browser/skin/window.svg");\n}\n\n/* leptonChrome.css:28064-28068 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenuSearchHistory {\n  list-style-image: url("chrome://global/skin/icons/search-glass.svg");\n}\n\n/* leptonChrome.css:28069-28074 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenuRestoreSession,\n#appMenu-restoreSession {\n  list-style-image: url("../icons/restore-session.svg");\n}\n\n/* leptonChrome.css:28075-28079 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenuClearRecentHistory {\n  list-style-image: url("../icons/forget.svg");\n}\n\n/* leptonChrome.css:28080-28084 \u2014 extra conditions dropped: userChrome.icon.panel */\n#PanelUI-historyMore {\n  list-style-image: url("chrome://browser/skin/history.svg");\n}\n\n/* leptonChrome.css:28085-28089 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-library-recentlyClosedTabs {\n  list-style-image: url("../icons/movetowindow-16.svg");\n}\n\n/* leptonChrome.css:28090-28094 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-library-recentlyClosedWindows {\n  list-style-image: url("../icons/restore-session.svg");\n}\n\n/* leptonChrome.css:28095-28099 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appmenu-moreTools-button {\n  list-style-image: url("chrome://browser/skin/customize.svg");\n}\n\n/* leptonChrome.css:28100-28105 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appmenu-developer-tools-view .subviewbutton:nth-child(1),\n#PanelUI-developer-tools-view .subviewbutton:nth-child(1) {\n  list-style-image: url("../icons/developer.svg");\n}\n\n/* leptonChrome.css:28106-28111 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appmenu-developer-tools-view .subviewbutton:nth-child(2),\n#PanelUI-developer-tools-view .subviewbutton:nth-child(2) {\n  list-style-image: url("../icons/performance.svg");\n}\n\n/* leptonChrome.css:28112-28117 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appmenu-developer-tools-view .subviewbutton:nth-child(3),\n#PanelUI-developer-tools-view .subviewbutton:nth-child(3) {\n  list-style-image: url("../icons/bug.svg");\n}\n\n/* leptonChrome.css:28118-28123 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appmenu-developer-tools-view .subviewbutton:nth-child(4),\n#PanelUI-developer-tools-view .subviewbutton:nth-child(4) {\n  list-style-image: url("../icons/window-dev-tools.svg");\n}\n\n/* leptonChrome.css:28124-28129 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appmenu-developer-tools-view .subviewbutton:nth-child(5),\n#PanelUI-developer-tools-view .subviewbutton:nth-child(5) {\n  list-style-image: url("../icons/command-frames.svg");\n}\n\n/* leptonChrome.css:28130-28135 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appmenu-developer-tools-view .subviewbutton:nth-last-child(5),\n#PanelUI-developer-tools-view .subviewbutton:nth-last-child(5) {\n  list-style-image: url("../icons/command-console.svg");\n}\n\n/* leptonChrome.css:28136-28141 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appmenu-developer-tools-view .subviewbutton:nth-last-child(4),\n#PanelUI-developer-tools-view .subviewbutton:nth-last-child(4) {\n  list-style-image: url("../icons/command-responsivemode.svg");\n}\n\n/* leptonChrome.css:28142-28147 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appmenu-developer-tools-view .subviewbutton:nth-last-child(3),\n#PanelUI-developer-tools-view .subviewbutton:nth-last-child(3) {\n  list-style-image: url("../icons/command-eyedropper.svg");\n}\n\n/* leptonChrome.css:28148-28153 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appmenu-developer-tools-view .subviewbutton:nth-last-child(2),\n#PanelUI-developer-tools-view .subviewbutton:nth-last-child(2) {\n  list-style-image: url("../icons/document-search.svg");\n}\n\n/* leptonChrome.css:28154-28159 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appmenu-developer-tools-view .subviewbutton:nth-last-child(1),\n#PanelUI-developer-tools-view .subviewbutton:nth-last-child(1) {\n  list-style-image: url("chrome://mozapps/skin/extensions/extension.svg");\n}\n\n/* leptonChrome.css:28160-28164 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appmenu-developer-tools-view .subviewbutton:last-child {\n  margin-bottom: 6px !important;\n}\n\n/* leptonChrome.css:28165-28169 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu_menu_openHelp {\n  list-style-image: url("chrome://global/skin/icons/help.svg");\n}\n\n/* leptonChrome.css:28170-28174 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu_feedbackPage {\n  list-style-image: url("../icons/send.svg");\n}\n\n/* leptonChrome.css:28175-28179 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu_helpSafeMode {\n  list-style-image: url("chrome://devtools/skin/images/debugging-workers.svg");\n}\n\n/* leptonChrome.css:28180-28184 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu_troubleShooting {\n  list-style-image: url("chrome://global/skin/icons/more.svg");\n}\n\n/* leptonChrome.css:28185-28190 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-report-broken-site-button,\n#appMenu_help_reportSiteIssue {\n  list-style-image: url("chrome://global/skin/icons/lightbulb.svg");\n}\n\n/* leptonChrome.css:28191-28195 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu_menu_HelpPopup_reportPhishingtoolmenu {\n  list-style-image: url("chrome://global/skin/icons/warning.svg");\n}\n\n/* leptonChrome.css:28196-28200 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu_helpSwitchDevice {\n  list-style-image: url("../icons/add-device.svg");\n}\n\n/* leptonChrome.css:28201-28205 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu_aboutName {\n  list-style-image: url("chrome://global/skin/icons/info.svg");\n}\n\n/* leptonChrome.css:28206-28210 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-library-bookmarks-button {\n  list-style-image: url("chrome://browser/skin/bookmark.svg");\n}\n\n/* leptonChrome.css:28211-28215 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-library-history-button {\n  list-style-image: url("chrome://browser/skin/history.svg");\n}\n\n/* leptonChrome.css:28216-28220 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-library-downloads-button {\n  list-style-image: url("chrome://browser/skin/downloads/downloads.svg");\n}\n\n/* leptonChrome.css:28221-28225 \u2014 extra conditions dropped: userChrome.icon.panel */\n#downloadsHistory {\n  list-style-image: url("chrome://browser/skin/downloads/downloads.svg");\n}\n\n/* leptonChrome.css:28226-28231 \u2014 extra conditions dropped: userChrome.icon.panel */\n#downloadsHistory .box-inherit.button-box {\n  display: inline-flex !important;\n  display: -moz-inline-box !important;\n}\n\n/* leptonChrome.css:28232-28236 \u2014 extra conditions dropped: userChrome.icon.panel */\n#overflowMenu-customize-button {\n  list-style-image: url("chrome://browser/skin/customize.svg");\n}\n\n/* leptonChrome.css:28237-28241 \u2014 extra conditions dropped: userChrome.icon.panel */\n#allTabsMenu-undoCloseTab {\n  list-style-image: url("../icons/undo.svg");\n}\n\n/* leptonChrome.css:28242-28246 \u2014 extra conditions dropped: userChrome.icon.panel */\n#allTabsMenu-searchTabs {\n  list-style-image: url("chrome://global/skin/icons/search-glass.svg");\n}\n\n/* leptonChrome.css:28247-28251 \u2014 extra conditions dropped: userChrome.icon.panel */\n#allTabsMenu-syncedTabs {\n  list-style-image: url("../icons/synced-tabs.svg");\n}\n\n/* leptonChrome.css:28252-28256 \u2014 extra conditions dropped: userChrome.icon.panel */\n#allTabsMenu-closeDuplicateTabs {\n  list-style-image: var(--uc-tab-close-duplicate-icon);\n}\n\n/* leptonChrome.css:28257-28261 \u2014 extra conditions dropped: userChrome.icon.panel */\n#allTabsMenu-containerTabsButton {\n  list-style-image: url("../icons/container-openin-16.svg");\n}\n\n/* leptonChrome.css:28262-28266 \u2014 extra conditions dropped: userChrome.icon.panel */\n#allTabsMenu-hiddenTabsButton {\n  list-style-image: url("../icons/eye-hide.svg");\n}\n\n/* leptonChrome.css:28267-28271 \u2014 extra conditions dropped: userChrome.icon.panel */\n#allTabsMenu-containerTabsView .subviewbutton:last-child {\n  list-style-image: url("chrome://global/skin/icons/settings.svg");\n}\n\n/* leptonChrome.css:28272-28279 \u2014 extra conditions dropped: userChrome.icon.panel */\n#BMB_viewBookmarksSidebar {\n  --menuitem-image: var(--uc-sidebar-icon);\n}\n#BMB_viewBookmarksSidebar[data-l10n-args=\'{"isVisible":true}\'] {\n  --menuitem-image: url("chrome://global/skin/icons/close.svg");\n}\n\n/* leptonChrome.css:28280-28284 \u2014 extra conditions dropped: userChrome.icon.panel */\n#BMB_searchBookmarks {\n  --menuitem-image: url("chrome://global/skin/icons/search-glass.svg");\n}\n\n/* leptonChrome.css:28285-28290 \u2014 extra conditions dropped: userChrome.icon.panel */\n#BMB_bookmarksShowAllTop,\n#BMB_bookmarksShowAll {\n  --menuitem-image: url("chrome://browser/skin/bookmark-star-on-tray.svg");\n}\n\n/* leptonChrome.css:28291-28295 \u2014 extra conditions dropped: userChrome.icon.panel */\n#BMB_bookmarksToolbar {\n  --menuitem-image: url("../icons/bookmarks-toolbar-alt.svg");\n}\n\n/* leptonChrome.css:28296-28304 \u2014 extra conditions dropped: userChrome.icon.panel */\n@media (-moz-pref("layout.css.osx-font-smoothing.enabled")) {\n  #BMB_bookmarksShowAllTop {\n    list-style-image: url("chrome://browser/skin/bookmark-star-on-tray.svg") !important;\n  }\n  #BMB_bookmarksShowAllTop > .menu-iconic-left {\n    display: flex !important;\n    display: -moz-box !important;\n  }\n}\n\n/* leptonChrome.css:28305-28312 \u2014 extra conditions dropped: userChrome.icon.panel */\n#BMB_viewBookmarksToolbar[data-l10n-args=\'{"isVisible":true}\'] {\n  --menuitem-image: url("../icons/eye-hide.svg");\n}\n#BMB_viewBookmarksToolbar[data-l10n-args=\'{"isVisible":false}\'] {\n  --menuitem-image: url("../icons/eye-show.svg");\n}\n\n/* leptonChrome.css:28313-28320 \u2014 extra conditions dropped: userChrome.icon.panel */\n#protections-popup-settings-button > .protections-popup-settings-icon,\n#protections-popup-show-report-button > .protections-popup-show-report-icon {\n  -moz-context-properties: fill, fill-opacity, stroke;\n  fill: currentColor;\n  margin-inline-end: 1em;\n}\n\n/* leptonChrome.css:28321-28325 \u2014 extra conditions dropped: userChrome.icon.panel */\n#protections-popup-report-broken-site-button {\n  list-style-image: url("chrome://global/skin/icons/lightbulb.svg");\n}\n\n/* leptonChrome.css:28326-28331 \u2014 extra conditions dropped: userChrome.icon.panel */\n#protections-popup-settings-button > .protections-popup-settings-icon,\n#protections-popup-multiView .panel-subview-footer-button {\n  list-style-image: url("chrome://global/skin/icons/settings.svg");\n}\n\n/* leptonChrome.css:28332-28337 \u2014 extra conditions dropped: userChrome.icon.panel */\n#protections-popup-show-report-button > .protections-popup-show-report-icon {\n  /* chrome://browser/skin/controlcenter/dashboard.svg */\n  list-style-image: url("../icons/dashboard.svg");\n}\n\n/* leptonChrome.css:28338-28343 \u2014 extra conditions dropped: userChrome.icon.panel */\n#identity-popup-clear-sitedata-button,\n#identity-popup-more-info {\n  padding-inline: 5px !important;\n}\n\n/* leptonChrome.css:28344-28348 \u2014 extra conditions dropped: userChrome.icon.panel */\n#identity-popup-securityView-body {\n  margin-inline-start: 32px !important; /* Original: 10px */\n}\n\n/* leptonChrome.css:28349-28353 \u2014 extra conditions dropped: userChrome.icon.panel */\n#identity-popup-clear-sitedata-button {\n  list-style-image: url("../icons/broom.svg");\n}\n\n/* leptonChrome.css:28354-28361 \u2014 extra conditions dropped: userChrome.icon.panel */\n#identity-popup-more-info.subviewbutton {\n  list-style-image: url("chrome://global/skin/icons/info.svg");\n}\n#identity-popup-more-info:is(menuitem) {\n  --menuitem-image: url("chrome://global/skin/icons/info.svg");\n}\n\n/* leptonChrome.css:28362-28369 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebar-switcher-bookmarks.subviewbutton {\n  list-style-image: url("chrome://browser/skin/bookmark.svg");\n}\n#sidebar-switcher-bookmarks:is(menuitem) {\n  --menuitem-image: url("chrome://browser/skin/bookmark.svg");\n}\n\n/* leptonChrome.css:28370-28377 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebar-switcher-history.subviewbutton {\n  list-style-image: url("chrome://browser/skin/history.svg");\n}\n#sidebar-switcher-history:is(menuitem) {\n  --menuitem-image: url("chrome://browser/skin/history.svg");\n}\n\n/* leptonChrome.css:28378-28385 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebar-switcher-tabs.subviewbutton {\n  list-style-image: url("../icons/synced-tabs.svg");\n}\n#sidebar-switcher-tabs:is(menuitem) {\n  --menuitem-image: url("../icons/synced-tabs.svg");\n}\n\n/* leptonChrome.css:28386-28393 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebar-switcher-genai-chat.subviewbutton {\n  list-style-image: url("chrome://mozapps/skin/extensions/category-discover.svg");\n}\n#sidebar-switcher-genai-chat:is(menuitem) {\n  --menuitem-image: url("chrome://mozapps/skin/extensions/category-discover.svg");\n}\n\n/* leptonChrome.css:28394-28401 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebar-reverse-position.subviewbutton {\n  list-style-image: var(--uc-sidebar-icon-reverse);\n}\n#sidebar-reverse-position:is(menuitem) {\n  --menuitem-image: var(--uc-sidebar-icon-reverse);\n}\n\n/* leptonChrome.css:28402-28409 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebarMenu-popup > *.subviewbutton[data-l10n-id="sidebar-menu-close"] {\n  list-style-image: url("chrome://global/skin/icons/close.svg");\n}\n#sidebarMenu-popup > *:is(menuitem)[data-l10n-id="sidebar-menu-close"] {\n  --menuitem-image: url("chrome://global/skin/icons/close.svg");\n}\n\n/* leptonChrome.css:28410-28414 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebar-context-menu-manage-extension {\n  --menuitem-image: url("chrome://global/skin/icons/settings.svg");\n}\n\n/* leptonChrome.css:28415-28419 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebar-context-menu-remove-extension {\n  --menuitem-image: url("chrome://global/skin/icons/delete.svg");\n}\n\n/* leptonChrome.css:28420-28424 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebar-context-menu-report-extension {\n  --menuitem-image: url("../icons/send.svg");\n}\n\n/* leptonChrome.css:28425-28429 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebar-context-menu-unpin-extension {\n  --menuitem-image: url("../icons/unpin-tab.svg");\n}\n\n/* leptonChrome.css:28430-28434 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebar-context-menu-hide-sidebar {\n  --menuitem-image: url("../icons/eye-hide.svg");\n}\n\n/* leptonChrome.css:28435-28439 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebar-context-menu-customize-sidebar {\n  --menuitem-image: url("chrome://browser/skin/customize.svg");\n}\n\n/* leptonChrome.css:28440-28444 \u2014 extra conditions dropped: userChrome.icon.panel */\n#sidebar-context-menu-enable-vertical-tabs {\n  --menuitem-image: url("chrome://browser/skin/sidebar-collapsed.svg");\n}\n\n/* leptonChrome.css:28445-28449 \u2014 extra conditions dropped: userChrome.icon.panel */\n.item.client[clientType="phone"] > .item-title-container > .item-icon-container {\n  background-image: url("../icons/device-phone.svg") !important;\n}\n\n/* leptonChrome.css:28450-28454 \u2014 extra conditions dropped: userChrome.icon.panel */\n.item.client[clientType="tablet"] > .item-title-container > .item-icon-container {\n  background-image: url("../icons/device-tablet.svg") !important;\n}\n\n/* leptonChrome.css:28455-28459 \u2014 extra conditions dropped: userChrome.icon.panel */\n.item.client[clientType="desktop"] > .item-title-container > .item-icon-container {\n  background-image: url("../icons/device-desktop.svg") !important;\n}\n\n/* leptonChrome.css:28460-28464 \u2014 extra conditions dropped: userChrome.icon.panel */\n.item.client[clientType="tv"] > .item-title-container > .item-icon-container {\n  background-image: url("../icons/device-tv.svg") !important;\n}\n\n/* leptonChrome.css:28465-28469 \u2014 extra conditions dropped: userChrome.icon.panel */\n.item.client[clientType="vr"] > .item-title-container > .item-icon-container {\n  background-image: url("../icons/device-vr.svg") !important;\n}\n\n/* leptonChrome.css:28470-28474 \u2014 extra conditions dropped: userChrome.icon.panel */\n#unified-extensions-manage-extensions {\n  list-style-image: url("chrome://mozapps/skin/extensions/extension.svg");\n}\n\n/* leptonChrome.css:28475-28482 \u2014 extra conditions dropped: userChrome.icon.panel */\n.sync-engine-tabs .checkbox-icon,\n.sync-engine-tabs.sync-engine-image,\n#sidebar-box[sidebarcommand="viewTabsSidebar"] > #sidebar-header > #sidebar-switcher-target > #sidebar-icon,\n#sync-button {\n  list-style-image: url("../icons/synced-tabs.svg") !important;\n}\n\n/* leptonChrome.css:28483-28487 \u2014 extra conditions dropped: userChrome.icon.panel */\n#allTabsMenu_sortTabsButton {\n  list-style-image: url("../icons/text-sort-ascending.svg");\n}\n\n/* leptonChrome.css:31283-31287 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenu-restart-button {\n  list-style-image: url("../icons/refresh-cw.svg") !important;\n}\n\n/* leptonChrome.css:31302-31312 \u2014 extra conditions dropped: userChrome.icon.panel */\n#appMenuNewIdentity {\n  list-style-image: url("chrome://browser/skin/new_identity.svg");\n}\n#appMenuNewCircuit {\n  list-style-image: url("chrome://browser/skin/new_circuit.svg");\n}\n#appMenu_torBrowserUserManual {\n  list-style-image: url("chrome://browser/skin/onion.svg");\n}\n\n/* leptonChrome.css:31324-31331 */\n#ssbPageAction-image {\n  list-style-image: url("../icons/pwa-install.svg");\n}\n#ssbPageAction-image[open-ssb="true"] {\n  list-style-image: url("../icons/pwa-launch.svg");\n}\n\n/* leptonChrome.css:31332-31348 \u2014 extra conditions dropped: userChrome.icon.panel */\n#rebootappmenu {\n  list-style-image: url("../icons/refresh-cw.svg");\n}\n#openprofiledir {\n  list-style-image: var(--uc-folder-icon);\n}\n#appMenu-ssb-button {\n  list-style-image: url("../icons/pwa-manage.svg");\n}\n#appMenu-install-or-open-ssb-current-page-button {\n  list-style-image: url("../icons/pwa-install.svg");\n}\n#appMenu-install-or-open-ssb-current-page-button[open-ssb="true"] {\n  list-style-image: url("../icons/pwa-launch.svg");\n}\n';

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/icon-menu.css?inline
  var icon_menu_default = `/* Floorp chrome-extras: iconMenu
 * Ported from vendored Lepton (leptonChrome.css 28488-31401, 352 block(s)).
 *
 * The \`@media (-moz-bool-pref: ...)\` gate has been removed \u2014 whether this
 * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).
 * That also drops Floorp's dependency on the vendored file, which
 * update_lepton.yml re-syncs from upstream every day.
 *
 * Applied only while icons are enabled (iconDisabled off).
 *
 * 20 block(s) gated on Lepton-only sub-options (userChrome.icon.menu.full, userChrome.theme.non_native_menu, userChrome.padding.global_menubar) are not ported \u2014 they stay Lepton-family-only.
 */

/* leptonChrome.css:28488-28582 */
menupopup menuitem:not([type="checkbox"][checked="true"], [type="radio"]),
menupopup menu:not([type="checkbox"][checked="true"], [type="radio"]),
menupopup menuitem:not([type="checkbox"][checked="true"], [type="radio"]) > .menu-iconic-left > .menu-iconic-icon,
menupopup menuitem:not([type="checkbox"][checked="true"], [type="radio"]) > .menu-icon,
menupopup menu:not([type="checkbox"][checked="true"], [type="radio"]) > .menu-iconic-left > .menu-iconic-icon,
menupopup menu:not([type="checkbox"][checked="true"], [type="radio"]) > .menu-icon,
#main-menubar > menu {
  -moz-appearance: none !important; /* Linux: menulist */
}
/* Icon */
#main-menubar > menu,
:not(menu, #ContentSelectDropdown)
  > menupopup:not(.in-menulist)
  > menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]),
:not(menu, #ContentSelectDropdown)
  > menupopup:not(.in-menulist)
  > menu:not(.menu-iconic, .in-menulist, [checked="true"]),
menupopup:is(
    #menu_FilePopup,
    #menu_EditPopup,
    #menu_viewPopup,
    #goPopup,
    #historyMenuPopup,
    #bookmarksMenuPopup,
    #menu_ProfilesPopup,
    #menu_ToolsPopup,
    #windowPopup,
    #menu_HelpPopup,
    #usercssloader-menupopup,
    #sidebar-context-menu
  )
  menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]),
menupopup:is(
    #menu_FilePopup,
    #menu_EditPopup,
    #menu_viewPopup,
    #goPopup,
    #historyMenuPopup,
    #bookmarksMenuPopup,
    #menu_ProfilesPopup,
    #menu_ToolsPopup,
    #windowPopup,
    #menu_HelpPopup,
    #usercssloader-menupopup,
    #sidebar-context-menu
  )
  menu:not(.menu-iconic, .in-menulist, [checked="true"]),
menupopup:is(#organizeButtonPopup, #maintenanceButtonPopup)
  > menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]),
menupopup:is(#viewMenuPopup, #maintenanceButtonPopup) > menu:not(.menu-iconic, .in-menulist, [checked="true"]),
menupopup:is(
    #context_sendTabToDevicePopupMenu,
    #context-sendpagetodevice-popup,
    #context-sendlinktodevice-popup,
    #frame > menupopup,
    #spell-dictionaries-menu,
    #context-ask-chat > menupopup
  )
  > menuitem:not([type="checkbox"][checked="true"], [type="radio"]),
.openintabs-menuitem,
#blockedPopupDontShowMessage,
#BMB_viewBookmarksToolbar,
#sidebarMenu-popup:is(menupopup),
#context_openANewTab.tabmix-newtab-menu-icon {
  /* Color */
  -moz-context-properties: fill, fill-opacity, stroke !important;
  fill: currentColor !important;
  stroke: currentColor;
  /* Layout */
  background-size: 16px !important;
  background-repeat: no-repeat !important;
  background-image: var(--menuitem-image);
}
#tabContextMenu > .tabmix-newtab-menu-icon {
  -moz-context-properties: fill, fill-opacity, stroke !important;
  fill: currentColor !important;
}
/* For native context menus */
/* Padding */
/* Menubar */
/* Padding - Non Native */
/* Padding - Windows */
/* Padding - Linux */
/* Padding - Mac */
/*= Bookmark Menu - Layout ===================================================*/
/* #goPopup(Legacy of historyMenuPopup), #historyMenuPopup, #bookmarksMenuPopup: looks like global menu
   * #BMB_bookmarksPopup: looks like arrow panel
   */
/* Empty Menu */
/* Bookmark Popup - As Arrow Panel */
/* Windows */
/* Windows 7, 8 */
/* Linux */

/* leptonChrome.css:28583-28593 */
@media (-moz-pref("widget.macos.native-context-menus")), (-moz-pref("widget.gtk.native-context-menus")) {
  :not(menu, #ContentSelectDropdown)
    > menupopup:not(.in-menulist)
    > menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]),
  :not(menu, #ContentSelectDropdown)
    > menupopup:not(.in-menulist)
    > menu:not(.menu-iconic, .in-menulist, [checked="true"]) {
    list-style-image: var(--menuitem-image, url("../icons/blank.svg")) !important;
  }
}

/* leptonChrome.css:28594-28603 */
:root {
  --uc-menu-background-position: left;
  --context-menu-background-padding-default: 5px;
  --context-menu-background-padding: var(--context-menu-background-padding-default);
}
:root:-moz-locale-dir(rtl) {
  --uc-menu-background-position: right;
}

/* leptonChrome.css:28604-28665 */
:not(menu, #ContentSelectDropdown, #context-navigation)
  > menupopup:not(.in-menulist)
  > menuitem:not(.menuitem-iconic),
:not(menu, #ContentSelectDropdown, #context-navigation) > menupopup:not(.in-menulist) > menu:not(.menu-iconic),
:not(menu, #ContentSelectDropdown, #context-navigation)
  > menupopup:not(.in-menulist)
  > menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]),
:not(menu, #ContentSelectDropdown, #context-navigation)
  > menupopup:not(.in-menulist)
  > menu:not(.menu-iconic, .in-menulist, [checked="true"]),
menupopup:is(
    #menu_FilePopup,
    #menu_EditPopup,
    #menu_viewPopup,
    #goPopup,
    #historyMenuPopup,
    #bookmarksMenuPopup,
    #menu_ProfilesPopup,
    #menu_ToolsPopup,
    #windowPopup,
    #menu_HelpPopup,
    #usercssloader-menupopup,
    #sidebar-context-menu
  )
  menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]),
menupopup:is(
    #menu_FilePopup,
    #menu_EditPopup,
    #menu_viewPopup,
    #goPopup,
    #historyMenuPopup,
    #bookmarksMenuPopup,
    #menu_ProfilesPopup,
    #menu_ToolsPopup,
    #windowPopup,
    #menu_HelpPopup,
    #usercssloader-menupopup,
    #sidebar-context-menu
  )
  menu:not(.menu-iconic, .in-menulist, [checked="true"]),
menupopup:is(#organizeButtonPopup, #maintenanceButtonPopup)
  > menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]),
menupopup:is(#viewMenuPopup, #maintenanceButtonPopup) > menu:not(.menu-iconic, .in-menulist, [checked="true"]),
menupopup:is(
    #context_sendTabToDevicePopupMenu,
    #context-sendpagetodevice-popup,
    #context-sendlinktodevice-popup,
    #frame > menupopup,
    #spell-dictionaries-menu,
    #context-ask-chat > menupopup
  )
  > menuitem:not([type="checkbox"][checked="true"], [type="radio"]),
.openintabs-menuitem,
#blockedPopupDontShowMessage,
#BMB_viewBookmarksToolbar,
#sidebarMenu-popup:is(menupopup),
#context_openANewTab.tabmix-newtab-menu-icon {
  background-position: var(--uc-menu-background-position) var(--context-menu-background-padding) center !important;
  padding-inline-start: var(--context-menu-background-padding) !important;
}

/* leptonChrome.css:28666-28677 */
#sidebarMenu-popup:is(menupopup):not(panel) {
  --context-menu-background-padding: var(--context-menu-background-padding-default);
  padding-inline-start: 0 !important;
}
:not(menu, #ContentSelectDropdown, #context-navigation)
  > #sidebarMenu-popup:is(menupopup):not(panel)
  > menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]) {
  padding-inline-start: 28px !important;
  margin-inline-start: var(--arrowpanel-menuitem-margin-inline) !important;
}

/* leptonChrome.css:28678-28692 \u2014 extra conditions dropped: userChrome.icon.global_menubar */
#main-menubar > menu {
  background-position: var(--uc-menu-background-position) var(--context-menu-background-padding-default) center !important;
  padding-inline-start: calc(16px + var(--context-menu-background-padding-default)) !important;
  padding-inline-end: 3px;
}
#main-menubar > menu:first-child {
  background-position: var(--uc-menu-background-position) calc(3px + var(--context-menu-background-padding-default))
    center !important;
  padding-inline-start: calc(19px + var(--context-menu-background-padding-default)) !important;
}
#main-menubar > menu menupopup {
  --menuitem-image: none; /* Prevent Image Inheritance */
}

/* leptonChrome.css:28693-28698 \u2014 extra conditions dropped: userChrome.icon.global_menubar */
@media (-moz-platform: windows) {
  #main-menubar > menu > :is(.menubar-text, .menu-text) {
    transform: translateY(-1px) !important; /* Fix menu top padding position */
    padding-inline-start: 0.3em !important;
  }
}

/* leptonChrome.css:28699-28703 \u2014 extra conditions dropped: userChrome.icon.global_menubar */
@media (-moz-gtk-csd-available) {
  #main-menubar > menu > :is(.menubar-text, .menu-text) {
    padding-inline-start: 3px;
  }
}

/* leptonChrome.css:28704-28708 \u2014 extra conditions dropped: userChrome.icon.global_menubar */
#main-menubar > menu {
  padding-block: 2px !important;
}

/* leptonChrome.css:28709-28840 */
@media (-moz-platform: windows), (-moz-gtk-csd-available) and (-moz-platform: windows) {
  :root {
    --context-menu-background-padding: 1em;
    --context-menu-text-padding-default: 24px;
    --context-menu-text-padding: var(--context-menu-text-padding-default);
    --menu-background-padding-default: calc(var(--context-menu-background-padding) + var(--context-menu-text-padding));
    --bookmark-menu-icon-align-padding: 0px;
  }
  :not(menu, #ContentSelectDropdown, #context-navigation)
    > menupopup:not(.in-menulist)
    > menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]),
  :not(menu, #ContentSelectDropdown, #context-navigation)
    > menupopup:not(.in-menulist)
    > menu:not(.menu-iconic, .in-menulist, [checked="true"]),
  menupopup:is(
      #menu_FilePopup,
      #menu_EditPopup,
      #menu_viewPopup,
      #goPopup,
      #historyMenuPopup,
      #bookmarksMenuPopup,
      #menu_ProfilesPopup,
      #menu_ToolsPopup,
      #windowPopup,
      #menu_HelpPopup,
      #usercssloader-menupopup,
      #sidebar-context-menu
    )
    menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]),
  menupopup:is(
      #menu_FilePopup,
      #menu_EditPopup,
      #menu_viewPopup,
      #goPopup,
      #historyMenuPopup,
      #bookmarksMenuPopup,
      #menu_ProfilesPopup,
      #menu_ToolsPopup,
      #windowPopup,
      #menu_HelpPopup,
      #usercssloader-menupopup,
      #sidebar-context-menu
    )
    menu:not(.menu-iconic, .in-menulist, [checked="true"]),
  menupopup:is(#organizeButtonPopup, #maintenanceButtonPopup)
    > menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]),
  menupopup:is(#viewMenuPopup, #maintenanceButtonPopup) > menu:not(.menu-iconic, .in-menulist, [checked="true"]),
  menupopup:is(
      #context_sendTabToDevicePopupMenu,
      #context-sendpagetodevice-popup,
      #context-sendlinktodevice-popup,
      #frame > menupopup,
      #spell-dictionaries-menu,
      #context-ask-chat > menupopup
    )
    > menuitem:not([type="checkbox"][checked="true"], [type="radio"]),
  .openintabs-menuitem,
  #blockedPopupDontShowMessage,
  #BMB_viewBookmarksToolbar,
  #sidebarMenu-popup:is(menupopup),
  #context_openANewTab.tabmix-newtab-menu-icon {
    padding-inline-start: var(--menu-background-padding-default) !important;
    margin-left: 0 !important;
  }
  :not(menu, #ContentSelectDropdown, #context-navigation)
    > menupopup:not(.in-menulist)
    > menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"])
    > .menu-icon,
  :not(menu, #ContentSelectDropdown, #context-navigation)
    > menupopup:not(.in-menulist)
    > menu:not(.menu-iconic, .in-menulist, [checked="true"])
    > .menu-icon,
  menupopup:is(
      #menu_FilePopup,
      #menu_EditPopup,
      #menu_viewPopup,
      #goPopup,
      #historyMenuPopup,
      #bookmarksMenuPopup,
      #menu_ProfilesPopup,
      #menu_ToolsPopup,
      #windowPopup,
      #menu_HelpPopup,
      #usercssloader-menupopup,
      #sidebar-context-menu
    )
    menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"])
    > .menu-icon,
  menupopup:is(
      #menu_FilePopup,
      #menu_EditPopup,
      #menu_viewPopup,
      #goPopup,
      #historyMenuPopup,
      #bookmarksMenuPopup,
      #menu_ProfilesPopup,
      #menu_ToolsPopup,
      #windowPopup,
      #menu_HelpPopup,
      #usercssloader-menupopup,
      #sidebar-context-menu
    )
    menu:not(.menu-iconic, .in-menulist, [checked="true"])
    > .menu-icon,
  menupopup:is(#organizeButtonPopup, #maintenanceButtonPopup)
    > menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"])
    > .menu-icon,
  menupopup:is(#viewMenuPopup, #maintenanceButtonPopup)
    > menu:not(.menu-iconic, .in-menulist, [checked="true"])
    > .menu-icon,
  menupopup:is(
      #context_sendTabToDevicePopupMenu,
      #context-sendpagetodevice-popup,
      #context-sendlinktodevice-popup,
      #frame > menupopup,
      #spell-dictionaries-menu,
      #context-ask-chat > menupopup
    )
    > menuitem:not([type="checkbox"][checked="true"], [type="radio"])
    > .menu-icon,
  .openintabs-menuitem > .menu-icon,
  #blockedPopupDontShowMessage > .menu-icon,
  #BMB_viewBookmarksToolbar > .menu-icon,
  #sidebarMenu-popup:is(menupopup) > .menu-icon,
  #context_openANewTab.tabmix-newtab-menu-icon > .menu-icon {
    display: none !important;
  }
  .menupopup-arrowscrollbox {
    padding-inline: 0 !important; /* Original: padding: var(--panel-padding); --panel-padding-block: 4px; */
  }
}

/* leptonChrome.css:28973-28982 */
@media (-moz-platform: windows) {
  :root {
    --bookmark-menu-icon-text-padding: calc(
      var(--context-menu-text-padding) + var(--arrowpanel-menuitem-padding-inline)
    );
    --bookmark-menu-icon-background-padding: calc(
      var(--arrowpanel-menuitem-padding-inline) + var(--bookmark-menu-icon-align-padding)
    );
  }
}

/* leptonChrome.css:28983-28994 */
@media (-moz-gtk-csd-available) {
  :root {
    --context-menu-background-padding-default: 6px;
    --context-menu-text-padding: 21px;
  }
  .menu-icon {
    display: unset !important;
  }
  #main-menubar > menu > .menu-icon {
    display: none !important;
  }
}

/* leptonChrome.css:28995-29021 */
@media (-moz-pref("layout.css.osx-font-smoothing.enabled")) {
  :root {
    --context-menu-background-padding-default: 10px;
    --context-menu-mac-padding: 21px;
  }
  /* context menu width */
  :not(menu, #ContentSelectDropdown, #context-navigation)
    > menupopup:not(.in-menulist)
    > menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]),
  :not(menu, #ContentSelectDropdown, #context-navigation)
    > menupopup:not(.in-menulist)
    > menu:not(.menu-iconic, .in-menulist, [checked="true"]),
  #blockedPopupDontShowMessage {
    padding-inline-end: var(--context-menu-background-padding) !important;
  }
  /* text position */
  :not(menu, #ContentSelectDropdown, #context-navigation) > menupopup:not(.in-menulist) > menuitem > .menu-text,
  :not(menu, #ContentSelectDropdown, #context-navigation) > menupopup:not(.in-menulist) > menu > .menu-text {
    padding-inline-start: var(--context-menu-mac-padding) !important;
  }
  /* Checkbox menuitem, None iconic menu */
  :not(menu, #ContentSelectDropdown, #context-navigation) > menupopup:not(.in-menulist) > menuitem[type="checkbox"] {
    padding-inline-start: calc(var(--context-menu-background-padding) + var(--context-menu-mac-padding)) !important;
  }
  /* Global Menu */
  /* Exeptions */
}

/* leptonChrome.css:29056-29060 */
@media (-moz-pref("layout.css.osx-font-smoothing.enabled")) {
  #unified-extensions-context-menu > menuitem::before {
    padding-inline: 0 !important;
  }
}

/* leptonChrome.css:29061-29084 */
@media (-moz-pref("layout.css.osx-font-smoothing.enabled")) {
  :not(menu, #ContentSelectDropdown, #context-navigation)
    > menupopup:not(.in-menulist)
    > menuitem:not(.menuitem-iconic, .bookmark-item, .in-menulist, [checked="true"]) {
    padding-inline-start: 0 !important;
  }
  :not(menu, #ContentSelectDropdown, #context-navigation)
    > menupopup:not(.in-menulist)
    > menuitem:not(.menuitem-iconic):has(.menu-iconic-left)[checked] {
    padding-left: 4px !important;
    background-image: none !important;
  }
  :not(menu, #ContentSelectDropdown, #context-navigation)
    > menupopup:not(.in-menulist)
    > menuitem:not(.menuitem-iconic):has(.menu-iconic-left)[checked="true"] {
    padding-left: calc(var(--context-menu-mac-padding) - 8px) !important;
  }
  :not(menu, #ContentSelectDropdown, #context-navigation)
    > menupopup:not(.in-menulist)
    > menuitem:not(.menuitem-iconic):has(.menu-iconic-left)[checked="true"]
    .menu-iconic-text {
    padding-left: 8px !important;
  }
}

/* leptonChrome.css:29085-29090 */
menupopup menupopup[emptyplacesresult] .menu-text,
#PersonalToolbar menupopup[emptyplacesresult] .menu-text {
  margin-inline-start: 0 !important;
}

/* leptonChrome.css:29091-29096 */
#BMB_bookmarksPopup,
#PersonalToolbar {
  --context-menu-background-padding: var(--arrowpanel-menuitem-padding-inline);
}

/* leptonChrome.css:29097-29111 */
@media (-moz-platform: windows) {
  /* Bookmark Popup - None icon menu */
  menupopup:is(#BMB_bookmarksPopup)[placespopup="true"]
    menuitem:not(.menuitem-iconic, [disabled="true"]):not(:has(.menu-icon[aria-hidden="true"])),
  menupopup:is(#BMB_bookmarksPopup)[placespopup="true"]
    menu:not(.menu-iconic):not(:has(.menu-icon[aria-hidden="true"])),
  #PersonalToolbar
    menupopup[placespopup="true"]
    menuitem:not(.menuitem-iconic, [disabled="true"]):not(:has(.menu-icon[aria-hidden="true"])),
  #PersonalToolbar menupopup[placespopup="true"] menu:not(.menu-iconic):not(:has(.menu-icon[aria-hidden="true"])) {
    margin-inline: var(--arrowpanel-menuitem-margin-inline) !important;
    padding-inline-start: var(--bookmark-menu-icon-text-padding) !important;
    background-position: var(--uc-menu-background-position) var(--bookmark-menu-icon-background-padding) center !important;
  }
}

/* leptonChrome.css:29112-29133 */
@media (-moz-gtk-csd-available) {
  /* Global Menu */
  menupopup:is(#goPopup, #historyMenuPopup, #bookmarksMenuPopup) .bookmark-item {
    padding-inline-start: var(--context-menu-background-padding) !important;
  }
  /* Bookmark Popup - Iconic menu */
  #BMB_bookmarksPopup .menu-iconic-text,
  #PersonalToolbar menupopup[placespopup="true"] .bookmark-item .menu-iconic-text {
    margin-inline-start: -1px !important;
  }
  /* Bookmark Popup - None icon menu */
  menupopup:is(#BMB_bookmarksPopup)[placespopup="true"]
    menuitem:not(.menuitem-iconic, [disabled="true"]):not(:has(.menu-icon[aria-hidden="true"])),
  menupopup:is(#BMB_bookmarksPopup)[placespopup="true"]
    menu:not(.menu-iconic):not(:has(.menu-icon[aria-hidden="true"])),
  #PersonalToolbar
    menupopup[placespopup="true"]
    menuitem:not(.menuitem-iconic, [disabled="true"]):not(:has(.menu-icon[aria-hidden="true"])),
  #PersonalToolbar menupopup[placespopup="true"] menu:not(.menu-iconic):not(:has(.menu-icon[aria-hidden="true"])) {
    padding-inline-start: calc(var(--context-menu-background-padding) + 2px) !important;
  }
}

/* leptonChrome.css:29146-29163 */
@media (-moz-gtk-csd-available) {
  menupopup:is(#BMB_bookmarksPopup)[placespopup="true"]
    menuitem:not(.menuitem-iconic, [disabled="true"]):not(:has(.menu-icon[aria-hidden="true"]))
    .menu-text,
  menupopup:is(#BMB_bookmarksPopup)[placespopup="true"]
    menu:not(.menu-iconic):not(:has(.menu-icon[aria-hidden="true"]))
    .menu-text,
  #PersonalToolbar
    menupopup[placespopup="true"]
    menuitem:not(.menuitem-iconic, [disabled="true"]):not(:has(.menu-icon[aria-hidden="true"]))
    .menu-text,
  #PersonalToolbar
    menupopup[placespopup="true"]
    menu:not(.menu-iconic):not(:has(.menu-icon[aria-hidden="true"]))
    .menu-text {
    margin-inline-start: var(--context-menu-text-padding) !important;
  }
}

/* leptonChrome.css:29164-29199 */
@media (-moz-pref("layout.css.osx-font-smoothing.enabled")) {
  /* Bookmark Popup - As Arrow Panel */
  #PersonalToolbar menupopup menuitem,
  #PersonalToolbar menupopup menu {
    padding-inline: var(--arrowpanel-menuitem-padding-inline) !important;
  }
  /* Bookmark Popup - None icon menu */
  menupopup:is(#BMB_bookmarksPopup)[placespopup="true"]
    menuitem:not(.menuitem-iconic, [disabled="true"]):not(:has(.menu-icon[aria-hidden="true"])),
  menupopup:is(#BMB_bookmarksPopup)[placespopup="true"]
    menu:not(.menu-iconic):not(:has(.menu-icon[aria-hidden="true"])) {
    padding-inline-start: calc(var(--arrowpanel-menuitem-padding-inline) + 3px) !important;
  }
  #PersonalToolbar
    menupopup[placespopup="true"]
    menuitem:not(.menuitem-iconic, [disabled="true"]):not(:has(.menu-icon[aria-hidden="true"])),
  #PersonalToolbar menupopup[placespopup="true"] menu:not(.menu-iconic):not(:has(.menu-icon[aria-hidden="true"])) {
    padding-inline-start: calc(var(--arrowpanel-menuitem-padding-inline) + 1px) !important;
  }
  menupopup:is(#BMB_bookmarksPopup)[placespopup="true"]
    menuitem:not(.menuitem-iconic, [disabled="true"]):not(:has(.menu-icon[aria-hidden="true"]))
    > .menu-text,
  menupopup:is(#BMB_bookmarksPopup)[placespopup="true"]
    menu:not(.menu-iconic):not(:has(.menu-icon[aria-hidden="true"]))
    > .menu-text,
  #PersonalToolbar
    menupopup[placespopup="true"]
    menuitem:not(.menuitem-iconic, [disabled="true"]):not(:has(.menu-icon[aria-hidden="true"]))
    > .menu-text,
  #PersonalToolbar
    menupopup[placespopup="true"]
    menu:not(.menu-iconic):not(:has(.menu-icon[aria-hidden="true"]))
    > .menu-text {
    padding-inline-start: var(--context-menu-mac-padding) !important;
  }
}

/* leptonChrome.css:29200-29326 \u2014 extra conditions dropped: userChrome.icon.context_menu */
/** Context Menu - Icons ******************************************************/ /*= tabContextMenu ===========================================================*/
#context_openANewTab,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_newTab,
#tabcenter-reborn_ariasuni-menuitem-_newTabContextMenuOpenAlternatePosition,
#sidebartabs_asamuzak_jp-menuitem-_newTab {
  --menuitem-image: var(--uc-new-tab-icon);
}
#tabcenter-reborn_ariasuni-menuitem-_newTabContextMenuOpenInWindow {
  --menuitem-image: url("chrome://browser/skin/window.svg");
}
#tabcenter-reborn_ariasuni-menuitem-_newTabContextMenuOpenInPrivateWindow {
  --menuitem-image: url("chrome://browser/skin/privateBrowsing.svg");
}
#context_reloadTab,
#context_reloadSelectedTabs,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_reloadTab,
#treestyletab_piro_sakura_ne_jp-menuitem-_noContextTab\\:context_reloadTab,
#treestyletab_piro_sakura_ne_jp-menuitem-_grouped\\:reloadTree,
#tabcenter-reborn_ariasuni-menuitem-_contextMenuReloadTab,
#sidebartabs_asamuzak_jp-menuitem-_reloadTab,
#sidebartabs_asamuzak_jp-menuitem-_reloadAllTabs {
  --menuitem-image: url("../icons/reload.svg");
}
#context_playTab,
#context_playSelectedTabs {
  --menuitem-image: url("../icons/play-fill.svg");
}
#context_toggleMuteTab,
#context_toggleMuteSelectedTabs,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_toggleMuteTab[label="Mute Tab"],
#treestyletab_piro_sakura_ne_jp-menuitem-_context_toggleMuteTab-mute,
#tabcenter-reborn_ariasuni-menuitem-_contextMenuMuteTab,
#sidebartabs_asamuzak_jp-menuitem-_muteTab {
  --menuitem-image: url("chrome://browser/skin/tabbrowser/tab-audio-muted-small.svg");
  stroke: transparent !important;
}
#context_toggleMuteTab[muted],
#context_toggleMuteSelectedTabs[muted],
#treestyletab_piro_sakura_ne_jp-menuitem-_context_toggleMuteTab[label="Unmute Tab"],
#treestyletab_piro_sakura_ne_jp-menuitem-_context_toggleMuteTab-unmute,
#tabcenter-reborn_ariasuni-menuitem-_contextMenuMuteTab[label="Unmute Tab"],
#sidebartabs_asamuzak_jp-menuitem-_muteTab[label="Unmute Tab"] {
  --menuitem-image: url("chrome://browser/skin/tabbrowser/tab-audio-playing-small.svg");
}
#context_pinTab,
#context_pinSelectedTabs,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_pinTab,
#tabcenter-reborn_ariasuni-menuitem-_contextMenuPinTab,
#sidebartabs_asamuzak_jp-menuitem-_pinTab {
  --menuitem-image: url("../icons/pin-tab.svg");
}
#context_unpinTab,
#context_unpinSelectedTabs,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_unpinTab,
#tabcenter-reborn_ariasuni-menuitem-_contextMenuPinTab[label="Unpin Tab"],
#sidebartabs_asamuzak_jp-menuitem-_pinTab[label="Unpin Tab"] {
  --menuitem-image: url("../icons/unpin-tab.svg");
}
#treestyletab_piro_sakura_ne_jp-menuitem-_context_topLevel_toggleSticky[label="Stick Tab to Edges"] {
  --menuitem-image: url("../icons/align-distribute-top.svg");
}
#treestyletab_piro_sakura_ne_jp-menuitem-_context_topLevel_toggleSticky[label="Unstick Tab to Edges"] {
  --menuitem-image: url("../icons/align-space-evenly-vertical.svg");
}
#context_duplicateTab,
#context_duplicateTabs,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_duplicateTab,
#tabcenter-reborn_ariasuni-menuitem-_contextMenuDuplicateTab,
#sidebartabs_asamuzak_jp-menuitem-_dupeTab {
  --menuitem-image: var(--uc-tab-copy-icon);
}
#tabcenter-reborn_ariasuni-menuitem-_contextMenuUnloadTab {
  --menuitem-image: var(--uc-tab-unload-icon);
}
#context_bookmarkTab,
#context_bookmarkSelectedTabs,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_bookmarkTab,
#treestyletab_piro_sakura_ne_jp-menuitem-_noContextTab\\:context_bookmarkSelected,
#treestyletab_piro_sakura_ne_jp-menuitem-_grouped\\:bookmarkTree,
#sidebartabs_asamuzak_jp-menuitem-_bookmarkTab,
#sidebartabs_asamuzak_jp-menuitem-_bookmarkAllTabs {
  --menuitem-image: url("chrome://browser/skin/bookmark.svg");
}
#context_moveTabOptions,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_moveTab,
#tabcenter-reborn_ariasuni-menuitem-_contextMenuMoveTab,
#sidebartabs_asamuzak_jp-menuitem-_moveTab {
  --menuitem-image: url("../icons/arrow-swap.svg");
}
#context_sendTabToDevice,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_sendTabsToDevice {
  --menuitem-image: url("../icons/send-to-device.svg");
}
#context_sendTabToDevice:is([disabled="true"]) + #context_shareTabURL,
#context_sendTabToDevice:is([disabled="true"]) + menuitem.share-tab-url-item {
  /* At windows */
  --menuitem-image: url("../icons/share.svg");
}
/*= new-tab-button-popup =====================================================*/
/*= toolbar-context-menu =====================================================*/
/*= Tab Mix Plus =============================================================*/
/*= contentAreaContextMenu ===================================================*/
/*= context-ask-chat =========================================================*/
/*= main-context-menu-frame ==================================================*/
/*= Tab Mix Plus =============================================================*/
/*= downloadsContextMenu =====================================================*/ /*= placeContext =============================================================*/ /*= Notification =============================================================*/
/*= blockedPopupOptions ======================================================*/
/*= autohide-context =========================================================*/
/*= pictureInPictureToggleContextMenu ========================================*/
/*= pageActionContextMenu ====================================================*/
/*= customizationPanelItemContextMenu ========================================*/
/*= customizationPaletteItemContextMenu ======================================*/
/*= customizationPanelContextMenu ============================================*/
/*= downloads-button-autohide-panel ==========================================*/
/*= SyncedTabsSidebarContext =================================================*/
/*= SyncedTabsSidebarTabsFilterContext =======================================*/
/*= urlbar-input-container ===================================================*/
/*= textbox-contextmenu ======================================================*/
/* Browser's Searchbar, Library's Searchbar, Page Info */
/* Only searchbar */
/*= context_sendTabToDevicePopupMenu =========================================*/
/*= unified-extensions-context-menu ==========================================*/
/*= urlbarView-result-menu ===================================================*/
/*= full-page-translations-panel-settings-menupopup ==========================*/
/*= select-translations-panel-settings-menupopup =============================*/

/* leptonChrome.css:29333-29341 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context_reopenInContainer,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_reopenInContainer,
#tabcenter-reborn_ariasuni-menuitem-_contextMenuOpenInContextualTab,
#sidebartabs_asamuzak_jp-menuitem-_openNewTabInContainer,
#sidebartabs_asamuzak_jp-menuitem-_reopenTabInContainer {
  --menuitem-image: url("../icons/container-openin-16.svg");
}

/* leptonChrome.css:29342-29349 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context_selectAllTabs,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_selectAllTabs,
#treestyletab_piro_sakura_ne_jp-menuitem-_noContextTab\\:context_selectAllTabs,
#sidebartabs_asamuzak_jp-menuitem-_selectAllTabs {
  --menuitem-image: var(--uc-tab-multiple-icon);
}

/* leptonChrome.css:29350-29358 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context_closeTab,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_closeTab,
#treestyletab_piro_sakura_ne_jp-menuitem-_grouped\\:closeTree,
#tabcenter-reborn_ariasuni-menuitem-_contextMenuCloseTab,
#sidebartabs_asamuzak_jp-menuitem-_closeTab {
  --menuitem-image: url("chrome://global/skin/icons/close.svg");
}

/* leptonChrome.css:29373-29381 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context_undoCloseTab,
#treestyletab_piro_sakura_ne_jp-menuitem-_context_undoCloseTab,
#treestyletab_piro_sakura_ne_jp-menuitem-_noContextTab\\:context_undoCloseTab,
#tabcenter-reborn_ariasuni-menuitem-_contextMenuUndoCloseTab,
#sidebartabs_asamuzak_jp-menuitem-_undoCloseTab {
  --menuitem-image: url("../icons/undo.svg");
}

/* leptonChrome.css:29382-29387 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#new-tab-button-popup > menuitem[command="Browser:NewUserContextTab"],
.new-tab-popup > menuitem[command="Browser:NewUserContextTab"] {
  --menuitem-image: url("../icons/container-openin-16.svg");
}

/* leptonChrome.css:29388-29393 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#new-tab-button-popup > menuitem[command="Browser:OpenAboutContainers"],
.new-tab-popup > menuitem[command="Browser:OpenAboutContainers"] {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}

/* leptonChrome.css:29394-29398 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.customize-context-manageExtension {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}

/* leptonChrome.css:29399-29403 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.customize-context-removeExtension {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:29404-29408 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.customize-context-reportExtension {
  --menuitem-image: url("../icons/send.svg");
}

/* leptonChrome.css:29409-29413 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.customize-context-moveToPanel {
  --menuitem-image: url("../icons/pin-tab.svg");
}

/* leptonChrome.css:29414-29419 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#toolbar-context-autohide-downloads-button {
  /* checkbox */
  --menuitem-image: url("../icons/eye-tracking-off.svg");
}

/* leptonChrome.css:29420-29424 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.customize-context-removeFromToolbar {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:29425-29430 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#toolbar-context-always-open-downloads-panel {
  /* checkbox */
  --menuitem-image: url("../icons/drawer-arrow-download.svg");
}

/* leptonChrome.css:29431-29435 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#toolbar-context-openANewTab {
  --menuitem-image: var(--uc-new-tab-icon);
}

/* leptonChrome.css:29436-29441 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#toolbar-context-reloadSelectedTab,
#toolbar-context-reloadSelectedTabs {
  --menuitem-image: url("../icons/reload.svg");
}

/* leptonChrome.css:29442-29447 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#toolbar-context-bookmarkSelectedTab,
#toolbar-context-bookmarkSelectedTabs {
  --menuitem-image: url("chrome://browser/skin/bookmark.svg");
}

/* leptonChrome.css:29448-29452 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#toolbar-context-selectAllTabs {
  --menuitem-image: var(--uc-tab-multiple-icon);
}

/* leptonChrome.css:29453-29457 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#toolbar-context-undoCloseTab {
  --menuitem-image: url("../icons/undo.svg");
}

/* leptonChrome.css:29458-29462 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#toolbar-context-customize-sidebar {
  --menuitem-image: url("chrome://browser/skin/customize.svg");
}

/* leptonChrome.css:29463-29467 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#toolbar-context-toggle-vertical-tabs {
  --menuitem-image: url("chrome://browser/skin/sidebar-collapsed.svg");
}

/* leptonChrome.css:29468-29473 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#toggle_toolbar-menubar {
  /* checkbox */
  --menuitem-image: url("../icons/calendar-agenda.svg");
}

/* leptonChrome.css:29474-29479 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#toggle_PersonalToolbar {
  /* Also placeContext */
  --menuitem-image: url("../icons/bookmarks-toolbar-alt.svg");
}

/* leptonChrome.css:29480-29484 \u2014 extra conditions dropped: userChrome.icon.context_menu */
menuitem.viewCustomizeToolbar {
  --menuitem-image: url("chrome://browser/skin/customize.svg");
}

/* leptonChrome.css:29485-29490 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.fullscreen-context-autohide {
  /* checkbox */
  --menuitem-image: url("../icons/eye-tracking-off.svg");
}

/* leptonChrome.css:29491-29495 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#toolbar-context-menu > menuitem[data-l10n-id="full-screen-exit"] {
  --menuitem-image: url("chrome://browser/skin/fullscreen-exit.svg");
}

/* leptonChrome.css:29496-29500 \u2014 extra conditions dropped: userChrome.icon.context_menu */
@media (-moz-platform: windows) {
  #context_openANewTab.tabmix-newtab-menu-icon .menu-iconic-left {
    display: none;
  }
}

/* leptonChrome.css:29509-29513 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-renameTab {
  --menuitem-image: url("../icons/edit-active.svg");
}

/* leptonChrome.css:29514-29518 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-copyTabUrl {
  --menuitem-image: url("../icons/link.svg");
}

/* leptonChrome.css:29519-29523 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-autoreloadTab_menu {
  --menuitem-image: url("../icons/timer10.svg");
}

/* leptonChrome.css:29524-29528 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context_reloadTabOptions {
  --menuitem-image: url("../icons/reload.svg");
}

/* leptonChrome.css:29534-29538 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context_reloadTabOptions + #context_reloadTab {
  --menuitem-image: url("../icons/blank.svg");
}

/* leptonChrome.css:29539-29543 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-docShell {
  --menuitem-image: url("chrome://browser/skin/permissions.svg");
}

/* leptonChrome.css:29544-29548 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-freezeTab {
  --menuitem-image: url("../icons/weather-snowflake.svg");
}

/* leptonChrome.css:29549-29553 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-protectTab {
  --menuitem-image: url("../icons/shield-task.svg");
}

/* leptonChrome.css:29564-29568 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-viewsource-goToLine {
  --menuitem-image: url("../icons/text-number-format.svg");
}

/* leptonChrome.css:29569-29574 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-viewsource-wrapLongLines {
  /* checkbox */
  --menuitem-image: url("../icons/arrow-sort-down-lines.svg");
}

/* leptonChrome.css:29575-29580 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-viewsource-highlightSyntax {
  /* checkbox */
  --menuitem-image: url("../icons/highlight.svg");
}

/* leptonChrome.css:29581-29585 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#spell-no-suggestions {
  --menuitem-image: url("../icons/text-proofing-tools.svg");
}

/* leptonChrome.css:29586-29591 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#spell-add-to-dictionary,
#spell-add-dictionaries {
  --menuitem-image: url("../icons/book-add.svg");
}

/* leptonChrome.css:29592-29596 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#spell-undo-add-to-dictionary {
  --menuitem-image: url("../icons/undo.svg");
}

/* leptonChrome.css:29597-29601 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-openlinkincurrent {
  --menuitem-image: url("../icons/link-square.svg");
}

/* leptonChrome.css:29602-29607 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-openlinkincontainertab,
#context-openlinkintab {
  --menuitem-image: var(--uc-new-tab-icon);
}

/* leptonChrome.css:29608-29612 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-openlinkinusercontext-menu {
  --menuitem-image: url("../icons/container-openin-16.svg");
}

/* leptonChrome.css:29613-29617 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-openlink {
  --menuitem-image: url("chrome://browser/skin/window.svg");
}

/* leptonChrome.css:29618-29622 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-openlinkprivate {
  --menuitem-image: url("chrome://browser/skin/privateBrowsing.svg");
}

/* leptonChrome.css:29623-29627 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-bookmarklink {
  --menuitem-image: url("chrome://browser/skin/bookmark.svg");
}

/* leptonChrome.css:29628-29632 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-savelink {
  --menuitem-image: url("../icons/toolbarButton-download.svg");
}

/* leptonChrome.css:29633-29637 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-savelinktopocket {
  --menuitem-image: url("../icons/pocket-outline.svg");
}

/* leptonChrome.css:29638-29642 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-copyemail {
  --menuitem-image: url("../icons/mail.svg");
}

/* leptonChrome.css:29643-29647 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-copyphone {
  --menuitem-image: url("../icons/device-phone.svg");
}

/* leptonChrome.css:29648-29652 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-copylink {
  --menuitem-image: url("../icons/link.svg");
}

/* leptonChrome.css:29653-29657 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-stripOnShareLink {
  --menuitem-image: url("../icons/link-no-tracking.svg");
}

/* leptonChrome.css:29658-29662 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-sendlinktodevice {
  --menuitem-image: url("../icons/send-to-device.svg");
}

/* leptonChrome.css:29663-29667 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-media-play {
  --menuitem-image: url("chrome://global/skin/media/play-fill.svg");
}

/* leptonChrome.css:29668-29672 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-media-pause {
  --menuitem-image: url("chrome://global/skin/media/pause-fill.svg");
}

/* leptonChrome.css:29673-29677 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-media-mute {
  --menuitem-image: url("chrome://global/skin/media/audio-muted.svg");
}

/* leptonChrome.css:29678-29682 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-media-unmute {
  --menuitem-image: url("chrome://global/skin/media/audio.svg");
}

/* leptonChrome.css:29683-29687 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-media-playbackrate {
  --menuitem-image: url("../icons/time-picker.svg");
}

/* leptonChrome.css:29688-29693 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-media-loop {
  /* checkbox */
  --menuitem-image: url("../icons/arrow-repeat-all.svg");
}

/* leptonChrome.css:29694-29698 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-leave-dom-fullscreen {
  --menuitem-image: url("chrome://global/skin/media/fullscreenExitButton.svg");
}

/* leptonChrome.css:29699-29703 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-video-fullscreen {
  --menuitem-image: url("chrome://global/skin/media/fullscreenEnterButton.svg");
}

/* leptonChrome.css:29704-29708 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-media-hidecontrols {
  --menuitem-image: url("../icons/eye-hide.svg");
}

/* leptonChrome.css:29709-29713 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-media-showcontrols {
  --menuitem-image: url("../icons/eye-show.svg");
}

/* leptonChrome.css:29714-29718 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-viewvideo {
  --menuitem-image: url("../icons/video.svg");
}

/* leptonChrome.css:29719-29724 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-video-pictureinpicture {
  /* checkbox */
  --menuitem-image: url("chrome://global/skin/media/picture-in-picture-open.svg");
}

/* leptonChrome.css:29725-29729 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-reloadimage {
  --menuitem-image: url("../icons/image-arrow-counterclockwise.svg");
}

/* leptonChrome.css:29730-29734 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-viewimage {
  --menuitem-image: url("../icons/image-add.svg");
}

/* leptonChrome.css:29735-29739 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-saveimage {
  --menuitem-image: url("../icons/image.svg");
}

/* leptonChrome.css:29740-29744 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-video-saveimage {
  --menuitem-image: url("../icons/video-snapshot.svg");
}

/* leptonChrome.css:29745-29749 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-savevideo {
  --menuitem-image: url("../icons/video.svg");
}

/* leptonChrome.css:29750-29754 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-saveaudio {
  --menuitem-image: url("chrome://global/skin/media/audio.svg");
}

/* leptonChrome.css:29755-29759 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-copyimage-contents {
  --menuitem-image: url("../icons/image-copy.svg");
}

/* leptonChrome.css:29760-29766 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-copyimage,
#context-copyvideourl,
#context-copyaudiourl {
  --menuitem-image: url("../icons/link.svg");
}

/* leptonChrome.css:29767-29773 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-sendimage,
#context-sendvideo,
#context-sendaudio {
  --menuitem-image: url("../icons/mail.svg");
}

/* leptonChrome.css:29774-29778 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-viewimageinfo {
  --menuitem-image: url("chrome://global/skin/icons/info.svg");
}

/* leptonChrome.css:29779-29783 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-viewimagedesc {
  --menuitem-image: url("../icons/image-alt-text.svg");
}

/* leptonChrome.css:29784-29788 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-setDesktopBackground {
  --menuitem-image: url("../icons/resize-image.svg");
}

/* leptonChrome.css:29789-29793 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-ctp-play {
  --menuitem-image: url("chrome://global/skin/icons/plugin.svg");
}

/* leptonChrome.css:29794-29798 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-ctp-hide {
  --menuitem-image: url("chrome://global/skin/icons/plugin-blocked.svg");
}

/* leptonChrome.css:29799-29803 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-savepage {
  --menuitem-image: url("../icons/toolbarButton-download.svg");
}

/* leptonChrome.css:29804-29808 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-pocket {
  --menuitem-image: url("../icons/pocket-outline.svg");
}

/* leptonChrome.css:29809-29813 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-sendpagetodevice {
  --menuitem-image: url("../icons/send-to-device.svg");
}

/* leptonChrome.css:29814-29818 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#fill-login {
  --menuitem-image: url("../icons/password.svg");
}

/* leptonChrome.css:29819-29823 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#fill-login-generated-password {
  --menuitem-image: url("chrome://browser/skin/login.svg");
}

/* leptonChrome.css:29824-29828 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#use-relay-mask {
  --menuitem-image: url("../icons/relay-logo.svg");
}

/* leptonChrome.css:29829-29833 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#manage-saved-logins {
  --menuitem-image: url("../icons/key-multiple.svg");
}

/* leptonChrome.css:29834-29839 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-undo,
#context-pdfjs-undo {
  --menuitem-image: url("../icons/undo.svg");
}

/* leptonChrome.css:29846-29851 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-cut,
#context-pdfjs-cut {
  --menuitem-image: url("../icons/edit-cut.svg");
}

/* leptonChrome.css:29852-29857 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-copy,
#context-pdfjs-copy {
  --menuitem-image: url("../icons/edit-copy.svg");
}

/* leptonChrome.css:29858-29863 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-paste,
#context-pdfjs-paste {
  --menuitem-image: url("../icons/edit-paste.svg");
}

/* leptonChrome.css:29864-29869 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-delete,
#context-pdfjs-delete {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:29870-29875 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-selectall,
#context-pdfjs-selectall {
  --menuitem-image: url("../icons/select-all-on.svg");
}

/* leptonChrome.css:29876-29880 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-pdfjs-highlight-selection {
  --menuitem-image: url("../icons/toolbarButton-editorHighlight.svg");
}

/* leptonChrome.css:29881-29885 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-reveal-password {
  --menuitem-image: url("../icons/eye-show.svg");
}

/* leptonChrome.css:29886-29890 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-print-selection {
  --menuitem-image: url("chrome://global/skin/icons/print.svg");
}

/* leptonChrome.css:29891-29895 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-take-screenshot {
  --menuitem-image: url("../icons/screenshot-1.svg");
}

/* leptonChrome.css:29896-29900 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-keywordfield {
  --menuitem-image: url("chrome://browser/skin/bookmark.svg");
}

/* leptonChrome.css:29901-29906 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-searchselect,
#context-searchselect-private {
  --menuitem-image: url("chrome://global/skin/icons/search-glass.svg");
}

/* leptonChrome.css:29907-29911 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-translate-selection {
  --menuitem-image: url("../icons/translations.svg");
}

/* leptonChrome.css:29912-29916 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-ask-chat {
  --menuitem-image: url("../icons/chat-sparkle.svg");
}

/* leptonChrome.css:29917-29921 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#frame {
  --menuitem-image: url("../icons/command-frames.svg");
}

/* leptonChrome.css:29922-29927 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#spell-check-enabled {
  /* checkbox */
  --menuitem-image: url("../icons/text-proofing-tools.svg");
}

/* leptonChrome.css:29928-29932 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#spell-add-dictionaries-main {
  --menuitem-image: url("../icons/book-add.svg");
}

/* leptonChrome.css:29933-29937 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#spell-dictionaries {
  --menuitem-image: url("../icons/book.svg");
}

/* leptonChrome.css:29938-29942 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-bidi-text-direction-toggle {
  --menuitem-image: url("../icons/text-direction-horizontal-ltr.svg");
}

/* leptonChrome.css:29943-29947 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-bidi-page-direction-toggle {
  --menuitem-image: url("../icons/document-landscape-split-hint.svg");
}

/* leptonChrome.css:29948-29953 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-viewpartialsource-selection,
#context-viewsource {
  --menuitem-image: url("../icons/document-search.svg");
}

/* leptonChrome.css:29954-29958 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-inspect-a11y {
  --menuitem-image: url("../icons/tool-accessibility.svg");
}

/* leptonChrome.css:29959-29963 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-inspect {
  --menuitem-image: url("../icons/command-pick.svg");
}

/* leptonChrome.css:29964-29968 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-media-eme-learnmore {
  /* iconic */
}

/* leptonChrome.css:29969-29985 \u2014 extra conditions dropped: userChrome.icon.context_menu */
@media (-moz-pref("layout.css.osx-font-smoothing.enabled")) {
  #context-back {
    --menuitem-image: url("chrome://browser/skin/back.svg");
  }
  #context-forward {
    --menuitem-image: url("chrome://browser/skin/forward.svg");
  }
  #context-reload {
    --menuitem-image: url("../icons/reload.svg");
  }
  #context-stop {
    --menuitem-image: url("chrome://global/skin/icons/close.svg");
  }
  #context-bookmarkpage {
    --menuitem-image: url("chrome://browser/skin/bookmark.svg");
  }
}

/* leptonChrome.css:29986-29991 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#ask-chat-shortcuts .toolbarbutton-icon {
  order: -1;
  margin-inline-end: var(--arrowpanel-menuitem-margin-inline) !important;
}

/* leptonChrome.css:29992-29996 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#ask-chat-shortcuts .subviewbutton:nth-child(1) {
  list-style-image: url("../icons/apps-list-detail.svg");
}

/* leptonChrome.css:29997-30001 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#ask-chat-shortcuts .subviewbutton:nth-child(2) {
  list-style-image: url("../icons/code-text.svg");
}

/* leptonChrome.css:30002-30006 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#ask-chat-shortcuts .subviewbutton:nth-child(3) {
  list-style-image: url("../icons/tetris-app.svg");
}

/* leptonChrome.css:30007-30011 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#ask-chat-shortcuts .subviewbutton:nth-child(4) {
  list-style-image: url("../icons/arrow-up-exclamation.svg");
}

/* leptonChrome.css:30012-30016 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-ask-chat menuitem:nth-child(1) {
  --menuitem-image: url("../icons/apps-list-detail.svg");
}

/* leptonChrome.css:30017-30021 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-ask-chat menuitem:nth-child(2) {
  --menuitem-image: url("../icons/code-text.svg");
}

/* leptonChrome.css:30022-30026 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-ask-chat menuitem:nth-child(3) {
  --menuitem-image: url("../icons/tetris-app.svg");
}

/* leptonChrome.css:30027-30031 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-ask-chat menuitem:nth-child(4) {
  --menuitem-image: url("../icons/arrow-up-exclamation.svg");
}

/* leptonChrome.css:30032-30036 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-showonlythisframe {
  --menuitem-image: url("../icons/eye-show.svg");
}

/* leptonChrome.css:30037-30041 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-openframeintab {
  --menuitem-image: var(--uc-new-tab-icon);
}

/* leptonChrome.css:30042-30046 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-openframe {
  --menuitem-image: url("chrome://browser/skin/window.svg");
}

/* leptonChrome.css:30047-30051 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-reloadframe {
  --menuitem-image: url("../icons/reload.svg");
}

/* leptonChrome.css:30052-30056 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-bookmarkframe {
  --menuitem-image: url("chrome://browser/skin/bookmark.svg");
}

/* leptonChrome.css:30057-30061 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-saveframe {
  --menuitem-image: url("../icons/toolbarButton-download.svg");
}

/* leptonChrome.css:30062-30066 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-printframe {
  --menuitem-image: url("chrome://global/skin/icons/print.svg");
}

/* leptonChrome.css:30067-30071 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-take-frame-screenshot {
  --menuitem-image: url("../icons/screenshot-1.svg");
}

/* leptonChrome.css:30072-30076 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-viewframesource {
  --menuitem-image: url("../icons/document-search.svg");
}

/* leptonChrome.css:30077-30081 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#context-viewframeinfo {
  --menuitem-image: url("chrome://global/skin/icons/info.svg");
}

/* leptonChrome.css:30082-30086 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-autoreload_menu {
  --menuitem-image: url("../icons/timer10.svg");
}

/* leptonChrome.css:30087-30091 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-linkWithhistory {
  --menuitem-image: var(--uc-new-tab-skip-forward-icon);
}

/* leptonChrome.css:30092-30096 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-openAllLinks {
  --menuitem-image: var(--uc-new-tab-multiple-icon);
}

/* leptonChrome.css:30097-30101 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-openinverselink {
  --menuitem-image: var(--uc-new-tab-forward-icon);
}

/* leptonChrome.css:30102-30106 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-content-closetab {
  --menuitem-image: url("../icons/dismiss-filled.svg");
}

/* leptonChrome.css:30107-30111 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-duplicateTabContext {
  --menuitem-image: var(--uc-tab-copy-icon);
}

/* leptonChrome.css:30112-30116 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-duplicateinWinContext {
  --menuitem-image: url("../icons/tab-desktop-multiple-bottom.svg");
}

/* leptonChrome.css:30117-30121 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-detachTabContext {
  --menuitem-image: url("../icons/convert-range.svg");
}

/* leptonChrome.css:30122-30126 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-mergeWindows {
  --menuitem-image: url("../icons/merge.svg");
}

/* leptonChrome.css:30127-30131 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-content-freezeTab {
  --menuitem-image: url("../icons/weather-snowflake.svg");
}

/* leptonChrome.css:30132-30136 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-content-protectTab {
  --menuitem-image: url("../icons/shield-task.svg");
}

/* leptonChrome.css:30137-30141 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-content-lockTab {
  --menuitem-image: url("../icons/lock-closed.svg");
}

/* leptonChrome.css:30142-30146 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#tm-content-undoCloseTab {
  --menuitem-image: url("../icons/undo.svg");
}

/* leptonChrome.css:30147-30151 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.downloadPauseMenuItem {
  --menuitem-image: url("chrome://global/skin/media/pause-fill.svg");
}

/* leptonChrome.css:30152-30156 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.downloadResumeMenuItem {
  --menuitem-image: url("chrome://global/skin/media/play-fill.svg");
}

/* leptonChrome.css:30157-30161 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.downloadUnblockMenuItem {
  --menuitem-image: url("../icons/checkmark-circle.svg");
}

/* leptonChrome.css:30162-30166 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.downloadUseSystemDefaultMenuItem {
  --menuitem-image: url("../icons/toolbarButton-upload.svg");
}

/* leptonChrome.css:30167-30172 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.downloadAlwaysUseSystemDefaultMenuItem {
  /* checkbox */
  --menuitem-image: url("../icons/folder-globe.svg");
}

/* leptonChrome.css:30173-30178 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.downloadAlwaysOpenSimilarFilesMenuItem {
  /* checkbox */
  --menuitem-image: url("../icons/fluid.svg");
}

/* leptonChrome.css:30179-30183 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.downloadShowMenuItem {
  --menuitem-image: var(--uc-folder-icon);
}

/* leptonChrome.css:30184-30193 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#downloadsContextMenu > menuitem.downloadOpenReferrerMenuItem,
#downloadsContextMenu > menuitem[command="downloadsCmd_openReferrer"] {
  --menuitem-image: url("../icons/link-square.svg");
}
#downloadsContextMenu > menuitem.downloadCopyLocationMenuItem,
#downloadsContextMenu > menuitem[command="downloadsCmd_copyLocation"] {
  --menuitem-image: url("../icons/link.svg");
}

/* leptonChrome.css:30194-30198 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.downloadDeleteFileMenuItem {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:30199-30203 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.downloadRemoveFromHistoryMenuItem {
  --menuitem-image: url("../icons/eraser.svg");
}

/* leptonChrome.css:30204-30209 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#downloadsContextMenu > menuitem[command="downloadsCmd_clearList"],
#downloadsContextMenu > menuitem[command="downloadsCmd_clearDownloads"] {
  --menuitem-image: url("../icons/broom.svg");
}

/* leptonChrome.css:30210-30214 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_open {
  --menuitem-image: url("../icons/link-square.svg");
}

/* leptonChrome.css:30215-30220 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_openBookmarkContainer\\:tabs,
#placesContext_openBookmarkLinks\\:tabs {
  --menuitem-image: url("../icons/movetowindow-16.svg");
}

/* leptonChrome.css:30221-30227 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_open\\:newtab,
#placesContext_openContainer\\:tabs,
#placesContext_openLinks\\:tabs {
  --menuitem-image: var(--uc-new-tab-icon);
}

/* leptonChrome.css:30228-30232 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_open\\:newcontainertab {
  --menuitem-image: url("../icons/container-openin-16.svg");
}

/* leptonChrome.css:30233-30237 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_open\\:newwindow {
  --menuitem-image: url("chrome://browser/skin/window.svg");
}

/* leptonChrome.css:30238-30242 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_open\\:newprivatewindow {
  --menuitem-image: url("chrome://browser/skin/privateBrowsing.svg");
}

/* leptonChrome.css:30243-30249 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_show_bookmark\\:info,
#placesContext_show\\:info,
#placesContext_show_folder\\:info {
  --menuitem-image: url("../icons/edit-active.svg");
}

/* leptonChrome.css:30250-30257 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_deleteBookmark,
#placesContext_deleteFolder,
#placesContext_delete,
#placesContext_delete_history {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:30258-30262 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_deleteHost {
  --menuitem-image: url("../icons/eye-hide.svg");
}

/* leptonChrome.css:30263-30267 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_sortBy\\:name {
  --menuitem-image: url("../icons/text-sort-ascending.svg");
}

/* leptonChrome.css:30268-30272 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_cut {
  --menuitem-image: url("../icons/edit-cut.svg");
}

/* leptonChrome.css:30273-30277 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_copy {
  --menuitem-image: url("../icons/edit-copy.svg");
}

/* leptonChrome.css:30278-30282 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_paste_group {
  --menuitem-image: url("../icons/edit-paste.svg");
}

/* leptonChrome.css:30283-30287 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_new\\:bookmark {
  --menuitem-image: url("chrome://browser/skin/bookmark.svg");
}

/* leptonChrome.css:30288-30293 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_showInFolder,
#placesContext_new\\:folder {
  --menuitem-image: var(--uc-folder-icon);
}

/* leptonChrome.css:30294-30298 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_new\\:separator {
  --menuitem-image: url("../icons/vertical-line.svg");
}

/* leptonChrome.css:30299-30303 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_paste {
  --menuitem-image: url("../icons/edit-paste.svg");
}

/* leptonChrome.css:30304-30308 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_createBookmark {
  --menuitem-image: url("chrome://browser/skin/bookmark.svg");
}

/* leptonChrome.css:30309-30314 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#show-other-bookmarks_PersonalToolbar {
  /* checkbox */
  --menuitem-image: url("../icons/star-line-horizontal.svg");
}

/* leptonChrome.css:30315-30319 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#placesContext_showAllBookmarks {
  --menuitem-image: url("chrome://browser/skin/bookmark-star-on-tray.svg");
}

/* leptonChrome.css:30320-30324 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.openintabs-menuitem {
  --menuitem-image: url("../icons/movetowindow-16.svg");
}

/* leptonChrome.css:30325-30329 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#doNotDisturbMenuItem {
  --menuitem-image: url(chrome://global/skin/media/pause-fill.svg);
}

/* leptonChrome.css:30330-30334 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#disableForOriginMenuItem {
  --menuitem-image: url("chrome://global/skin/icons/blocked.svg");
}

/* leptonChrome.css:30335-30339 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#openSettingsMenuItem {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}

/* leptonChrome.css:30340-30344 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#blockedPopupAllowSite {
  --menuitem-image: url("chrome://global/skin/icons/check.svg");
}

/* leptonChrome.css:30345-30349 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#blockedPopupOptions > menuitem[oncommand="gPopupBlockerObserver.editPopupSettings();"] {
  --menuitem-image: url("../icons/edit-active.svg");
}

/* leptonChrome.css:30350-30355 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#blockedPopupDontShowMessage {
  /* checkbox */
  --menuitem-image: url("chrome://global/skin/icons/blocked.svg");
}

/* leptonChrome.css:30356-30361 \u2014 extra conditions dropped: userChrome.icon.context_menu */
menuitem[data-l10n-id="popup-show-popup-menuitem"] {
  /* checkbox */
  --menuitem-image: url("../icons/eye-show.svg");
}

/* leptonChrome.css:30362-30367 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#autohide-context > menuitem[data-l10n-id="full-screen-autohide"] {
  /* checkbox */
  --menuitem-image: url("../icons/eye-tracking-off.svg");
}

/* leptonChrome.css:30368-30372 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#autohide-context > menuitem[data-l10n-id="full-screen-exit"] {
  --menuitem-image: url("chrome://browser/skin/fullscreen-exit.svg");
}

/* leptonChrome.css:30373-30377 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#pictureInPictureToggleContextMenu > menuitem[oncommand="PictureInPicture.hideToggle();"] {
  --menuitem-image: url("../icons/eye-hide.svg");
}

/* leptonChrome.css:30378-30385 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.pageActionContextMenuItem.extensionPinned.extensionUnpinned.manageExtensionItem {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}
.pageActionContextMenuItem.extensionPinned.extensionUnpinned.removeExtensionItem {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:30386-30390 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#customizationPanelItemContextMenuUnpin {
  --menuitem-image: url("../icons/unpin-tab.svg");
}

/* leptonChrome.css:30391-30395 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.customize-context-removeFromPanel {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:30396-30400 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.customize-context-addToToolbar {
  --menuitem-image: url("chrome://devtools/skin/images/dock-bottom.svg");
}

/* leptonChrome.css:30401-30405 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.customize-context-addToPanel {
  --menuitem-image: url("chrome://browser/skin/menu.svg");
}

/* leptonChrome.css:30406-30410 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#customizationPanelContextMenu > menuitem[command="cmd_CustomizeToolbars"] {
  --menuitem-image: url("chrome://browser/skin/customize.svg");
}

/* leptonChrome.css:30411-30416 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#downloads-button-autohide-checkbox {
  /* checkbox */
  --menuitem-image: url("../icons/eye-tracking-off.svg");
}

/* leptonChrome.css:30417-30421 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#syncedTabsOpenSelected {
  --menuitem-image: url("../icons/link-square.svg");
}

/* leptonChrome.css:30422-30426 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#syncedTabsOpenSelectedInTab {
  --menuitem-image: var(--uc-new-tab-icon);
}

/* leptonChrome.css:30427-30431 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#syncedTabsOpenSelectedInWindow {
  --menuitem-image: url("chrome://browser/skin/window.svg");
}

/* leptonChrome.css:30432-30436 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#syncedTabsOpenSelectedInPrivateWindow {
  --menuitem-image: url("chrome://browser/skin/privateBrowsing.svg");
}

/* leptonChrome.css:30437-30441 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#syncedTabsBookmarkSelected {
  --menuitem-image: url("chrome://browser/skin/bookmark.svg");
}

/* leptonChrome.css:30442-30446 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#syncedTabsCopySelected {
  --menuitem-image: url("../icons/link.svg");
}

/* leptonChrome.css:30447-30451 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#syncedTabsOpenAllInTabs {
  --menuitem-image: url("../icons/movetowindow-16.svg");
}

/* leptonChrome.css:30452-30456 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#syncedTabsManageDevices {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}

/* leptonChrome.css:30457-30461 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#syncedTabsRefresh {
  --menuitem-image: url("chrome://browser/skin/sync.svg");
}

/* leptonChrome.css:30462-30466 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#SyncedTabsSidebarTabsFilterContext > menuitem[cmd="cmd_undo"] {
  --menuitem-image: url("../icons/undo.svg");
}

/* leptonChrome.css:30467-30471 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#SyncedTabsSidebarTabsFilterContext > menuitem[cmd="cmd_cut"] {
  --menuitem-image: url("../icons/edit-cut.svg");
}

/* leptonChrome.css:30472-30476 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#SyncedTabsSidebarTabsFilterContext > menuitem[cmd="cmd_copy"] {
  --menuitem-image: url("../icons/edit-copy.svg");
}

/* leptonChrome.css:30477-30481 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#SyncedTabsSidebarTabsFilterContext > menuitem[cmd="cmd_paste"] {
  --menuitem-image: url("../icons/edit-paste.svg");
}

/* leptonChrome.css:30482-30486 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#SyncedTabsSidebarTabsFilterContext > menuitem[cmd="cmd_delete"] {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:30487-30491 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#SyncedTabsSidebarTabsFilterContext > menuitem[cmd="cmd_selectAll"] {
  --menuitem-image: url("../icons/select-all-on.svg");
}

/* leptonChrome.css:30492-30496 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#syncedTabsRefreshFilter {
  --menuitem-image: url("chrome://browser/skin/sync.svg");
}

/* leptonChrome.css:30497-30501 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#urlbar-input-container .textbox-contextmenu menuitem[cmd="cmd_undo"] {
  --menuitem-image: url("../icons/undo.svg");
}

/* leptonChrome.css:30507-30511 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#urlbar-input-container .textbox-contextmenu menuitem[cmd="cmd_cut"] {
  --menuitem-image: url("../icons/edit-cut.svg");
}

/* leptonChrome.css:30512-30516 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#urlbar-input-container .textbox-contextmenu menuitem[cmd="cmd_copy"] {
  --menuitem-image: url("../icons/edit-copy.svg");
}

/* leptonChrome.css:30522-30526 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#urlbar-input-container .textbox-contextmenu menuitem[cmd="cmd_paste"] {
  --menuitem-image: url("../icons/edit-paste.svg");
}

/* leptonChrome.css:30532-30536 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#urlbar-input-container .textbox-contextmenu menuitem[cmd="cmd_delete"] {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:30537-30541 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#urlbar-input-container .textbox-contextmenu menuitem[cmd="cmd_selectAll"] {
  --menuitem-image: url("../icons/select-all-on.svg");
}

/* leptonChrome.css:30542-30546 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.textbox-contextmenu > menuitem[data-l10n-id="text-action-undo"] {
  --menuitem-image: url("../icons/undo.svg");
}

/* leptonChrome.css:30552-30556 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.textbox-contextmenu > menuitem[data-l10n-id="text-action-cut"] {
  --menuitem-image: url("../icons/edit-cut.svg");
}

/* leptonChrome.css:30557-30561 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.textbox-contextmenu > menuitem[data-l10n-id="text-action-copy"] {
  --menuitem-image: url("../icons/edit-copy.svg");
}

/* leptonChrome.css:30562-30566 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.textbox-contextmenu > menuitem[data-l10n-id="text-action-paste"] {
  --menuitem-image: url("../icons/edit-paste.svg");
}

/* leptonChrome.css:30567-30571 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.textbox-contextmenu > menuitem[data-l10n-id="text-action-delete"] {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:30572-30576 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.textbox-contextmenu > menuitem[data-l10n-id="text-action-select-all"] {
  --menuitem-image: url("../icons/select-all-on.svg");
}

/* leptonChrome.css:30582-30586 \u2014 extra conditions dropped: userChrome.icon.context_menu */
menuitem.searchbar-clear-history {
  --menuitem-image: url("../icons/forget.svg");
}

/* leptonChrome.css:30587-30609 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.sync-menuitem.sendtab-target[clientType="phone"] {
  --menuitem-image: url("../icons/device-phone.svg");
}
.sync-menuitem.sendtab-target[clientType="tablet"] {
  --menuitem-image: url("../icons/device-tablet.svg");
}
.sync-menuitem.sendtab-target[clientType="desktop"] {
  --menuitem-image: url("../icons/device-desktop.svg");
}
.sync-menuitem.sendtab-target[clientType="tv"] {
  --menuitem-image: url("../icons/device-tv.svg");
}
.sync-menuitem.sendtab-target[clientType="vr"] {
  --menuitem-image: url("../icons/device-vr.svg");
}
.sync-menuitem.sendtab-target[clientType=""] {
  --menuitem-image: url("../icons/send-to-device.svg");
}
.sync-menuitem.sendtab-target:not([clientType]) {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}

/* leptonChrome.css:30610-30614 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#treestyletab_piro_sakura_ne_jp-menuitem-_context_sendTabsToDevice\\:manage {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}

/* leptonChrome.css:30615-30619 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.unified-extensions-context-menu-pin-to-toolbar {
  --menuitem-image: url("../icons/pin-tab.svg");
}

/* leptonChrome.css:30620-30624 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.unified-extensions-context-menu-move-widget-up {
  --menuitem-image: url("chrome://global/skin/icons/arrow-up.svg");
}

/* leptonChrome.css:30625-30629 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.unified-extensions-context-menu-move-widget-down {
  --menuitem-image: url("chrome://global/skin/icons/arrow-down.svg");
}

/* leptonChrome.css:30630-30634 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.unified-extensions-context-menu-manage-extension {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}

/* leptonChrome.css:30635-30639 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.unified-extensions-context-menu-remove-extension {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:30640-30644 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.unified-extensions-context-menu-report-extension {
  --menuitem-image: url("../icons/send.svg");
}

/* leptonChrome.css:30645-30655 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.urlbarView-result-menuitem[data-command="trendingblock"] {
  --menuitem-image: url("../icons/eye-hide.svg");
}
.urlbarView-result-menuitem[data-command="dismiss"] {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}
.urlbarView-result-menuitem[data-command="help"] {
  --menuitem-image: url("chrome://global/skin/icons/info.svg");
}

/* leptonChrome.css:30656-30660 \u2014 extra conditions dropped: userChrome.icon.context_menu */
.manage-languages-menuitem {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}

/* leptonChrome.css:30661-30665 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#full-page-translations-panel-settings-menupopup > menuitem[data-l10n-id="translations-panel-settings-about2"] {
  --menuitem-image: url("chrome://global/skin/icons/info.svg");
}

/* leptonChrome.css:30666-30670 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#select-translations-panel-open-settings-page-menuitem {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}

/* leptonChrome.css:30671-30675 \u2014 extra conditions dropped: userChrome.icon.context_menu */
#select-translations-panel-about-translations-menuitem {
  --menuitem-image: url("chrome://global/skin/icons/info.svg");
}

/* leptonChrome.css:30676-30718 \u2014 extra conditions dropped: userChrome.icon.global_menubar */
/*= main-menubar =============================================================*/
#file-menu {
  --menuitem-image: url("../icons/mail-inbox-all.svg");
}
#edit-menu {
  --menuitem-image: url("../icons/edit-active.svg");
}
#view-menu {
  --menuitem-image: url("../icons/content-view.svg");
}
#history-menu {
  --menuitem-image: url("chrome://browser/skin/history.svg");
}
#bookmarksMenu {
  --menuitem-image: url("chrome://browser/skin/bookmark.svg");
}
#profiles-menu:is(menu) {
  fill: transparent !important;
  --menuitem-image: url("chrome://browser/content/profiles/assets/16_heart.svg");
}
#tools-menu {
  --menuitem-image: url("../icons/toolbox.svg");
}
#tabSharingMenu {
  --menuitem-image: url("chrome://browser/skin/notification-icons/screen.svg");
}
#helpMenu {
  --menuitem-image: url("chrome://global/skin/icons/help.svg");
}
/*= Others ===================================================================*/
#alltabs-button {
  list-style-image: url("chrome://global/skin/icons/arrow-down.svg") !important;
}
#alltabs-button > .toolbarbutton-badge-stack > .toolbarbutton-icon {
  /* Original
     width: 20px;
     margin: -2px;
   */
  width: 16px !important;
  margin: 0 !important;
}

/* leptonChrome.css:30719-30754 \u2014 extra conditions dropped: userChrome.icon.global_menu */
/** Global Menu ***************************************************************/
/*= menu_FilePopup ===========================================================*/
#menu_newNavigatorTab {
  --menuitem-image: var(--uc-new-tab-icon);
}
#menu_newUserContext {
  --menuitem-image: url("../icons/container-openin-16.svg");
}
#menu_newNavigator {
  --menuitem-image: url("chrome://browser/skin/window.svg");
}
#menu_newPrivateWindow {
  --menuitem-image: url("chrome://browser/skin/privateBrowsing.svg");
}
#menu_openFile {
  --menuitem-image: url("../icons/toolbarButton-upload.svg");
}
#menu_close {
  --menuitem-image: var(--uc-tab-close-icon);
}
/* Mange Containers */
/*= menu_EditPopup ===========================================================*/
/*= menu_viewPopup ===========================================================*/
/* view-menu-popup sub menu */
/* viewSidebarMenu sub menu */
/* viewFullZoomMenu sub menu */
/*= goPopup ==================================================================*/
/* sub menu */
/*= bookmarksMenuPopup =======================================================*/
/*= menu_ProfilesPopup =======================================================*/
/*= menu_ToolsPopup ==========================================================*/
/* menuWebDeveloperPopup sub menu */
/*= windowPopup ==============================================================*/
/*= menu_HelpPopup ===========================================================*/

/* leptonChrome.css:30760-30764 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_savePage {
  --menuitem-image: url("../icons/toolbarButton-download.svg");
}

/* leptonChrome.css:30765-30769 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_sendLink {
  --menuitem-image: url("../icons/mail.svg");
}

/* leptonChrome.css:30770-30774 \u2014 extra conditions dropped: userChrome.icon.global_menu */
menu.share-tab-url-item {
  --menuitem-image: url("chrome://browser/skin/share.svg");
}

/* leptonChrome.css:30775-30779 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_print {
  --menuitem-image: url("chrome://global/skin/icons/print.svg");
}

/* leptonChrome.css:30780-30784 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_importFromAnotherBrowser {
  --menuitem-image: url("chrome://browser/skin/import.svg");
}

/* leptonChrome.css:30785-30790 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#goOfflineMenuitem {
  /* checkbox */
  --menuitem-image: url("../icons/plug-disconnected.svg");
}

/* leptonChrome.css:30791-30795 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_FileQuitItem {
  --menuitem-image: url("../icons/quit.svg");
}

/* leptonChrome.css:30796-30800 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_newUserContext menupopup menuitem:last-child {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}

/* leptonChrome.css:30801-30805 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_undo {
  --menuitem-image: url("../icons/undo.svg");
}

/* leptonChrome.css:30811-30815 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_cut {
  --menuitem-image: url("../icons/edit-cut.svg");
}

/* leptonChrome.css:30816-30820 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_copy {
  --menuitem-image: url("../icons/edit-copy.svg");
}

/* leptonChrome.css:30821-30825 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_paste {
  --menuitem-image: url("../icons/edit-paste.svg");
}

/* leptonChrome.css:30826-30830 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_delete {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:30831-30835 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_selectAll {
  --menuitem-image: url("../icons/select-all-on.svg");
}

/* leptonChrome.css:30836-30840 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_find {
  --menuitem-image: url("chrome://global/skin/icons/search-glass.svg");
}

/* leptonChrome.css:30846-30850 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_preferences {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}

/* leptonChrome.css:30851-30855 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#viewToolbarsMenu {
  --menuitem-image: url("../icons/toolbar.svg");
}

/* leptonChrome.css:30856-30860 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#viewSidebarMenuMenu {
  --menuitem-image: var(--uc-sidebar-icon);
}

/* leptonChrome.css:30861-30865 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#viewFullZoomMenu {
  --menuitem-image: url("../icons/screenshot.svg");
}

/* leptonChrome.css:30866-30870 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#pageStyleMenu {
  --menuitem-image: url("../icons/document-css.svg");
}

/* leptonChrome.css:30871-30875 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#repair-text-encoding {
  --menuitem-image: url("../icons/characterEncoding.svg");
}

/* leptonChrome.css:30876-30880 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#enterFullScreenItem {
  --menuitem-image: url("chrome://browser/skin/fullscreen.svg");
}

/* leptonChrome.css:30881-30885 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#exitFullScreenItem {
  --menuitem-image: url("chrome://browser/skin/fullscreen-exit.svg");
}

/* leptonChrome.css:30886-30891 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#fullScreenItem {
  /* checkbox */
  --menuitem-image: url("chrome://browser/skin/fullscreen.svg");
}

/* leptonChrome.css:30892-30896 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_readerModeItem {
  --menuitem-image: url("chrome://browser/skin/reader-mode.svg");
}

/* leptonChrome.css:30897-30901 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_showAllTabs {
  --menuitem-image: var(--uc-tab-icon);
}

/* leptonChrome.css:30902-30906 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#documentDirection-swap {
  --menuitem-image: url("../icons/text-direction-horizontal-ltr.svg");
}

/* leptonChrome.css:30907-30911 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_customizeToolbars {
  --menuitem-image: url("chrome://browser/skin/customize.svg");
}

/* leptonChrome.css:30912-30916 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_bookmarksSidebar {
  --menuitem-image: url("chrome://browser/skin/bookmark.svg");
}

/* leptonChrome.css:30917-30921 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_historySidebar {
  --menuitem-image: url("chrome://browser/skin/history.svg");
}

/* leptonChrome.css:30922-30926 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_tabsSidebar {
  --menuitem-image: url("../icons/synced-tabs.svg");
}

/* leptonChrome.css:30927-30932 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_zoomEnlarge {
  --menuitem-image: url("chrome://browser/skin/add-circle-fill.svg");
  stroke: transparent !important;
}

/* leptonChrome.css:30933-30938 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_zoomReduce {
  --menuitem-image: url("chrome://browser/skin/subtract-circle-fill.svg");
  stroke: transparent !important;
}

/* leptonChrome.css:30939-30943 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_zoomReset {
  --menuitem-image: url("../icons/resize.svg");
}

/* leptonChrome.css:30944-30948 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#toggle_zoom {
  --menuitem-image: url("../icons/screenshot.svg");
}

/* leptonChrome.css:30949-30953 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_showAllHistory {
  --menuitem-image: url("chrome://browser/skin/history.svg");
}

/* leptonChrome.css:30954-30958 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#sanitizeItem {
  --menuitem-image: url("../icons/forget.svg");
}

/* leptonChrome.css:30959-30963 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#sync-tabs-menuitem {
  --menuitem-image: url("chrome://browser/skin/sync.svg");
}

/* leptonChrome.css:30964-30968 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#historyRestoreLastSession {
  --menuitem-image: url("../icons/restore-session.svg");
}

/* leptonChrome.css:30969-30973 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#hiddenTabsMenu {
  --menuitem-image: url("../icons/eye-hide.svg");
}

/* leptonChrome.css:30974-30978 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_searchHistory {
  --menuitem-image: url("chrome://global/skin/icons/search-glass.svg");
}

/* leptonChrome.css:30979-30983 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#historyUndoMenu {
  --menuitem-image: var(--uc-tab-icon);
}

/* leptonChrome.css:30984-30988 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#historyUndoWindowMenu {
  --menuitem-image: url("chrome://browser/skin/window.svg");
}

/* leptonChrome.css:30989-30993 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#historyUndoPopup .restoreallitem {
  --menuitem-image: url("../icons/movetowindow-16.svg");
}

/* leptonChrome.css:30994-30998 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#historyUndoWindowPopup .restoreallitem {
  --menuitem-image: url("../icons/restore-session.svg");
}

/* leptonChrome.css:30999-31003 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#bookmarksShowAll {
  --menuitem-image: url("chrome://browser/skin/bookmark-star-on-tray.svg");
}

/* leptonChrome.css:31004-31009 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_bookmarkThisPage,
#menu_bookmarkAllTabs {
  --menuitem-image: url("chrome://browser/skin/bookmark-hollow.svg");
}

/* leptonChrome.css:31010-31014 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_bookmarkThisPage[data-l10n-id="menu-bookmark-edit"] {
  --menuitem-image: url("chrome://browser/skin/bookmark.svg");
}

/* leptonChrome.css:31015-31019 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_searchBookmarks {
  --menuitem-image: url("chrome://global/skin/icons/search-glass.svg");
}

/* leptonChrome.css:31020-31024 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_manageProfiles {
  --menuitem-image: url("chrome://global/skin/icons/settings.svg");
}

/* leptonChrome.css:31025-31029 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_openDownloads {
  --menuitem-image: url("chrome://browser/skin/downloads/downloads.svg");
}

/* leptonChrome.css:31030-31034 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_openAddons {
  --menuitem-image: url("chrome://mozapps/skin/extensions/extension.svg");
}

/* leptonChrome.css:31035-31039 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#sync-setup {
  --menuitem-image: url("chrome://browser/skin/fxa/avatar-empty.svg");
}

/* leptonChrome.css:31040-31044 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#sync-syncnowitem {
  --menuitem-image: url("chrome://browser/skin/sync.svg");
}

/* leptonChrome.css:31045-31049 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_openFirefoxView {
  --menuitem-image: url("../icons/firefox-view.svg");
}

/* leptonChrome.css:31050-31055 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#webDeveloperMenu,
#browserToolsMenu {
  --menuitem-image: url("../icons/developer.svg");
}

/* leptonChrome.css:31056-31060 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_pageInfo {
  --menuitem-image: url("../icons/document-endnote.svg");
}

/* leptonChrome.css:31061-31066 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_devToolbox {
  /* checkbox */
  --menuitem-image: url("../icons/developer.svg");
}

/* leptonChrome.css:31067-31071 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_taskManager {
  --menuitem-image: url("../icons/performance.svg");
}

/* leptonChrome.css:31072-31076 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_devtools_remotedebugging {
  --menuitem-image: url("../icons/bug.svg");
}

/* leptonChrome.css:31077-31081 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_browserToolbox {
  --menuitem-image: url("../icons/window-dev-tools.svg");
}

/* leptonChrome.css:31082-31086 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_browserContentToolbox {
  --menuitem-image: url("../icons/command-frames.svg");
}

/* leptonChrome.css:31087-31091 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_browserConsole {
  --menuitem-image: url("../icons/command-console.svg");
}

/* leptonChrome.css:31092-31097 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_responsiveUI {
  /* checkbox */
  --menuitem-image: url("../icons/command-responsivemode.svg");
}

/* leptonChrome.css:31098-31103 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_eyedropper {
  /* checkbox */
  --menuitem-image: url("../icons/command-eyedropper.svg");
}

/* leptonChrome.css:31104-31108 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_pageSource {
  --menuitem-image: url("../icons/document-search.svg");
}

/* leptonChrome.css:31109-31113 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#extensionsForDevelopers {
  --menuitem-image: url("chrome://mozapps/skin/extensions/extension.svg");
}

/* leptonChrome.css:31114-31121 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#windowPopup > menuitem[command="minimizeWindow"] {
  --menuitem-image: url("../icons/arrow-between-down.svg");
}
#windowPopup > menuitem[command="zoomWindow"] {
  --menuitem-image: url("../icons/auto-fit-width.svg");
}

/* leptonChrome.css:31122-31126 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_openHelp {
  --menuitem-image: url("chrome://global/skin/icons/help.svg");
}

/* leptonChrome.css:31127-31131 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#help_reportBrokenSite {
  --menuitem-image: url("chrome://global/skin/icons/lightbulb.svg");
}

/* leptonChrome.css:31132-31136 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#feedbackPage {
  --menuitem-image: url("../icons/send.svg");
}

/* leptonChrome.css:31137-31141 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#helpSafeMode {
  --menuitem-image: url("chrome://devtools/skin/images/debugging-workers.svg");
}

/* leptonChrome.css:31142-31146 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#troubleShooting {
  --menuitem-image: url("chrome://global/skin/icons/more.svg");
}

/* leptonChrome.css:31147-31151 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#help_reportSiteIssue {
  --menuitem-image: url("chrome://global/skin/icons/lightbulb.svg");
}

/* leptonChrome.css:31152-31156 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_HelpPopup_reportPhishingtoolmenu {
  --menuitem-image: url("chrome://global/skin/icons/warning.svg");
}

/* leptonChrome.css:31157-31161 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#menu_HelpPopup_reportPhishingErrortoolmenu {
  --menuitem-image: url("../icons/checkmark-circle.svg");
}

/* leptonChrome.css:31162-31166 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#helpSwitchDevice {
  --menuitem-image: url("../icons/add-device.svg");
}

/* leptonChrome.css:31167-31171 \u2014 extra conditions dropped: userChrome.icon.global_menu */
#aboutName {
  --menuitem-image: url("chrome://global/skin/icons/info.svg");
}

/* leptonChrome.css:31172-31188 \u2014 extra conditions dropped: userChrome.icon.library */
/*= organizeButtonPopup ======================================================*/
#newbookmark {
  --menuitem-image: url("chrome://browser/skin/bookmark.svg");
}
#newfolder {
  --menuitem-image: var(--uc-folder-icon);
}
#newseparator {
  --menuitem-image: url("../icons/vertical-line.svg");
}
#orgUndo {
  --menuitem-image: url("../icons/undo.svg");
}
/*= viewMenuPopup ============================================================*/
/*= maintenanceButtonPopup ===================================================*/

/* leptonChrome.css:31194-31198 \u2014 extra conditions dropped: userChrome.icon.library */
#orgCut {
  --menuitem-image: url("../icons/edit-cut.svg");
}

/* leptonChrome.css:31199-31203 \u2014 extra conditions dropped: userChrome.icon.library */
#orgCopy {
  --menuitem-image: url("../icons/edit-copy.svg");
}

/* leptonChrome.css:31204-31208 \u2014 extra conditions dropped: userChrome.icon.library */
#orgPaste {
  --menuitem-image: url("../icons/edit-paste.svg");
}

/* leptonChrome.css:31209-31213 \u2014 extra conditions dropped: userChrome.icon.library */
#orgDelete {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}

/* leptonChrome.css:31214-31218 \u2014 extra conditions dropped: userChrome.icon.library */
#orgSelectAll {
  --menuitem-image: url("../icons/select-all-on.svg");
}

/* leptonChrome.css:31219-31223 \u2014 extra conditions dropped: userChrome.icon.library */
#orgClose {
  --menuitem-image: url("chrome://global/skin/icons/close.svg");
}

/* leptonChrome.css:31224-31228 \u2014 extra conditions dropped: userChrome.icon.library */
#viewColumns {
  --menuitem-image: url("chrome://global/skin/icons/columnpicker.svg");
}

/* leptonChrome.css:31229-31233 \u2014 extra conditions dropped: userChrome.icon.library */
#viewSort {
  --menuitem-image: url("../icons/text-sort-ascending.svg");
}

/* leptonChrome.css:31234-31238 \u2014 extra conditions dropped: userChrome.icon.library */
#backupBookmarks {
  --menuitem-image: url("../icons/datastore.svg");
}

/* leptonChrome.css:31244-31248 \u2014 extra conditions dropped: userChrome.icon.library */
#fileImport {
  --menuitem-image: url("../icons/toolbarButton-download.svg");
}

/* leptonChrome.css:31249-31253 \u2014 extra conditions dropped: userChrome.icon.library */
#fileExport {
  --menuitem-image: url("../icons/toolbarButton-upload.svg");
}

/* leptonChrome.css:31254-31258 \u2014 extra conditions dropped: userChrome.icon.library */
#browserImport {
  --menuitem-image: url("chrome://browser/skin/import.svg");
}

/* leptonChrome.css:31288-31301 */
#menu_FileRestartItem {
  --menuitem-image: url("../icons/refresh-cw.svg");
}
#toggle_status-dummybar {
  --menuitem-image: url("../icons/pulse-square.svg");
}
menuitem.privatetab-icon {
  --menuitem-image: url("../icons/private-favicon.svg");
}
#sidebar_waterfox_net-menuitem-_openAllBookmarksWithStructure {
  --menuitem-image: url("../icons/organization-horizontal.svg");
}

/* leptonChrome.css:31313-31323 */
#menu_newIdentity {
  --menuitem-image: url("chrome://browser/skin/new_identity.svg");
}
#menu_newCircuit {
  --menuitem-image: url("chrome://browser/skin/new_circuit.svg");
}
#torBrowserUserManual {
  --menuitem-image: url("chrome://browser/skin/onion.svg");
}

/* leptonChrome.css:31349-31401 */
#toggle_sharemode {
  --menuitem-image: url("chrome://branding/content/about-logo-private.png");
}
#usercssloader-menu {
  --menuitem-image: url("../icons/developer.svg");
}
#usercssloader-menupopup > menu[data-l10n-id="css-menu"] {
  --menuitem-image: url("../icons/document-css.svg");
}
#usercssloader-submenupopup > menuitem[data-l10n-id="rebuild-css"] {
  --menuitem-image: url("chrome://global/skin/icons/reload.svg");
}
#usercssloader-submenupopup > menuitem[data-l10n-id="make-browsercss-file"] {
  --menuitem-image: url("../icons/edit-active.svg");
}
#usercssloader-submenupopup > menuitem[data-l10n-id="open-css-folder"] {
  --menuitem-image: var(--uc-folder-icon);
}
#usercssloader-submenupopup > menuitem[data-l10n-id="edit-userChromeCss-editor"] {
  --menuitem-image: url("chrome://browser/skin/window.svg");
}
#usercssloader-submenupopup > menuitem[data-l10n-id="edit-userContentCss-editor"] {
  --menuitem-image: url("chrome://global/skin/icons/page-portrait.svg");
}
#context_toggleToPrivateContainer,
#open_in_private_container {
  --menuitem-image: url("../icons/private-favicon.svg");
}
#toggle_statusBar {
  --menuitem-image: url("../icons/pulse-square.svg");
}
#muteMenu {
  --menuitem-image: url("chrome://browser/skin/tabbrowser/tab-audio-muted-small.svg");
  stroke: transparent !important;
}
#unloadWebpanelMenu {
  --menuitem-image: var(--uc-tab-unload-icon);
}
#changeUAWebpanelMenu {
  --menuitem-image: url("../icons/command-responsivemode.svg");
  fill-opacity: 0;
}
#deleteWebpanelMenu {
  --menuitem-image: url("chrome://global/skin/icons/delete.svg");
}
#run-ssb-contextmenu {
  --menuitem-image: url("../icons/pwa-launch.svg");
}
#uninstall-ssb-contextmenu {
  --menuitem-image: url("../icons/pwa-remove.svg");
}
`;

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/sidebar-overlap.css?inline
  var sidebar_overlap_default = "/* Floorp native Firefox sidebar overlay. The native controller keeps ownership\n * of panel documents, width persistence, commands and sidebar positioning. */\n:root[floorp-firefox-sidebar-overlay] #browser,\n:root[floorp-firefox-sidebar-hover] #browser {\n  position: relative;\n}\n\n:root[floorp-firefox-sidebar-overlay] #sidebar-box:not([hidden]) {\n  position: absolute !important;\n  left: var(--floorp-firefox-sidebar-left, 0px) !important;\n  right: auto !important;\n  top: var(--floorp-firefox-sidebar-top, 0px) !important;\n  bottom: auto !important;\n  height: var(--floorp-firefox-sidebar-height, 100%) !important;\n  min-width: min(160px, var(--floorp-firefox-sidebar-available-width, 75vw))\n    !important;\n  max-width: var(--floorp-firefox-sidebar-available-width, 75vw) !important;\n  margin: 0 !important;\n  /* Lepton's overlay-only layout uses a full panel width as inline padding.\n   * The native panel now occupies that width itself. */\n  padding-inline: 0 !important;\n  transition: none !important;\n  box-sizing: border-box;\n  background-color: var(--sidebar-background-color,\n    var(--toolbar-background-color));\n  box-shadow: 0 2px 12px color-mix(in srgb, black 25%, transparent);\n  z-index: 6 !important;\n}\n\n:root[floorp-firefox-sidebar-overlay] #sidebar-splitter {\n  display: none !important;\n}\n\n/* Keep logical positioning in Firefox; the measured physical anchor above also\n * handles [sidebar-positionend], legacy [positionend], and RTL windows. */\n:root[floorp-firefox-sidebar-overlay] #sidebar-box[sidebar-positionend] {\n  direction: inherit;\n}\n\n/* Lepton's legacy auto-hide preference also constrains the embedded browser to\n * a 40px lane. Overlay collapse keeps the full document width, including when\n * the panel is pinned open and the pointer has moved away. */\n:root[floorp-firefox-sidebar-overlay] #sidebar-box :is(#sidebar-header, #sidebar) {\n  min-width: 0 !important;\n  max-width: none !important;\n  width: 100% !important;\n  box-sizing: border-box;\n  transition: none !important;\n}\n\n#floorp-firefox-sidebar-overlay-resizer {\n  position: absolute;\n  inset-block: 0;\n  right: -3px;\n  width: 6px;\n  cursor: ew-resize;\n  z-index: 1;\n  touch-action: none;\n}\n#sidebar-box[data-floorp-sidebar-right] > #floorp-firefox-sidebar-overlay-resizer {\n  right: auto;\n  left: -3px;\n}\n#floorp-firefox-sidebar-overlay-resizer:is(:hover, :focus-visible) {\n  background-color: var(--focus-outline-color);\n}\n#floorp-firefox-sidebar-overlay-resizer[hidden] {\n  display: none;\n}\n/* The launcher's pre-hover occupied space is reserved explicitly. Native panel\n * opening may reveal the launcher; that reveal must not resize web content. */\n:root[floorp-firefox-sidebar-overlay] #sidebar-container {\n  position: absolute !important;\n  left: var(--floorp-firefox-launcher-left, 0px) !important;\n  right: auto !important;\n  inset-block-start: var(--floorp-firefox-sidebar-top, 0px) !important;\n  inset-block-end: auto !important;\n  height: var(--floorp-firefox-sidebar-height, 100%) !important;\n  margin: 0 !important;\n  z-index: 7;\n}\n:root[floorp-firefox-sidebar-overlay] #sidebar-launcher-splitter {\n  display: none !important;\n}\n:root[floorp-firefox-sidebar-overlay] #tabbrowser-tabbox {\n  margin-left: var(--floorp-firefox-launcher-reserve-start, 0px) !important;\n  margin-right: var(--floorp-firefox-launcher-reserve-end, 0px) !important;\n}\n\n:root[inDOMFullscreen][floorp-firefox-sidebar-overlay] #tabbrowser-tabbox {\n  margin-left: 0 !important;\n  margin-right: 0 !important;\n}\n:root[inDOMFullscreen][floorp-firefox-sidebar-overlay] #sidebar-box {\n  visibility: collapse !important;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/tabbar-as-titlebar.css?inline
  var tabbar_as_titlebar_default = '/* Floorp chrome-extras: tabbarAsTitlebar\n *\n * Mostly a port, with one sub-option dropped.\n *\n * Source: leptonChrome.css:6841-6926 (`@media -moz-pref("userChrome.tabbar.as_titlebar")`).\n * That block is self-contained apart from two `--uc-*` tokens, both of which the\n * scaffold provides, so it ports directly instead of needing a rewrite.\n *\n * Not ported:\n *  - the `centered.tab.label` branch of the padding block \u2014 both non-label\n *    branches set identical values, and `centered.tab.label` is a Lepton-only\n *    sub-option;\n *  - the `browser.tabs.tabmanager.enabled` branch (a different Lepton feature);\n *  - `margin-inline-start: calc(var(--tab-shadow-max-size) * -1)`.\n *    `--tab-shadow-max-size` is not defined anywhere in Lepton or in Gecko 152,\n *    so upstream that declaration is already dead \u2014 dropping it is a no-op.\n */\n\n@media not -moz-pref("sidebar.verticalTabs") {\n  :root:not([tabsintitlebar]):not([customtitlebar]) #tabbrowser-tabs,\n  #scrollbutton-up,\n  #scrollbutton-down,\n  .titlebar-spacer[type="pre-tabs"],\n  spacer:is([part="overflow-start-indicator"], [part="overflow-end-indicator"]),\n  .tabbrowser-tab:not([selected]),\n  .tabbrowser-tab[selected] .tab-background,\n  .tabbrowser-tab[selected] .tab-stack:is(::before, ::after),\n  .tabbrowser-tab[selected] .tab-close-button,\n  #tabs-newtab-button {\n    display: none !important;\n  }\n\n  /* Override the vendor sheet\'s old value selectors without disabling its\n   * horizontal layout preferences. */\n  :root:is([tabsintitlebar], [customtitlebar]) #tabbrowser-tabs[orient="horizontal"],\n  #tabbrowser-tabs[orient="horizontal"] .tabbrowser-tab[selected] {\n    display: flex !important;\n  }\n\n  .tabbrowser-tab[selected] {\n    -moz-window-dragging: drag;\n    --tab-max-width: 100vw;\n    min-width: calc(var(--uc-tabbar-width, 100vw) -\n      var(--uc-window-control-space)) !important;\n    max-width: var(--tab-max-width) !important;\n  }\n\n  .tabbrowser-tab[selected][pinned] {\n    flex: 100;\n    -moz-box-flex: 100;\n    max-width: var(--tab-max-width) !important;\n  }\n\n  .tabbrowser-tab[selected] .tab-label-container {\n    margin-inline: 0 !important;\n  }\n\n  .tab-content {\n    margin-inline: auto;\n    width: 100%;\n  }\n\n  /* Pinned */\n  #tabbrowser-tabs[positionpinnedtabs] > #pinned-tabs-container > .tabbrowser-tab[pinned][selected],\n  #tabbrowser-tabs[positionpinnedtabs] > #tabbrowser-arrowscrollbox > .tabbrowser-tab[pinned][selected] {\n    position: relative !important;\n  }\n\n  #tabbrowser-tabs {\n    --tab-overflow-pinned-tabs-width: 0 !important;\n    padding-inline: 0\n      !important; /* Original: var(--tab-overflow-pinned-tabs-width) 0; */\n  }\n\n  .tab-throbber[pinned],\n  .tab-icon-pending[pinned],\n  .tab-icon-image[pinned],\n  .tab-sharing-icon-overlay[pinned],\n  .tab-icon-overlay[pinned] {\n    margin-inline-end: 5.5px;\n  }\n\n  .tab-label-container[pinned] {\n    width: unset !important;\n  }\n\n  /* Padding \u2014 the non-`centered.tab.label` branch, applied unconditionally. */\n  #pinned-tabs-container,\n  #tabbrowser-arrowscrollbox {\n    margin-inline: 2px !important;\n  }\n\n  :root[sizemode="normal"] #pinned-tabs-container,\n  :root[sizemode="normal"] #tabbrowser-arrowscrollbox {\n    margin-inline: 6px !important;\n  }\n}\n';

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/tabbar-one-liner.css?inline
  var tabbar_one_liner_default = '/* Floorp chrome-extras: tabbarOneLiner\n *\n * Ported with the Lepton variables inlined and the sub-options dropped.\n *\n * Source: leptonChrome.css:5191-5300 (`@media -moz-pref("userChrome.tabbar.one_liner")`\n * \u2192 `@media not -moz-pref("userChrome.tabbar.one_liner.responsive")`). That is\n * Lepton\'s default configuration; the whole region runs to ~2,500 lines because\n * every combination of `responsive` / `combine_navbar` / `tabbar_first` /\n * `fill_urlbar` / `on_bottom` is spelled out separately.\n *\n * Which branch was taken (and why):\n *  - `responsive` OFF  \u2192 the fixed-width layout, no media query;\n *  - `combine_navbar` OFF \u2192 nav-bar and TabsToolbar stay separate toolbars;\n *  - `tabbar_first` OFF  \u2192 nav-bar first, tab bar after it;\n *  - `fill_urlbar` OFF   \u2192 `--uc-navbar-width-origin: 40vw`.\n *\n * Those four are Lepton-only sub-options that Floorp does not expose, so the\n * default branch is what ships here. `--uc-tabbar-height` and\n * `--uc-tabbar-hide-height` come from scaffold.css; every other `--uc-*` token\n * is inlined as a literal below.\n */\n\n:root {\n  --uc-navbar-width-origin: 40vw;\n  --uc-navbar-width: var(--uc-navbar-width-origin);\n  --uc-tabbar-width: calc(100vw - var(--uc-navbar-width));\n  --uc-navbar-margin: calc(var(--uc-navbar-width) + var(--uc-navbar-gap, 0px));\n  --uc-navbar-gap: 10px;\n  --uc-navbar-block: 1.5px;\n}\n\n:root[uidensity="compact"] {\n  --uc-navbar-block: 1px;\n}\n\n:root[uidensity="touch"] {\n  --uc-navbar-block: 3px;\n}\n\n#nav-bar {\n  --uc-navbar-double-block: calc(2 * var(--uc-navbar-block, 0px));\n  height: calc(var(--uc-tabbar-height) - var(--uc-navbar-double-block)) !important;\n  margin-top: calc(var(--uc-tabbar-hide-height) + var(--uc-navbar-double-block)) !important;\n  margin-bottom: var(--uc-navbar-block, 0px) !important;\n}\n\n#titlebar,\n#toolbar-menubar,\n#TabsToolbar {\n  transform: translateY(var(--uc-navbar-block, 0px));\n}\n\n#TabsToolbar > .titlebar-buttonbox-container {\n  transform: translateY(calc(-1 * var(--uc-navbar-block, 0px)));\n}\n\n#navigator-toolbox {\n  position: relative;\n  z-index: 2;\n}\n\n#nav-bar {\n  --toolbarbutton-inner-padding: 6px; /* Original: 8px */\n  border-radius: var(--uc-rounding-toolbar, var(--tab-border-radius, 4px));\n}\n\n#nav-bar,\n#nav-bar-customization-target {\n  align-items: center;\n  -moz-box-align: center;\n}\n\n#urlbar-container {\n  min-height: calc(var(--urlbar-container-height) - 2px) !important;\n}\n\n#urlbar-container {\n  min-width: calc(\n    var(--uc-oneliner-urlbar-base-width, 50px) + 24px + 2 * var(--toolbarbutton-inner-padding)\n  ) !important;\n}\n\n#urlbar[breakout][breakout-extend] {\n  min-width: calc(310px + 2 * (24px + 2 * var(--toolbarbutton-inner-padding))) !important;\n}\n\n#PersonalToolbar {\n  position: relative;\n  z-index: -1;\n}\n\ntoolbarspring.chromeclass-toolbar-additional {\n  display: none !important;\n}\n\n/* `tabbar_first` OFF: nav-bar first. */\n#TabsToolbar {\n  margin-inline-start: var(--uc-navbar-margin) !important;\n}\n\n#nav-bar {\n  margin-inline-end: var(--uc-tabbar-width) !important;\n}\n\n.titlebar-spacer[type="pre-tabs"] {\n  display: none !important;\n}\n\n/*= Window controls - move to toolbar =========================================\n * leptonChrome.css:6566-6571. scoped to Windows by the enclosing\n * `@media (-moz-platform: windows)` in the source. */\n@media (-moz-platform: windows) {\n  #nav-bar {\n    margin-inline-start: var(--uc-window-drag-space-pre) !important;\n  }\n}\n';

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/url-view-always-show-page-actions.css?inline
  var url_view_always_show_page_actions_default = "/* Floorp chrome-extras: urlViewAlwaysShowPageActions\n * Ported from vendored Lepton (leptonChrome.css 25089-25094, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp's dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n *\n * Lepton gates this on `userChrome.urlbar.always_show_page_actions` while\n *\n * Floorp's settings page wrote `userChrome.urlView.always_show_page_actions`,\n *\n * so the toggle never did anything. Porting from the Lepton name fixes it.\n */\n\n/* leptonChrome.css:25089-25094 */\n#urlbar:not([breakout-extend=\"true\"]) #pageActionButton {\n  display: block !important;\n  visibility: visible !important;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/url-view-go-button-when-typing.css?inline
  var url_view_go_button_when_typing_default = '/* Floorp chrome-extras: urlViewGoButtonWhenTyping\n * Ported from vendored Lepton (leptonChrome.css 25108-25112, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp\'s dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n *\n * Gecko 152 changed the URL bar nodes from IDs to classes. The selectors\n *\n * are rewritten to target the current .urlbar-input-container and\n *\n * .urlbar-go-button nodes.\n */\n\n/* leptonChrome.css:25108-25112 */\n.urlbar-input-container[pageproxystate="invalid"] .urlbar-go-button {\n  display: block !important;\n}\n';

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras-css/url-view-move-icon-to-left.css?inline
  var url_view_move_icon_to_left_default = "/* Floorp chrome-extras: urlViewMoveIconToLeft\n * Ported from vendored Lepton (leptonChrome.css 25096-25106, 1 block(s)).\n *\n * The `@media (-moz-bool-pref: ...)` gate has been removed \u2014 whether this\n * sheet is injected is decided in JS (ui-custom/styles/style-manager.ts).\n * That also drops Floorp's dependency on the vendored file, which\n * update_lepton.yml re-syncs from upstream every day.\n */\n\n/* leptonChrome.css:25096-25106 */\n.urlbarView-type-icon {\n  min-width: 16px !important;\n  height: 16px !important;\n  margin-block: 0 !important;\n  margin-inline-start: 0 !important;\n}\n.urlbarView-favicon {\n  margin-inline-start: 20px !important;\n}\n";

  // qa-kan24-pr2950/browser-features/chrome/common/designs/chrome-extras.ts
  var CHROME_EXTRAS_DEFAULTS = {
    autohideTab: false,
    autohideBackButton: false,
    autohideForwardButton: false,
    autohidePageAction: false,
    autohideSidebar: false,
    autohideNavbar: false,
    hiddenTabIcon: false,
    hiddenTabCloseButton: false,
    hiddenTabAudioIndicator: false,
    hiddenTabSharingIndicator: false,
    hiddenTabNoteIcon: false,
    hiddenTabAttentionIndicator: false,
    hiddenTabbar: false,
    hiddenNavbar: false,
    hiddenSidebarHeader: false,
    hiddenUrlbarIconbox: false,
    hiddenBookmarkbarIcon: false,
    hiddenBookmarkbarLabel: false,
    hiddenDisabledMenu: false,
    iconDisabled: false,
    iconMenu: false,
    centeredTab: false,
    centeredUrlbar: false,
    centeredBookmarkbar: false,
    urlViewMoveIconToLeft: false,
    urlViewGoButtonWhenTyping: false,
    urlViewAlwaysShowPageActions: false,
    sidebarOverlap: false,
    tabbarAsTitlebar: false,
    tabbarOneLiner: false
  };
  function leptonIconBasePath() {
    if (false) {
      return "http://localhost:5174/lepton/icons";
    }
    return "chrome://noraneko-skin/content/lepton/icons";
  }
  function resolveIcons(css) {
    return css.replaceAll("../icons", leptonIconBasePath());
  }
  var CHROME_EXTRAS_CSS = {
    autohideTab: autohide_tab_default,
    autohideBackButton: resolveIcons(autohide_back_button_default),
    autohideForwardButton: resolveIcons(autohide_forward_button_default),
    autohidePageAction: autohide_page_action_default,
    autohideSidebar: autohide_sidebar_default,
    autohideNavbar: autohide_navbar_default,
    hiddenTabIcon: hidden_tab_icon_default,
    hiddenTabCloseButton: hidden_tab_close_button_default,
    hiddenTabAudioIndicator: hidden_tab_audio_indicator_default,
    hiddenTabSharingIndicator: hidden_tab_sharing_indicator_default,
    hiddenTabNoteIcon: hidden_tab_note_icon_default,
    hiddenTabAttentionIndicator: hidden_tab_attention_indicator_default,
    hiddenTabbar: hidden_tabbar_default,
    hiddenNavbar: hidden_navbar_default,
    hiddenSidebarHeader: hidden_sidebar_header_default,
    hiddenUrlbarIconbox: hidden_urlbar_iconbox_default,
    hiddenBookmarkbarIcon: hidden_bookmarkbar_icon_default,
    hiddenBookmarkbarLabel: hidden_bookmarkbar_label_default,
    hiddenDisabledMenu: hidden_disabled_menu_default,
    // NOTE the polarity: `iconDisabled` gates whether the *icon* rules run, so the
    // sheet is emitted while icons are ENABLED (i.e. the toggle is off) and
    // nothing at all is emitted while it is on. `iconMenu` only has meaning in
    // that state too.
    iconDisabled: resolveIcons(icon_disabled_default),
    iconMenu: resolveIcons(icon_menu_default),
    centeredTab: centered_tab_default,
    centeredUrlbar: centered_urlbar_default,
    centeredBookmarkbar: centered_bookmarkbar_default,
    urlViewMoveIconToLeft: url_view_move_icon_to_left_default,
    urlViewGoButtonWhenTyping: url_view_go_button_when_typing_default,
    urlViewAlwaysShowPageActions: url_view_always_show_page_actions_default,
    sidebarOverlap: sidebar_overlap_default,
    tabbarAsTitlebar: tabbar_as_titlebar_default,
    tabbarOneLiner: tabbar_one_liner_default
  };

  // qa-kan24-pr2950/browser-features/pages-settings/src/app/design/dataManager.ts
  var SPLIT_VIEW_DND_CREATE_PREF = "floorp.splitView.dragToSplitCreate.enabled";
  var DEFAULT_SPLIT_VIEW_DND_CREATE = false;
  var DESIGN_CONFIGS_PREF = "floorp.design.configs";
  var CHROME_EXTRAS_KEYS = Object.keys(
    CHROME_EXTRAS_DEFAULTS
  );
  var designConfigWriteQueue = Promise.resolve();
  function queueDesignConfigWrite(write) {
    const pending = designConfigWriteQueue.then(write, write);
    designConfigWriteQueue = pending.catch(() => void 0);
    return pending;
  }
  function saveDesignSettings(settings, options = {}) {
    return queueDesignConfigWrite(
      () => saveDesignSettingsUnqueued(settings, options)
    );
  }
  async function saveDesignSettingsUnqueued(settings, options = {}) {
    if (Object.keys(settings).length === 0) {
      return;
    }
    const result = await rpc.getStringPref(DESIGN_CONFIGS_PREF);
    if (!result) {
      return null;
    }
    const oldData = JSON.parse(result);
    const { hasTabStyleChanged = false } = options;
    const storedStyle = normalizeTabbarStyle(oldData.tabbar?.tabbarStyle);
    const requestedStyle = normalizeTabbarStyle(settings.style ?? storedStyle);
    const nativeVerticalTabs = await rpc.getBoolPref(VERTICAL_TABS_PREF);
    const tabbarStyle = !hasTabStyleChanged && nativeVerticalTabs !== null ? resolveTabbarStyleFromNative(nativeVerticalTabs, requestedStyle) : requestedStyle;
    const newData = {
      ...oldData,
      globalConfigs: {
        ...oldData.globalConfigs,
        userInterface: settings.design,
        faviconColor: settings.faviconColor
      },
      tabbar: {
        ...oldData.tabbar,
        tabbarPosition: settings.position,
        tabbarStyle,
        multiRowTabBar: {
          ...oldData.tabbar.multiRowTabBar,
          maxRowEnabled: settings.maxRowEnabled,
          maxRow: settings.maxRow
        }
      },
      tab: {
        ...oldData.tab,
        tabScroll: {
          ...oldData.tab.tabScroll,
          reverse: settings.tabScrollReverse,
          wrap: settings.tabScrollWrap,
          enabled: settings.tabScroll
        },
        tabOpenPosition: settings.tabOpenPosition,
        tabMinHeight: settings.tabMinHeight,
        tabMinWidth: settings.tabMinWidth,
        tabPinTitle: settings.tabPinTitle,
        tabDoubleClickToClose: settings.tabDoubleClickToClose
      },
      uiCustomization: {
        ...oldData.uiCustomization,
        navbar: {
          ...oldData.uiCustomization.navbar,
          position: settings.navbarPosition,
          searchBarTop: settings.searchBarTop
        },
        display: {
          ...oldData.uiCustomization.display,
          disableFullscreenNotification: settings.disableFullscreenNotification,
          deleteBrowserBorder: settings.deleteBrowserBorder
        },
        special: {
          ...oldData.uiCustomization.special,
          optimizeForTreeStyleTab: settings.optimizeForTreeStyleTab,
          hideForwardBackwardButton: settings.hideForwardBackwardButton,
          stgLikeWorkspaces: settings.stgLikeWorkspaces
        },
        multirowTab: {
          ...oldData.uiCustomization.multirowTab,
          newtabInsideEnabled: settings.multirowTabNewtabInside
        },
        bookmarkBar: {
          ...oldData.uiCustomization.bookmarkBar,
          focusExpand: settings.bookmarkBarFocusExpand,
          position: settings.bookmarkBarPosition
        },
        qrCode: {
          ...oldData.uiCustomization.qrCode,
          disableButton: settings.disableQRCodeButton
        },
        disableFloorpStart: settings.disableFloorpStart
      }
    };
    delete newData.tab.tabDubleClickToClose;
    await rpc.setStringPref(DESIGN_CONFIGS_PREF, JSON.stringify(newData));
    await rpc.setBoolPref(
      SPLIT_VIEW_DND_CREATE_PREF,
      settings.tabDragToSplitCreate
    );
    if (hasTabStyleChanged && settings.style !== void 0) {
      await rpc.setBoolPref(VERTICAL_TABS_PREF, requestedStyle === "vertical");
    }
  }
  async function getDesignSettings() {
    await designConfigWriteQueue;
    const result = await rpc.getStringPref(DESIGN_CONFIGS_PREF);
    if (!result) {
      return null;
    }
    const data = JSON.parse(result);
    const splitViewDndCreateEnabled = await rpc.getBoolPref(
      SPLIT_VIEW_DND_CREATE_PREF
    );
    const nativeVerticalTabs = await rpc.getBoolPref(VERTICAL_TABS_PREF);
    const tabbarStyle = normalizeTabbarStyle(
      data.tabbar?.tabbarStyle
    );
    const formData = {
      design: data.globalConfigs.userInterface,
      position: data.tabbar.tabbarPosition,
      style: nativeVerticalTabs === null ? tabbarStyle : resolveTabbarStyleFromNative(nativeVerticalTabs, tabbarStyle),
      tabOpenPosition: data.tab.tabOpenPosition,
      tabMinHeight: data.tab.tabMinHeight,
      tabMinWidth: data.tab.tabMinWidth,
      tabPinTitle: data.tab.tabPinTitle,
      tabDragToSplitCreate: splitViewDndCreateEnabled ?? DEFAULT_SPLIT_VIEW_DND_CREATE,
      tabScrollReverse: data.tab.tabScroll.reverse,
      tabScrollWrap: data.tab.tabScroll.wrap,
      tabDoubleClickToClose: typeof data.tab.tabDoubleClickToClose === "boolean" ? data.tab.tabDoubleClickToClose : data.tab.tabDubleClickToClose === true,
      tabScroll: data.tab.tabScroll.enabled,
      faviconColor: data.globalConfigs.faviconColor,
      maxRowEnabled: data.tabbar.multiRowTabBar.maxRowEnabled,
      maxRow: data.tabbar.multiRowTabBar.maxRow,
      navbarPosition: data.uiCustomization.navbar.position,
      searchBarTop: data.uiCustomization.navbar.searchBarTop,
      disableFullscreenNotification: data.uiCustomization.display.disableFullscreenNotification,
      deleteBrowserBorder: data.uiCustomization.display.deleteBrowserBorder,
      optimizeForTreeStyleTab: data.uiCustomization.special.optimizeForTreeStyleTab,
      hideForwardBackwardButton: data.uiCustomization.special.hideForwardBackwardButton,
      stgLikeWorkspaces: data.uiCustomization.special.stgLikeWorkspaces,
      multirowTabNewtabInside: data.uiCustomization.multirowTab.newtabInsideEnabled,
      bookmarkBarFocusExpand: data.uiCustomization.bookmarkBar?.focusExpand ?? false,
      bookmarkBarPosition: data.uiCustomization.bookmarkBar?.position ?? "top",
      disableQRCodeButton: data.uiCustomization.qrCode?.disableButton ?? false,
      disableFloorpStart: data.uiCustomization.disableFloorpStart
    };
    return formData;
  }
  return __toCommonJS(stdin_exports);
})();
