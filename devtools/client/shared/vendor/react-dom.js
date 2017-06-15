/**
 * ReactDOM v15.3.2
 *
 * Copyright 2013-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 *
 */
// Based off https://github.com/ForbesLindesay/umd/blob/master/template.js
;(function(f) {
  // CommonJS
  if (typeof exports === "object" && typeof module !== "undefined") {
    module.exports = f(require('devtools/client/shared/vendor/react'));

  // RequireJS
  } else if (typeof define === "function" && define.amd) {
    define(['devtools/client/shared/vendor/react'], f);

  // <script>
  } else {
    var g;
    if (typeof window !== "undefined") {
      g = window;
    } else if (typeof global !== "undefined") {
      g = global;
    } else if (typeof self !== "undefined") {
      g = self;
    } else {
      // works providing we're not in "use strict";
      // needed for Java 8 Nashorn
      // see https://github.com/facebook/react/issues/3037
      g = this;
    }
    g.ReactDOM = f(g.React);
  }

})(function(React) {
  //--------------------------------------------------------------------------------------
  // START MONKEY PATCH
  /**
   * This section contains a monkey patch for React DOM, so that it functions correctly in
   * certain XUL situations. React centralizes events to specific DOM nodes by only
   * binding a single listener to the document of the page. It then captures these events,
   * and then uses a SyntheticEvent system to dispatch these throughout the page.
   *
   * In privileged XUL with a XUL iframe, and React in both documents, this system breaks.
   * By design, these XUL frames can still talk to each other, while in a normal HTML
   * situation, they would not be able to. The events from the XUL iframe propagate to the
   * parent document as well. This leads to the React event system incorrectly dispatching
   * TWO SyntheticEvents for for every ONE action.
   *
   * The fix here is trick React into thinking that the owning document for every node in
   * a XUL iframe to be the toolbox.xul. This is done by creating a Proxy object that
   * captures any usage of HTMLElement.ownerDocument, and then passing in the toolbox.xul
   * document rather than (for example) the netmonitor.xul document. React will then hook
   * up the event system correctly on the top level controlling document.
   *
   * @return {object} The proxied and monkey patched ReactDOM
   */
  function monkeyPatchReactDOM(ReactDOM) {
    // This is the actual monkey patched function.
    const reactDomRender = monkeyPatchRender(ReactDOM);

    // Proxied method calls might need to be bound, but do this lazily with caching.
    const lazyFunctionBinding = functionLazyBinder();

    // Create a proxy, but the render property is not writable on the ReactDOM object, so
    // a direct proxy will fail with an error. Instead, create a proxy on a a blank object.
    // Pass on getting and setting behaviors.
    return new Proxy({}, {
      get: (target, name) => {
        return name === "render"
          ? reactDomRender
          : lazyFunctionBinding(ReactDOM, name);
      },
      set: (target, name, value) => {
        ReactDOM[name] = value;
      }
    });
  };

  /**
   * Creates a function that replaces the ReactDOM.render method. It does this by
   * creating a proxy for the dom node being mounted. This proxy rewrites the
   * "ownerDocument" property to point to the toolbox.xul document. This function
   * is only used for XUL iframes inside of a XUL document.
   *
   * @param {object} ReactDOM
   * @return {function} The patched ReactDOM.render function.
   */
  function monkeyPatchRender(ReactDOM) {
    const elementProxyCache = new WeakMap();

    return (...args) => {
      const container = args[1];
      const toolboxDoc = getToolboxDocIfXulOnly(container);

      if (toolboxDoc) {
        // Re-use any existing cached HTMLElement proxies.
        let proxy = elementProxyCache.get(container);

        if (!proxy) {
          // Proxied method calls need to be bound, but do this lazily.
          const lazyFunctionBinding = functionLazyBinder();

          // Create a proxy to the container HTMLElement. If React tries to access the
          // ownerDocument, pass in the toolbox's document, as the event dispatching system
          // is rooted from the toolbox document.
          proxy = new Proxy(container, {
            get: function (target, name) {
              if (name === "ownerDocument") {
                return toolboxDoc;
              }
              return lazyFunctionBinding(target, name);
            }
          });

          elementProxyCache.set(container, proxy);
        }

        // Update the args passed to ReactDOM.render.
        args[1] = proxy;
      }

      return ReactDOM.render.apply(this, args);
    };
  }

  /**
   * Try to access the containing toolbox XUL document, but only if all of the iframes
   * in the heirarchy are XUL documents. Events dispatch differently in the case of all
   * privileged XUL documents. Events that fire in an iframe propagate up to the parent
   * frame. This does not happen when HTML is in the mix. Only return the toolbox if
   * it matches the proper case of a XUL iframe inside of a XUL document.
   *
   * In addition to the XUL case, if the panel uses the toolbox's ReactDOM instance,
   * this patch needs to be applied as well. This is the case for the inspector.
   *
   * @param {HTMLElement} node - The DOM node inside of an iframe.
   * @return {XULDocument|null} The toolbox.xul document, or null.
   */
  function getToolboxDocIfXulOnly(node) {
    // This execution context doesn't know about XULDocuments, so don't get the toolbox.
    if (typeof XULDocument !== "function") {
      return null;
    }

    let doc = node.ownerDocument;
    const inspectorUrl = "chrome://devtools/content/inspector/inspector.xhtml";
    const netMonitorUrl = "chrome://devtools/content/netmonitor/netmonitor.xhtml";
    const webConsoleUrl = "chrome://devtools/content/webconsole/webconsole.xhtml";

    while (doc instanceof XULDocument ||
           doc.location.href === inspectorUrl ||
           doc.location.href === netMonitorUrl ||
           doc.location.href === webConsoleUrl) {
      const {frameElement} = doc.defaultView;

      if (!frameElement) {
        // We're at the root element, and no toolbox was found.
        return null;
      }

      doc = frameElement.parentElement.ownerDocument;
      if (doc.documentURI === "about:devtools-toolbox") {
        return doc;
      }
    }

    return null;
  }

  /**
   * When accessing proxied functions, the instance becomes unbound to the method. This
   * utility either passes a value through if it's not a function, or automatically binds a
   * function and caches that bound function for repeated calls.
   */
  function functionLazyBinder() {
    const boundFunctions = {};

    return (target, name) => {
      if (typeof target[name] === "function") {
        // Lazily cache function bindings.
        if (boundFunctions[name]) {
          return boundFunctions[name];
        }
        boundFunctions[name] = target[name].bind(target);
        return boundFunctions[name];
      }
      return target[name];
    };
  }

  // END MONKEY PATCH
  //--------------------------------------------------------------------------------------

  return monkeyPatchReactDOM(React.__SECRET_DOM_DO_NOT_USE_OR_YOU_WILL_BE_FIRED);
});
