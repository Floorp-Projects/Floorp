/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  webExtensionInspectedWindowSpec,
} = require("resource://devtools/shared/specs/addon/webextension-inspected-window.js");

const {
  DevToolsServer,
} = require("resource://devtools/server/devtools-server.js");

loader.lazyGetter(
  this,
  "NodeActor",
  () =>
    require("resource://devtools/server/actors/inspector/node.js").NodeActor,
  true
);

// A weak set of the documents for which a warning message has been
// already logged (so that we don't keep emitting the same warning if an
// extension keeps calling the devtools.inspectedWindow.eval API method
// when it fails to retrieve a result, but we do log the warning message
// if the user reloads the window):
//
// WeakSet<Document>
const deniedWarningDocuments = new WeakSet();

function isSystemPrincipalWindow(window) {
  return window.document.nodePrincipal.isSystemPrincipal;
}

// Create the exceptionInfo property in the format expected by a
// WebExtension inspectedWindow.eval API calls.
function createExceptionInfoResult(props) {
  return {
    exceptionInfo: {
      isError: true,
      code: "E_PROTOCOLERROR",
      description: "Unknown Inspector protocol error",

      // Apply the passed properties.
      ...props,
    },
  };
}

// Show a warning message in the webconsole when an extension
// eval request has been denied, so that the user knows about it
// even if the extension doesn't report the error itself.
function logAccessDeniedWarning(window, callerInfo, extensionPolicy) {
  // Do not log the same warning multiple times for the same document.
  if (deniedWarningDocuments.has(window.document)) {
    return;
  }

  deniedWarningDocuments.add(window.document);

  const { name } = extensionPolicy;

  // System principals have a null nodePrincipal.URI and so we use
  // the url from window.location.href.
  const reportedURIorPrincipal = isSystemPrincipalWindow(window)
    ? Services.io.newURI(window.location.href)
    : window.document.nodePrincipal;

  const error = Cc["@mozilla.org/scripterror;1"].createInstance(
    Ci.nsIScriptError
  );

  const msg = `The extension "${name}" is not allowed to access ${reportedURIorPrincipal.spec}`;

  const innerWindowId = window.windowGlobalChild.innerWindowId;

  const errorFlag = 0;

  let { url, lineNumber } = callerInfo;

  const callerURI = callerInfo.url && Services.io.newURI(callerInfo.url);

  // callerInfo.url is not the full path to the file that called the WebExtensions
  // API yet (Bug 1448878), and so we associate the error to the url of the extension
  // manifest.json file as a fallback.
  if (callerURI.filePath === "/") {
    url = extensionPolicy.getURL("/manifest.json");
    lineNumber = null;
  }

  error.initWithWindowID(
    msg,
    url,
    lineNumber,
    0,
    0,
    errorFlag,
    "webExtensions",
    innerWindowId
  );
  Services.console.logMessage(error);
}

function extensionAllowedToInspectPrincipal(extensionPolicy, principal) {
  if (principal.isNullPrincipal) {
    // data: and sandboxed documents.
    //
    // Rather than returning true unconditionally, we go through additional
    // checks to prevent execution in sandboxed documents created by principals
    // that extensions cannot access otherwise.
    principal = principal.precursorPrincipal;
    if (!principal) {
      // Top-level about:blank, etc.
      return true;
    }
  }
  if (!principal.isContentPrincipal) {
    return false;
  }
  const principalURI = principal.URI;
  if (principalURI.schemeIs("https") || principalURI.schemeIs("http")) {
    if (WebExtensionPolicy.isRestrictedURI(principalURI)) {
      return false;
    }
    if (extensionPolicy.quarantinedFromURI(principalURI)) {
      return false;
    }
    // Common case: http(s) allowed.
    return true;
  }

  if (principalURI.schemeIs("moz-extension")) {
    // Ordinarily, we don't allow extensions to execute arbitrary code in
    // their own context. The devtools.inspectedWindow.eval API is a special
    // case - this can only be used through the devtools_page feature, which
    // requires the user to open the developer tools first. If an extension
    // really wants to debug itself, we let it do so.
    return extensionPolicy.id === principal.addonId;
  }

  if (principalURI.schemeIs("file")) {
    return true;
  }

  return false;
}

class CustomizedReload {
  constructor(params) {
    this.docShell = params.targetActor.window.docShell;
    this.docShell.QueryInterface(Ci.nsIWebProgress);

    this.inspectedWindowEval = params.inspectedWindowEval;
    this.callerInfo = params.callerInfo;

    this.ignoreCache = params.ignoreCache;
    this.injectedScript = params.injectedScript;

    this.customizedReloadWindows = new WeakSet();
  }

  QueryInterface = ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
  ]);

  get window() {
    return this.docShell.DOMWindow;
  }

  get webNavigation() {
    return this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation);
  }

  get browsingContext() {
    return this.docShell.browsingContext;
  }

  start() {
    if (!this.waitForReloadCompleted) {
      this.waitForReloadCompleted = new Promise((resolve, reject) => {
        this.resolveReloadCompleted = resolve;
        this.rejectReloadCompleted = reject;

        let reloadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

        if (this.ignoreCache) {
          reloadFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
        }

        try {
          if (this.injectedScript) {
            // Listen to the newly created document elements only if there is an
            // injectedScript to evaluate.
            Services.obs.addObserver(this, "initial-document-element-inserted");
          }

          // Watch the loading progress and clear the current CustomizedReload once the
          // page has been reloaded (or if its reloading has been interrupted).
          this.docShell.addProgressListener(
            this,
            Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
          );

          this.webNavigation.reload(reloadFlags);
        } catch (err) {
          // Cancel the injected script listener if the reload fails
          // (which will also report the error by rejecting the promise).
          this.stop(err);
        }
      });
    }

    return this.waitForReloadCompleted;
  }

  observe(subject, topic, data) {
    if (topic !== "initial-document-element-inserted") {
      return;
    }

    const document = subject;
    const window = document?.defaultView;

    // Filter out non interesting documents.
    if (!document || !document.location || !window) {
      return;
    }

    const subjectDocShell = window.docShell;

    // Keep track of the set of window objects where we are going to inject
    // the injectedScript: the top level window and all its descendant
    // that are still of type content (filtering out loaded XUL pages, if any).
    if (window == this.window) {
      this.customizedReloadWindows.add(window);
    } else if (subjectDocShell.sameTypeParent) {
      const parentWindow = subjectDocShell.sameTypeParent.domWindow;
      if (parentWindow && this.customizedReloadWindows.has(parentWindow)) {
        this.customizedReloadWindows.add(window);
      }
    }

    if (this.customizedReloadWindows.has(window)) {
      const { apiErrorResult } = this.inspectedWindowEval(
        this.callerInfo,
        this.injectedScript,
        {},
        window
      );

      // Log only apiErrorResult, because no one is waiting for the
      // injectedScript result, and any exception is going to be logged
      // in the inspectedWindow webconsole.
      if (apiErrorResult) {
        console.error(
          "Unexpected Error in injectedScript during inspectedWindow.reload for",
          `${this.callerInfo.url}:${this.callerInfo.lineNumber}`,
          apiErrorResult
        );
      }
    }
  }

  onStateChange(webProgress, request, state, status) {
    if (webProgress.DOMWindow !== this.window) {
      return;
    }

    if (state & Ci.nsIWebProgressListener.STATE_STOP) {
      if (status == Cr.NS_BINDING_ABORTED) {
        // The customized reload has been interrupted and we can clear
        // the CustomizedReload and reject the promise.
        const url = this.window.location.href;
        this.stop(
          new Error(
            `devtools.inspectedWindow.reload on ${url} has been interrupted`
          )
        );
      } else {
        // Once the top level frame has been loaded, we can clear the customized reload
        // and resolve the promise.
        this.stop();
      }
    }
  }

  stop(error) {
    if (this.stopped) {
      return;
    }

    this.docShell.removeProgressListener(this);

    if (this.injectedScript) {
      Services.obs.removeObserver(this, "initial-document-element-inserted");
    }

    if (error) {
      this.rejectReloadCompleted(error);
    } else {
      this.resolveReloadCompleted();
    }

    this.stopped = true;
  }
}

class WebExtensionInspectedWindowActor extends Actor {
  /**
   * Created the WebExtension InspectedWindow actor
   */
  constructor(conn, targetActor) {
    super(conn, webExtensionInspectedWindowSpec);
    this.targetActor = targetActor;
  }

  destroy(conn) {
    super.destroy();

    if (this.customizedReload) {
      this.customizedReload.stop(
        new Error("WebExtensionInspectedWindowActor destroyed")
      );
      delete this.customizedReload;
    }

    if (this._dbg) {
      this._dbg.disable();
      delete this._dbg;
    }
  }

  get dbg() {
    if (this._dbg) {
      return this._dbg;
    }

    this._dbg = this.targetActor.makeDebugger();
    return this._dbg;
  }

  get window() {
    return this.targetActor.window;
  }

  get webNavigation() {
    return this.targetActor.webNavigation;
  }

  createEvalBindings(dbgWindow, options) {
    const bindings = Object.create(null);

    let selectedDOMNode;

    if (options.toolboxSelectedNodeActorID) {
      const actor = DevToolsServer.searchAllConnectionsForActor(
        options.toolboxSelectedNodeActorID
      );
      if (actor && actor instanceof NodeActor) {
        selectedDOMNode = actor.rawNode;
      }
    }

    Object.defineProperty(bindings, "$0", {
      enumerable: true,
      configurable: true,
      get: () => {
        if (selectedDOMNode && !Cu.isDeadWrapper(selectedDOMNode)) {
          return dbgWindow.makeDebuggeeValue(selectedDOMNode);
        }

        return undefined;
      },
    });

    // This function is used by 'eval' and 'reload' requests, but only 'eval'
    // passes 'toolboxConsoleActor' from the client side in order to set
    // the 'inspect' binding.
    Object.defineProperty(bindings, "inspect", {
      enumerable: true,
      configurable: true,
      value: dbgWindow.makeDebuggeeValue(object => {
        const consoleActor = DevToolsServer.searchAllConnectionsForActor(
          options.toolboxConsoleActorID
        );
        if (consoleActor) {
          const dbgObj = consoleActor.makeDebuggeeValue(object);
          consoleActor.inspectObject(
            dbgObj,
            "webextension-devtools-inspectedWindow-eval"
          );
        } else {
          // TODO(rpl): evaluate if it would be better to raise an exception
          // to the caller code instead.
          console.error("Toolbox Console RDP Actor not found");
        }
      }),
    });

    return bindings;
  }

  /**
   * Reload the target tab, optionally bypass cache, customize the userAgent and/or
   * inject a script in targeted document or any of its sub-frame.
   *
   * @param {webExtensionCallerInfo} callerInfo
   *   the addonId and the url (the addon base url or the url of the actual caller
   *   filename and lineNumber) used to log useful debugging information in the
   *   produced error logs and eval stack trace.
   *
   * @param {webExtensionReloadOptions} options
   *   used to optionally enable the reload customizations.
   * @param {boolean|undefined}       options.ignoreCache
   *   enable/disable the cache bypass headers.
   * @param {string|undefined}        options.userAgent
   *   customize the userAgent during the page reload.
   * @param {string|undefined}        options.injectedScript
   *   evaluate the provided javascript code in the top level and every sub-frame
   *   created during the page reload, before any other script in the page has been
   *   executed.
   */
  async reload(callerInfo, { ignoreCache, userAgent, injectedScript }) {
    if (isSystemPrincipalWindow(this.window)) {
      console.error(
        "Ignored inspectedWindow.reload on system principal target for " +
          `${callerInfo.url}:${callerInfo.lineNumber}`
      );
      return {};
    }

    await new Promise(resolve => {
      const delayedReload = () => {
        // This won't work while the browser is shutting down and we don't really
        // care.
        if (Services.startup.shuttingDown) {
          return;
        }

        if (injectedScript || userAgent) {
          if (this.customizedReload) {
            // TODO(rpl): check what chrome does, and evaluate if queue the new reload
            // after the current one has been completed.
            console.error(
              "Reload already in progress. Ignored inspectedWindow.reload for " +
                `${callerInfo.url}:${callerInfo.lineNumber}`
            );
            return;
          }

          try {
            this.customizedReload = new CustomizedReload({
              targetActor: this.targetActor,
              inspectedWindowEval: this.eval.bind(this),
              callerInfo,
              injectedScript,
              ignoreCache,
            });

            this.customizedReload
              .start()
              .catch(err => {
                console.error(err);
              })
              .then(() => {
                delete this.customizedReload;
                resolve();
              });
          } catch (err) {
            // Cancel the customized reload (if any) on exception during the
            // reload setup.
            if (this.customizedReload) {
              this.customizedReload.stop(err);
            }
            throw err;
          }
        } else {
          // If there is no custom user agent and/or injected script, then
          // we can reload the target without subscribing any observer/listener.
          let reloadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
          if (ignoreCache) {
            reloadFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
          }
          this.webNavigation.reload(reloadFlags);
          resolve();
        }
      };

      // Execute the reload in a dispatched runnable, so that we can
      // return the reply to the caller before the reload is actually
      // started.
      Services.tm.dispatchToMainThread(delayedReload);
    });

    return {};
  }

  /**
   * Evaluate the provided javascript code in a target window (that is always the
   * targetActor window when called through RDP protocol, or the passed
   * customTargetWindow when called directly from the CustomizedReload instances).
   *
   * @param {webExtensionCallerInfo} callerInfo
   *   the addonId and the url (the addon base url or the url of the actual caller
   *   filename and lineNumber) used to log useful debugging information in the
   *   produced error logs and eval stack trace.
   *
   * @param {string} expression
   *   the javascript code to be evaluated in the target window
   *
   * @param {webExtensionEvalOptions} evalOptions
   *   used to optionally enable the eval customizations.
   *   NOTE: none of the eval options is currently implemented, they will be already
   *   reported as unsupported by the WebExtensions schema validation wrappers, but
   *   an additional level of error reporting is going to be applied here, so that
   *   if the server and the client have different ideas of which option is supported
   *   the eval call result will contain detailed informations (in the format usually
   *   expected for errors not raised in the evaluated javascript code).
   *
   * @param {DOMWindow|undefined} customTargetWindow
   *   Used in the CustomizedReload instances to evaluate the `injectedScript`
   *   javascript code in every sub-frame of the target window during the tab reload.
   *   NOTE: this parameter is not part of the RDP protocol exposed by this actor, when
   *   it is called over the remote debugging protocol the target window is always
   *   `targetActor.window`.
   */
  // eslint-disable-next-line complexity
  eval(callerInfo, expression, options, customTargetWindow) {
    const window = customTargetWindow || this.window;
    options = options || {};

    const extensionPolicy = WebExtensionPolicy.getByID(callerInfo.addonId);

    if (!extensionPolicy) {
      return createExceptionInfoResult({
        description: "Inspector protocol error: %s %s",
        details: ["Caller extension not found for", callerInfo.url],
      });
    }

    if (!window) {
      return createExceptionInfoResult({
        description: "Inspector protocol error: %s",
        details: [
          "The target window is not defined. inspectedWindow.eval not executed.",
        ],
      });
    }

    if (
      !extensionAllowedToInspectPrincipal(
        extensionPolicy,
        window.document.nodePrincipal
      )
    ) {
      // Log the error for the user to know that the extension request has been
      // denied (the extension may not warn the user at all).
      logAccessDeniedWarning(window, callerInfo, extensionPolicy);

      // The error message is generic here. If access is disallowed, we do not
      // expose the URL either.
      return createExceptionInfoResult({
        description: "Inspector protocol error: %s",
        details: [
          "This extension is not allowed on the current inspected window origin",
        ],
      });
    }

    // Raise an error on the unsupported options.
    if (
      options.frameURL ||
      options.contextSecurityOrigin ||
      options.useContentScriptContext
    ) {
      return createExceptionInfoResult({
        description: "Inspector protocol error: %s",
        details: [
          "The inspectedWindow.eval options are currently not supported",
        ],
      });
    }

    const dbgWindow = this.dbg.makeGlobalObjectReference(window);

    let evalCalledFrom = callerInfo.url;
    if (callerInfo.lineNumber) {
      evalCalledFrom += `:${callerInfo.lineNumber}`;
    }

    const bindings = this.createEvalBindings(dbgWindow, options);

    const result = dbgWindow.executeInGlobalWithBindings(expression, bindings, {
      url: `debugger eval called from ${evalCalledFrom} - eval code`,
    });

    let evalResult;

    if (result) {
      if ("return" in result) {
        evalResult = result.return;
      } else if ("yield" in result) {
        evalResult = result.yield;
      } else if ("throw" in result) {
        const throwErr = result.throw;

        // XXXworkers: Calling unsafeDereference() returns an object with no
        // toString method in workers. See Bug 1215120.
        const unsafeDereference =
          throwErr &&
          typeof throwErr === "object" &&
          throwErr.unsafeDereference();
        const message = unsafeDereference?.toString
          ? unsafeDereference.toString()
          : String(throwErr);
        const stack = unsafeDereference?.stack ? unsafeDereference.stack : null;

        return {
          exceptionInfo: {
            isException: true,
            value: `${message}\n\t${stack}`,
          },
        };
      }
    } else {
      // TODO(rpl): can the result of executeInGlobalWithBinding be null or
      // undefined? (which means that it is not a return, a yield or a throw).
      console.error(
        "Unexpected empty inspectedWindow.eval result for",
        `${callerInfo.url}:${callerInfo.lineNumber}`
      );
    }

    if (evalResult) {
      try {
        // Return the evalResult as a grip (used by the WebExtensions
        // devtools inspector's sidebar.setExpression API method).
        if (options.evalResultAsGrip) {
          if (!options.toolboxConsoleActorID) {
            return createExceptionInfoResult({
              description: "Inspector protocol error: %s - %s",
              details: [
                "Unexpected invalid sidebar panel expression request",
                "missing toolboxConsoleActorID",
              ],
            });
          }

          const consoleActor = DevToolsServer.searchAllConnectionsForActor(
            options.toolboxConsoleActorID
          );

          return { valueGrip: consoleActor.createValueGrip(evalResult) };
        }

        if (evalResult && typeof evalResult === "object") {
          evalResult = evalResult.unsafeDereference();
        }
        evalResult = JSON.parse(JSON.stringify(evalResult));
      } catch (err) {
        // The evaluation result cannot be sent over the RDP Protocol,
        // report it as with the same data format used in the corresponding
        // chrome API method.
        return createExceptionInfoResult({
          description: "Inspector protocol error: %s",
          details: [String(err)],
        });
      }
    }

    return { value: evalResult };
  }
}

exports.WebExtensionInspectedWindowActor = WebExtensionInspectedWindowActor;
