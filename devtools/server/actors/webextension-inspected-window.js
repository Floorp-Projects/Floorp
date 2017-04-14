/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");

const {Ci, Cu, Cr} = require("chrome");

const Services = require("Services");

const {
  XPCOMUtils,
} = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

const {
  webExtensionInspectedWindowSpec,
} = require("devtools/shared/specs/webextension-inspected-window");

function CustomizedReload(params) {
  this.docShell = params.tabActor.window
                        .QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDocShell);
  this.docShell.QueryInterface(Ci.nsIWebProgress);

  this.inspectedWindowEval = params.inspectedWindowEval;
  this.callerInfo = params.callerInfo;

  this.ignoreCache = params.ignoreCache;
  this.injectedScript = params.injectedScript;
  this.userAgent = params.userAgent;

  this.customizedReloadWindows = new WeakSet();
}

CustomizedReload.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsISupports]),
  get window() {
    return this.docShell.DOMWindow;
  },

  get webNavigation() {
    return this.docShell
               .QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation);
  },

  start() {
    if (!this.waitForReloadCompleted) {
      this.waitForReloadCompleted = new Promise((resolve, reject) => {
        this.resolveReloadCompleted = resolve;
        this.rejectReloadCompleted = reject;

        if (this.userAgent) {
          this.docShell.customUserAgent = this.userAgent;
        }

        let reloadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

        if (this.ignoreCache) {
          reloadFlags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
        }

        try {
          if (this.injectedScript) {
            // Listen to the newly created document elements only if there is an
            // injectedScript to evaluate.
            Services.obs.addObserver(this, "document-element-inserted");
          }

          // Watch the loading progress and clear the current CustomizedReload once the
          // page has been reloaded (or if its reloading has been interrupted).
          this.docShell.addProgressListener(this,
                                            Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);

          this.webNavigation.reload(reloadFlags);
        } catch (err) {
          // Cancel the injected script listener if the reload fails
          // (which will also report the error by rejecting the promise).
          this.stop(err);
        }
      });
    }

    return this.waitForReloadCompleted;
  },

  observe(subject, topic, data) {
    if (topic !== "document-element-inserted") {
      return;
    }

    const document = subject;
    const window = document && document.defaultView;

    // Filter out non interesting documents.
    if (!document || !document.location || !window) {
      return;
    }

    let subjectDocShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIWebNavigation)
                                .QueryInterface(Ci.nsIDocShell);

    // Keep track of the set of window objects where we are going to inject
    // the injectedScript: the top level window and all its descendant
    // that are still of type content (filtering out loaded XUL pages, if any).
    if (window == this.window) {
      this.customizedReloadWindows.add(window);
    } else if (subjectDocShell.sameTypeParent) {
      let parentWindow = subjectDocShell.sameTypeParent
                                        .QueryInterface(Ci.nsIInterfaceRequestor)
                                        .getInterface(Ci.nsIDOMWindow);
      if (parentWindow && this.customizedReloadWindows.has(parentWindow)) {
        this.customizedReloadWindows.add(window);
      }
    }

    if (this.customizedReloadWindows.has(window)) {
      const {
        apiErrorResult
      } = this.inspectedWindowEval(this.callerInfo, this.injectedScript, {}, window);

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
  },

  onStateChange(webProgress, request, state, status) {
    if (webProgress.DOMWindow !== this.window) {
      return;
    }

    if (state & Ci.nsIWebProgressListener.STATE_STOP) {
      if (status == Cr.NS_BINDING_ABORTED) {
        // The customized reload has been interrupted and we can clear
        // the CustomizedReload and reject the promise.
        const url = this.window.location.href;
        this.stop(new Error(
          `devtools.inspectedWindow.reload on ${url} has been interrupted`
        ));
      } else {
        // Once the top level frame has been loaded, we can clear the customized reload
        // and resolve the promise.
        this.stop();
      }
    }
  },

  stop(error) {
    if (this.stopped) {
      return;
    }

    this.docShell.removeProgressListener(this);

    if (this.injectedScript) {
      Services.obs.removeObserver(this, "document-element-inserted");
    }

    // Reset the customized user agent.
    if (this.userAgent && this.docShell.customUserAgent == this.userAgent) {
      this.docShell.customUserAgent = null;
    }

    if (error) {
      this.rejectReloadCompleted(error);
    } else {
      this.resolveReloadCompleted();
    }

    this.stopped = true;
  }
};

var WebExtensionInspectedWindowActor = protocol.ActorClassWithSpec(
  webExtensionInspectedWindowSpec,
  {
    /**
     * Created the WebExtension InspectedWindow actor
     */
    initialize(conn, tabActor) {
      protocol.Actor.prototype.initialize.call(this, conn);
      this.tabActor = tabActor;
    },

    destroy(conn) {
      protocol.Actor.prototype.destroy.call(this, conn);
      if (this.customizedReload) {
        this.customizedReload.stop(
          new Error("WebExtensionInspectedWindowActor destroyed")
        );
        delete this.customizedReload;
      }

      if (this._dbg) {
        this._dbg.enabled = false;
        delete this._dbg;
      }
    },

    isSystemPrincipal(window) {
      const principal = window.document.nodePrincipal;
      return Services.scriptSecurityManager.isSystemPrincipal(principal);
    },

    get dbg() {
      if (this._dbg) {
        return this._dbg;
      }

      this._dbg = this.tabActor.makeDebugger();
      return this._dbg;
    },

    get window() {
      return this.tabActor.window;
    },

    get webNavigation() {
      return this.tabActor.webNavigation;
    },

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
    reload(callerInfo, {ignoreCache, userAgent, injectedScript}) {
      if (this.isSystemPrincipal(this.window)) {
        console.error("Ignored inspectedWindow.reload on system principal target for " +
                      `${callerInfo.url}:${callerInfo.lineNumber}`);
        return {};
      }

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
              tabActor: this.tabActor,
              inspectedWindowEval: this.eval.bind(this),
              callerInfo, injectedScript, userAgent, ignoreCache,
            });

            this.customizedReload.start()
                .then(() => {
                  delete this.customizedReload;
                })
                .catch(err => {
                  delete this.customizedReload;
                  throw err;
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
        }
      };

      // Execute the reload in a dispatched runnable, so that we can
      // return the reply to the caller before the reload is actually
      // started.
      Services.tm.currentThread.dispatch(delayedReload, 0);

      return {};
    },

    /**
     * Evaluate the provided javascript code in a target window (that is always the
     * tabActor window when called through RDP protocol, or the passed customTargetWindow
     * when called directly from the CustomizedReload instances).
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
     *   `tabActor.window`.
     */
    eval(callerInfo, expression, options, customTargetWindow) {
      const window = customTargetWindow || this.window;

      if (Object.keys(options).length > 0) {
        return {
          exceptionInfo: {
            isError: true,
            code: "E_PROTOCOLERROR",
            description: "Inspector protocol error: %s",
            details: [
              "The inspectedWindow.eval options are currently not supported",
            ],
          },
        };
      }

      if (!window) {
        return {
          exceptionInfo: {
            isError: true,
            code: "E_PROTOCOLERROR",
            description: "Inspector protocol error: %s",
            details: [
              "The target window is not defined. inspectedWindow.eval not executed.",
            ],
          },
        };
      }

      if (this.isSystemPrincipal(window)) {
        // On denied JS evaluation, report it using the same data format
        // used in the corresponding chrome API method to report issues that are
        // not exceptions raised in the evaluated javascript code.
        return {
          exceptionInfo: {
            isError: true,
            code: "E_PROTOCOLERROR",
            description: "Inspector protocol error: %s",
            details: [
              "This target has a system principal. inspectedWindow.eval denied.",
            ],
          },
        };
      }

      const dbgWindow = this.dbg.makeGlobalObjectReference(window);

      let evalCalledFrom = callerInfo.url;
      if (callerInfo.lineNumber) {
        evalCalledFrom += `:${callerInfo.lineNumber}`;
      }
      // TODO(rpl): add $0 and inspect(...) bindings (Bug 1300590)
      const result = dbgWindow.executeInGlobalWithBindings(expression, {}, {
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
          const unsafeDereference = throwErr && (typeof throwErr === "object") &&
            throwErr.unsafeDereference();
          const message = unsafeDereference && unsafeDereference.toString ?
            unsafeDereference.toString() : String(throwErr);
          const stack = unsafeDereference && unsafeDereference.stack ?
            unsafeDereference.stack : null;

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
        console.error("Unexpected empty inspectedWindow.eval result for",
                      `${callerInfo.url}:${callerInfo.lineNumber}`);
      }

      if (evalResult) {
        try {
          if (evalResult && typeof evalResult === "object") {
            evalResult = evalResult.unsafeDereference();
          }
          evalResult = JSON.parse(JSON.stringify(evalResult));
        } catch (err) {
          // The evaluation result cannot be sent over the RDP Protocol,
          // report it as with the same data format used in the corresponding
          // chrome API method.
          return {
            exceptionInfo: {
              isError: true,
              code: "E_PROTOCOLERROR",
              description: "Inspector protocol error: %s",
              details: [
                String(err),
              ],
            },
          };
        }
      }

      return {value: evalResult};
    }
  }
);

exports.WebExtensionInspectedWindowActor = WebExtensionInspectedWindowActor;
