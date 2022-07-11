/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../mochitest/common.js */
/* import-globals-from ../mochitest/layout.js */
/* import-globals-from ../mochitest/promisified-events.js */

/* exported Logger, MOCHITESTS_DIR, isCacheEnabled, isWinNoCache, invokeSetAttribute, invokeFocus,
            invokeSetStyle, getAccessibleDOMNodeID, getAccessibleTagName,
            addAccessibleTask, findAccessibleChildByID, isDefunct,
            CURRENT_CONTENT_DIR, loadScripts, loadContentScripts, snippetToURL,
            Cc, Cu, arrayFromChildren, forceGC, contentSpawnMutation,
            DEFAULT_IFRAME_ID, DEFAULT_IFRAME_DOC_BODY_ID, invokeContentTask,
            matchContentDoc, currentContentDoc, getContentDPR,
            waitForImageMap, getContentBoundsForDOMElm, untilCacheIs, untilCacheOk, testBoundsInContent, waitForContentPaint */

const CURRENT_FILE_DIR = "/browser/accessible/tests/browser/";

/**
 * Current browser test directory path used to load subscripts.
 */
const CURRENT_DIR = `chrome://mochitests/content${CURRENT_FILE_DIR}`;
/**
 * A11y mochitest directory where we find common files used in both browser and
 * plain tests.
 */
const MOCHITESTS_DIR =
  "chrome://mochitests/content/a11y/accessible/tests/mochitest/";
/**
 * A base URL for test files used in content.
 */
const CURRENT_CONTENT_DIR = `http://example.com${CURRENT_FILE_DIR}`;

const LOADED_CONTENT_SCRIPTS = new Map();

const DEFAULT_CONTENT_DOC_BODY_ID = "body";
const DEFAULT_IFRAME_ID = "default-iframe-id";
const DEFAULT_IFRAME_DOC_BODY_ID = "default-iframe-body-id";

const HTML_MIME_TYPE = "text/html";
const XHTML_MIME_TYPE = "application/xhtml+xml";

const isCacheEnabled = Services.prefs.getBoolPref(
  "accessibility.cache.enabled",
  false
);

// Some RemoteAccessible methods aren't supported on Windows when the cache is
// disabled.
const isWinNoCache = !isCacheEnabled && AppConstants.platform == "win";

function loadHTMLFromFile(path) {
  // Load the HTML to return in the response from file.
  // Since it's relative to the cwd of the test runner, we start there and
  // append to get to the actual path of the file.
  const testHTMLFile = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  const dirs = path.split("/");
  for (let i = 0; i < dirs.length; i++) {
    testHTMLFile.append(dirs[i]);
  }

  const testHTMLFileStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  testHTMLFileStream.init(testHTMLFile, -1, 0, 0);
  const testHTML = NetUtil.readInputStreamToString(
    testHTMLFileStream,
    testHTMLFileStream.available()
  );

  return testHTML;
}

let gIsIframe = false;
let gIsRemoteIframe = false;

function currentContentDoc() {
  return gIsIframe ? DEFAULT_IFRAME_DOC_BODY_ID : DEFAULT_CONTENT_DOC_BODY_ID;
}

/**
 * Accessible event match criteria based on the id of the current document
 * accessible in test.
 *
 * @param   {nsIAccessibleEvent}  event
 *        Accessible event to be tested for a match.
 *
 * @return  {Boolean}
 *          True if accessible event's accessible object ID matches current
 *          document accessible ID.
 */
function matchContentDoc(event) {
  return getAccessibleDOMNodeID(event.accessible) === currentContentDoc();
}

/**
 * Used to dump debug information.
 */
let Logger = {
  /**
   * Set up this variable to dump log messages into console.
   */
  dumpToConsole: false,

  /**
   * Set up this variable to dump log messages into error console.
   */
  dumpToAppConsole: false,

  /**
   * Return true if dump is enabled.
   */
  get enabled() {
    return this.dumpToConsole || this.dumpToAppConsole;
  },

  /**
   * Dump information into console if applicable.
   */
  log(msg) {
    if (this.enabled) {
      this.logToConsole(msg);
      this.logToAppConsole(msg);
    }
  },

  /**
   * Log message to console.
   */
  logToConsole(msg) {
    if (this.dumpToConsole) {
      dump(`\n${msg}\n`);
    }
  },

  /**
   * Log message to error console.
   */
  logToAppConsole(msg) {
    if (this.dumpToAppConsole) {
      Services.console.logStringMessage(`${msg}`);
    }
  },
};

/**
 * Asynchronously set or remove content element's attribute (in content process
 * if e10s is enabled).
 * @param  {Object}  browser  current "tabbrowser" element
 * @param  {String}  id       content element id
 * @param  {String}  attr     attribute name
 * @param  {String?} value    optional attribute value, if not present, remove
 *                            attribute
 * @return {Promise}          promise indicating that attribute is set/removed
 */
function invokeSetAttribute(browser, id, attr, value) {
  if (value) {
    Logger.log(`Setting ${attr} attribute to ${value} for node with id: ${id}`);
  } else {
    Logger.log(`Removing ${attr} attribute from node with id: ${id}`);
  }

  return invokeContentTask(
    browser,
    [id, attr, value],
    (contentId, contentAttr, contentValue) => {
      let elm = content.document.getElementById(contentId);
      if (contentValue) {
        elm.setAttribute(contentAttr, contentValue);
      } else {
        elm.removeAttribute(contentAttr);
      }
    }
  );
}

/**
 * Asynchronously set or remove content element's style (in content process if
 * e10s is enabled, or in fission process if fission is enabled and a fission
 * frame is present).
 * @param  {Object}  browser  current "tabbrowser" element
 * @param  {String}  id       content element id
 * @param  {String}  aStyle   style property name
 * @param  {String?} aValue   optional style property value, if not present,
 *                            remove style
 * @return {Promise}          promise indicating that style is set/removed
 */
function invokeSetStyle(browser, id, style, value) {
  if (value) {
    Logger.log(`Setting ${style} style to ${value} for node with id: ${id}`);
  } else {
    Logger.log(`Removing ${style} style from node with id: ${id}`);
  }

  return invokeContentTask(
    browser,
    [id, style, value],
    (contentId, contentStyle, contentValue) => {
      const elm = content.document.getElementById(contentId);
      if (contentValue) {
        elm.style[contentStyle] = contentValue;
      } else {
        delete elm.style[contentStyle];
      }
    }
  );
}

/**
 * Asynchronously set focus on a content element (in content process if e10s is
 * enabled, or in fission process if fission is enabled and a fission frame is
 * present).
 * @param  {Object}  browser  current "tabbrowser" element
 * @param  {String}  id       content element id
 * @return {Promise} promise  indicating that focus is set
 */
function invokeFocus(browser, id) {
  Logger.log(`Setting focus on a node with id: ${id}`);

  return invokeContentTask(browser, [id], contentId => {
    const elm = content.document.getElementById(contentId);
    if (elm.editor) {
      elm.selectionStart = elm.selectionEnd = elm.value.length;
    }

    elm.focus();
  });
}

/**
 * Get DPR for a specific content window.
 * @param  browser
 *         Browser for which we want its content window's DPR reported.
 *
 * @return {Promise}
 *         Promise with the value that resolves to the devicePixelRatio of the
 *         content window of a given browser.
 *
 */
function getContentDPR(browser) {
  return invokeContentTask(browser, [], () => content.window.devicePixelRatio);
}

/**
 * Asynchronously perform a task in content (in content process if e10s is
 * enabled, or in fission process if fission is enabled and a fission frame is
 * present).
 * @param  {Object}    browser  current "tabbrowser" element
 * @param  {Array}     args     arguments for the content task
 * @param  {Function}  task     content task function
 *
 * @return {Promise} promise  indicating that content task is complete
 */
function invokeContentTask(browser, args, task) {
  return SpecialPowers.spawn(
    browser,
    [DEFAULT_IFRAME_ID, task.toString(), ...args],
    (iframeId, contentTask, ...contentArgs) => {
      // eslint-disable-next-line no-eval
      const runnableTask = eval(`
      (() => {
        return (${contentTask});
      })();`);
      const frame = content.document.getElementById(iframeId);

      return frame
        ? SpecialPowers.spawn(frame, contentArgs, runnableTask)
        : runnableTask.call(this, ...contentArgs);
    }
  );
}

/**
 * Compare process ID's between the top level content process and possible
 * remote/local iframe proccess.
 * @param {Object}  browser
 *        Top level browser object for a tab.
 * @param {Boolean} isRemote
 *        Indicates if we expect the iframe content process to be remote or not.
 */
async function comparePIDs(browser, isRemote) {
  function getProcessID() {
    return Services.appinfo.processID;
  }

  const contentPID = await SpecialPowers.spawn(browser, [], getProcessID);
  const iframePID = await invokeContentTask(browser, [], getProcessID);
  is(
    isRemote,
    contentPID !== iframePID,
    isRemote
      ? "Remote IFRAME is in a different process."
      : "IFRAME is in the same process."
  );
}

/**
 * Load a list of scripts into the test
 * @param {Array} scripts  a list of scripts to load
 */
function loadScripts(...scripts) {
  for (let script of scripts) {
    let path =
      typeof script === "string"
        ? `${CURRENT_DIR}${script}`
        : `${script.dir}${script.name}`;
    Services.scriptloader.loadSubScript(path, this);
  }
}

/**
 * Load a list of scripts into target's content.
 * @param {Object} target
 *        target for loading scripts into
 * @param {Array}  scripts
 *        a list of scripts to load into content
 */
async function loadContentScripts(target, ...scripts) {
  for (let { script, symbol } of scripts) {
    let contentScript = `${CURRENT_DIR}${script}`;
    let loadedScriptSet = LOADED_CONTENT_SCRIPTS.get(contentScript);
    if (!loadedScriptSet) {
      loadedScriptSet = new WeakSet();
      LOADED_CONTENT_SCRIPTS.set(contentScript, loadedScriptSet);
    } else if (loadedScriptSet.has(target)) {
      continue;
    }

    await SpecialPowers.spawn(
      target,
      [contentScript, symbol],
      async (_contentScript, importSymbol) => {
        let module = ChromeUtils.import(_contentScript);
        content.window[importSymbol] = module[importSymbol];
      }
    );
    loadedScriptSet.add(target);
  }
}

function attrsToString(attrs) {
  return Object.entries(attrs)
    .map(([attr, value]) => `${attr}=${JSON.stringify(value)}`)
    .join(" ");
}

function wrapWithIFrame(doc, options = {}) {
  let src;
  let { iframeAttrs = {}, iframeDocBodyAttrs = {} } = options;
  iframeDocBodyAttrs = {
    id: DEFAULT_IFRAME_DOC_BODY_ID,
    ...iframeDocBodyAttrs,
  };
  if (options.remoteIframe) {
    const srcURL = new URL(`http://example.net/document-builder.sjs`);
    if (doc.endsWith("html")) {
      srcURL.searchParams.append("file", `${CURRENT_FILE_DIR}${doc}`);
    } else {
      srcURL.searchParams.append(
        "html",
        `<!doctype html>
        <html>
          <head>
            <meta charset="utf-8"/>
            <title>Accessibility Fission Test</title>
          </head>
          <body ${attrsToString(iframeDocBodyAttrs)}>${doc}</body>
        </html>`
      );
    }
    src = srcURL.href;
  } else {
    const mimeType = doc.endsWith("xhtml") ? XHTML_MIME_TYPE : HTML_MIME_TYPE;
    if (doc.endsWith("html")) {
      doc = loadHTMLFromFile(`${CURRENT_FILE_DIR}${doc}`);
      doc = doc.replace(
        /<body[.\s\S]*?>/,
        `<body ${attrsToString(iframeDocBodyAttrs)}>`
      );
    } else {
      doc = `<!doctype html>
      <body ${attrsToString(iframeDocBodyAttrs)}>${doc}</body>`;
    }

    src = `data:${mimeType};charset=utf-8,${encodeURIComponent(doc)}`;
  }

  iframeAttrs = {
    id: DEFAULT_IFRAME_ID,
    src,
    ...iframeAttrs,
  };

  return `<iframe ${attrsToString(iframeAttrs)}/>`;
}

/**
 * Takes an HTML snippet or HTML doc url and returns an encoded URI for a full
 * document with the snippet or the URL as a source for the IFRAME.
 * @param {String} doc
 *        a markup snippet or url.
 * @param {Object} options (see options in addAccessibleTask).
 *
 * @return {String}
 *        a base64 encoded data url of the document container the snippet.
 **/
function snippetToURL(doc, options = {}) {
  const { contentDocBodyAttrs = {} } = options;
  const attrs = {
    id: DEFAULT_CONTENT_DOC_BODY_ID,
    ...contentDocBodyAttrs,
  };

  if (gIsIframe) {
    doc = wrapWithIFrame(doc, options);
  }

  const encodedDoc = encodeURIComponent(
    `<!doctype html>
    <html>
      <head>
        <meta charset="utf-8"/>
        <title>Accessibility Test</title>
      </head>
      <body ${attrsToString(attrs)}>${doc}</body>
    </html>`
  );

  return `data:text/html;charset=utf-8,${encodedDoc}`;
}

function accessibleTask(doc, task, options = {}) {
  return async function() {
    gIsRemoteIframe = options.remoteIframe;
    gIsIframe = options.iframe || gIsRemoteIframe;
    let url;
    if (options.chrome && doc.endsWith("html")) {
      // Load with a chrome:// URL so this loads as a chrome document in the
      // parent process.
      url = `${CURRENT_DIR}${doc}`;
    } else if (doc.endsWith("html") && !gIsIframe) {
      url = `${CURRENT_CONTENT_DIR}${doc}`;
    } else {
      url = snippetToURL(doc, options);
    }

    registerCleanupFunction(() => {
      for (let observer of Services.obs.enumerateObservers(
        "accessible-event"
      )) {
        Services.obs.removeObserver(observer, "accessible-event");
      }
    });

    let onContentDocLoad;
    if (!options.chrome) {
      onContentDocLoad = waitForEvent(
        EVENT_DOCUMENT_LOAD_COMPLETE,
        DEFAULT_CONTENT_DOC_BODY_ID
      );
    }

    let onIframeDocLoad;
    if (options.remoteIframe && !options.skipFissionDocLoad) {
      onIframeDocLoad = waitForEvent(
        EVENT_DOCUMENT_LOAD_COMPLETE,
        DEFAULT_IFRAME_DOC_BODY_ID
      );
    }

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        // For chrome, we need a non-remote browser.
        opening: !options.chrome
          ? url
          : () => {
              // Passing forceNotRemote: true still sets maychangeremoteness,
              // which will cause data: URIs to load remotely. There's no way to
              // avoid this with gBrowser or BrowserTestUtils. Therefore, we
              // load a blank document initially and replace it below.
              gBrowser.selectedTab = BrowserTestUtils.addTab(
                gBrowser,
                "about:blank",
                {
                  forceNotRemote: true,
                }
              );
            },
      },
      async function(browser) {
        registerCleanupFunction(() => {
          if (browser) {
            let tab = gBrowser.getTabForBrowser(browser);
            if (tab && !tab.closing && tab.linkedBrowser) {
              gBrowser.removeTab(tab);
            }
          }
        });

        if (options.chrome) {
          await SpecialPowers.pushPrefEnv({
            set: [["security.allow_unsafe_parent_loads", true]],
          });
          // Ensure this never becomes a remote browser.
          browser.removeAttribute("maychangeremoteness");
          // Now we can load our page without it becoming remote.
          browser.setAttribute("src", url);
        }

        await SimpleTest.promiseFocus(browser);
        await loadContentScripts(browser, {
          script: "Common.jsm",
          symbol: "CommonUtils",
        });

        if (options.chrome) {
          ok(!browser.isRemoteBrowser, "Not remote browser");
        } else if (Services.appinfo.browserTabsRemoteAutostart) {
          ok(browser.isRemoteBrowser, "Actually remote browser");
        }

        let docAccessible;
        if (options.chrome) {
          // Chrome documents don't fire DOCUMENT_LOAD_COMPLETE. Instead, wait
          // until we can get the DocAccessible and it doesn't have the busy
          // state.
          await BrowserTestUtils.waitForCondition(() => {
            docAccessible = getAccessible(browser.contentWindow.document);
            if (!docAccessible) {
              return false;
            }
            const state = {};
            docAccessible.getState(state, {});
            return !(state.value & STATE_BUSY);
          });
        } else {
          ({ accessible: docAccessible } = await onContentDocLoad);
        }
        let iframeDocAccessible;
        if (gIsIframe) {
          if (!options.skipFissionDocLoad) {
            await comparePIDs(browser, options.remoteIframe);
            iframeDocAccessible = onIframeDocLoad
              ? (await onIframeDocLoad).accessible
              : findAccessibleChildByID(docAccessible, DEFAULT_IFRAME_ID)
                  .firstChild;
          }
        }

        await task(
          browser,
          iframeDocAccessible || docAccessible,
          iframeDocAccessible && docAccessible
        );
      }
    );
  };
}

/**
 * A wrapper around browser test add_task that triggers an accessible test task
 * as a new browser test task with given document, data URL or markup snippet.
 * @param  {String} doc
 *         URL (relative to current directory) or data URL or markup snippet
 *         that is used to test content with
 * @param  {Function|AsyncFunction} task
 *         a generator or a function with tests to run
 * @param  {null|Object} options
 *         Options for running accessibility test tasks:
 *         - {Boolean} topLevel
 *           Flag to run the test with content in the top level content process.
 *           Default is true.
 *         - {Boolean} chrome
 *           Flag to run the test with content as a chrome document in the
 *           parent process. Default is false. Although url can be a markup
 *           snippet, a snippet cannot be used for XUL content. To load XUL,
 *           specify a relative URL to a XUL document. In that case, toplevel
 *           should usually be set to false, since XUL documents don't work in
 *           content processes.
 *         - {Boolean} iframe
 *           Flag to run the test with content wrapped in an iframe. Default is
 *           false.
 *         - {Boolean} remoteIframe
 *           Flag to run the test with content wrapped in a remote iframe.
 *           Default is false.
 *         - {Object} iframeAttrs
 *           A map of attribute/value pairs to be applied to IFRAME element.
 *         - {Boolean} skipFissionDocLoad
 *           If true, the test will not wait for iframe document document
 *           loaded event (useful for when IFRAME is initially hidden).
 *         - {Object} contentDocBodyAttrs
 *           a set of attributes to be applied to a top level content document
 *           body
 *         - {Object} iframeDocBodyAttrs
 *           a set of attributes to be applied to a iframe content document body
 */
function addAccessibleTask(doc, task, options = {}) {
  const {
    topLevel = true,
    chrome = false,
    iframe = false,
    remoteIframe = false,
  } = options;
  if (topLevel) {
    add_task(
      accessibleTask(doc, task, {
        ...options,
        chrome: false,
        iframe: false,
        remoteIframe: false,
      })
    );
  }

  if (chrome) {
    add_task(
      accessibleTask(doc, task, {
        ...options,
        topLevel: false,
        iframe: false,
        remoteIframe: false,
      })
    );
  }

  if (iframe) {
    add_task(
      accessibleTask(doc, task, {
        ...options,
        topLevel: false,
        chrome: false,
        remoteIframe: false,
      })
    );
  }

  if (gFissionBrowser && remoteIframe) {
    add_task(
      accessibleTask(doc, task, {
        ...options,
        topLevel: false,
        chrome: false,
        iframe: false,
      })
    );
  }
}

/**
 * Check if an accessible object has a defunct test.
 * @param  {nsIAccessible}  accessible object to test defunct state for
 * @return {Boolean}        flag indicating defunct state
 */
function isDefunct(accessible) {
  let defunct = false;
  try {
    let extState = {};
    accessible.getState({}, extState);
    defunct = extState.value & Ci.nsIAccessibleStates.EXT_STATE_DEFUNCT;
  } catch (x) {
    defunct = true;
  } finally {
    if (defunct) {
      Logger.log(`Defunct accessible: ${prettyName(accessible)}`);
    }
  }
  return defunct;
}

/**
 * Get the DOM tag name for a given accessible.
 * @param  {nsIAccessible}  accessible accessible
 * @return {String?}                   tag name of associated DOM node, or null.
 */
function getAccessibleTagName(acc) {
  try {
    return acc.attributes.getStringProperty("tag");
  } catch (e) {
    return null;
  }
}

/**
 * Traverses the accessible tree starting from a given accessible as a root and
 * looks for an accessible that matches based on its DOMNode id.
 * @param  {nsIAccessible}  accessible root accessible
 * @param  {String}         id         id to look up accessible for
 * @param  {Array?}         interfaces the interface or an array interfaces
 *                                     to query it/them from obtained accessible
 * @return {nsIAccessible?}            found accessible if any
 */
function findAccessibleChildByID(accessible, id, interfaces) {
  if (getAccessibleDOMNodeID(accessible) === id) {
    return queryInterfaces(accessible, interfaces);
  }
  for (let i = 0; i < accessible.children.length; ++i) {
    let found = findAccessibleChildByID(accessible.getChildAt(i), id);
    if (found) {
      return queryInterfaces(found, interfaces);
    }
  }
  return null;
}

function queryInterfaces(accessible, interfaces) {
  if (!interfaces) {
    return accessible;
  }

  for (let iface of interfaces.filter(i => !(accessible instanceof i))) {
    try {
      accessible.QueryInterface(iface);
    } catch (e) {
      ok(false, "Can't query " + iface);
    }
  }

  return accessible;
}

function arrayFromChildren(accessible) {
  return Array.from({ length: accessible.childCount }, (c, i) =>
    accessible.getChildAt(i)
  );
}

/**
 * Force garbage collection.
 */
function forceGC() {
  SpecialPowers.gc();
  SpecialPowers.forceShrinkingGC();
  SpecialPowers.forceCC();
  SpecialPowers.gc();
  SpecialPowers.forceShrinkingGC();
  SpecialPowers.forceCC();
}

/*
 * This function spawns a content task and awaits expected mutation events from
 * various content changes. It's good at catching events we did *not* expect. We
 * do this advancing the layout refresh to flush the relocations/insertions
 * queue.
 */
async function contentSpawnMutation(browser, waitFor, func, args = []) {
  let onReorders = waitForEvents({ expected: waitFor.expected || [] });
  let unexpectedListener = new UnexpectedEvents(waitFor.unexpected || []);

  function tick() {
    // 100ms is an arbitrary positive number to advance the clock.
    // We don't need to advance the clock for a11y mutations, but other
    // tick listeners may depend on an advancing clock with each refresh.
    content.windowUtils.advanceTimeAndRefresh(100);
  }

  // This stops the refreh driver from doing its regular ticks, and leaves
  // us in control.
  await invokeContentTask(browser, [], tick);

  // Perform the tree mutation.
  await invokeContentTask(browser, args, func);

  // Do one tick to flush our queue (insertions, relocations, etc.)
  await invokeContentTask(browser, [], tick);

  let events = await onReorders;

  unexpectedListener.stop();

  // Go back to normal refresh driver ticks.
  await invokeContentTask(browser, [], function() {
    content.windowUtils.restoreNormalRefresh();
  });

  return events;
}

async function waitForImageMap(browser, accDoc, id = "imgmap") {
  let acc = findAccessibleChildByID(accDoc, id);

  if (!acc) {
    const onShow = waitForEvent(EVENT_SHOW, id);
    acc = (await onShow).accessible;
  }

  if (acc.firstChild) {
    return;
  }

  const onReorder = waitForEvent(EVENT_REORDER, id);
  // Wave over image map
  await invokeContentTask(browser, [id], contentId => {
    const { ContentTaskUtils } = ChromeUtils.import(
      "resource://testing-common/ContentTaskUtils.jsm"
    );
    const EventUtils = ContentTaskUtils.getEventUtils(content);
    EventUtils.synthesizeMouse(
      content.document.getElementById(contentId),
      10,
      10,
      { type: "mousemove" },
      content
    );
  });
  await onReorder;
}

async function getContentBoundsForDOMElm(browser, id) {
  return invokeContentTask(browser, [id], contentId => {
    const { Layout: LayoutUtils } = ChromeUtils.import(
      "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
    );

    return LayoutUtils.getBoundsForDOMElm(contentId, content.document);
  });
}

const CACHE_WAIT_TIMEOUT_MS = 5000;

/**
 * Wait for a predicate to be true after cache ticks.
 * This function takes two callbacks, the condition is evaluated
 * by calling the first callback with the arguments returned by the second.
 * This allows us to asynchronously return the arguments as a result if the condition
 * of the first callback is met, or if it times out. The returned arguments can then
 * be used to record a pass or fail in the test.
 */
function untilCacheCondition(conditionFunc, argsFunc) {
  return new Promise((resolve, reject) => {
    let args = argsFunc();
    if (conditionFunc(...args)) {
      resolve(args);
      return;
    }

    let cacheObserver = {
      observe(subject) {
        args = argsFunc();
        if (conditionFunc(...args)) {
          clearTimeout(this.timer);
          Services.obs.removeObserver(this, "accessible-cache");
          resolve(args);
        }
      },

      timeout() {
        Services.obs.removeObserver(this, "accessible-cache");
        args = argsFunc();
        resolve(args);
      },
    };

    cacheObserver.timer = setTimeout(
      cacheObserver.timeout.bind(cacheObserver),
      CACHE_WAIT_TIMEOUT_MS
    );
    Services.obs.addObserver(cacheObserver, "accessible-cache");
  });
}

function untilCacheOk(conditionFunc, message) {
  return untilCacheCondition(
    (v, _unusedMessage) => v,
    () => [conditionFunc(), message]
  ).then(([v, msg]) => ok(v, msg));
}

function untilCacheIs(retrievalFunc, expected, message) {
  return untilCacheCondition(
    (a, b, _unusedMessage) => Object.is(a, b),
    () => [retrievalFunc(), expected, message]
  ).then(([got, exp, msg]) => is(got, exp, msg));
}

async function waitForContentPaint(browser) {
  await SpecialPowers.spawn(browser, [], () => {
    return new Promise(function(r) {
      content.requestAnimationFrame(() => content.setTimeout(r));
    });
  });
}

async function testBoundsInContent(iframeDocAcc, id, browser) {
  const acc = findAccessibleChildByID(iframeDocAcc, id);
  const x = {};
  const y = {};
  const width = {};
  const height = {};
  acc.getBounds(x, y, width, height);

  await invokeContentTask(
    browser,
    [id, x.value, y.value, width.value, height.value],
    (_id, _x, _y, _width, _height) => {
      const { Layout: LayoutUtils } = ChromeUtils.import(
        "chrome://mochitests/content/browser/accessible/tests/browser/Layout.jsm"
      );
      const FUZZ = 1;

      let [
        expectedX,
        expectedY,
        expectedWidth,
        expectedHeight,
      ] = LayoutUtils.getBoundsForDOMElm(_id, content.document);

      ok(
        _x >= expectedX - FUZZ && _x <= expectedX + FUZZ,
        "Got " + _x + " expected " + expectedX + ", accurate x for " + _id
      );
      ok(
        _y >= expectedY - FUZZ && _y <= expectedY + FUZZ,
        "Got " + _y + " expected " + expectedY + ", accurate y for " + _id
      );
      ok(
        _width >= expectedWidth - FUZZ && _width <= expectedWidth + FUZZ,
        "Got " +
          _width +
          " expected " +
          expectedWidth +
          ", accurate width for " +
          _id
      );
      ok(
        _height >= expectedHeight - FUZZ && _height <= expectedHeight + FUZZ,
        "Got " +
          _height +
          " expected " +
          expectedHeight +
          ", accurate height for " +
          _id
      );
    }
  );
}
