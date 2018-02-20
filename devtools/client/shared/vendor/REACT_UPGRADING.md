[//]: # (
  This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
)

# Upgrading React

## Introduction

We use a version of React that has a few minor tweaks. We want to use an un-minified production version anyway so you need to build React yourself.

## First, Upgrade react-prop-types.js

You should start by upgrading our prop-types library to match the latest version of React. Please follow the instructions in `devtools/client/shared/vendor/REACT_PROP_TYPES_UPGRADING.md` before continuing.

## Getting the Source

```bash
git clone https://github.com/facebook/react.git
cd react
git checkout v16.2.0 # or the version you are targetting
```

## Preparing to Build

We need to disable minification and tree shaking as they overcomplicate the upgrade process without adding any benefits.

- Open scripts/rollup/build.js
- Find a method called `function getRollupOutputOptions()`
- After `sourcemap: false` add `treeshake: false` and `freeze: false`
- Change this:
  ```js
  // Apply dead code elimination and/or minification.
  isProduction &&
  ```
  To this:
  ```js
  {
    transformBundle(source) {
      return (
        source.replace(/['"]react['"]/g,
                        "'devtools/client/shared/vendor/react'")
              .replace(/createElementNS\(['"]http:\/\/www\.w3\.org\/1999\/xhtml['"], ['"]devtools\/client\/shared\/vendor\/react['"]\)/g,
                        "createElementNS('http://www.w3.org/1999/xhtml', 'react'")
              .replace(/['"]react-dom['"]/g,
                        "'devtools/client/shared/vendor/react-dom'")
              .replace(/rendererPackageName:\s['"]devtools\/client\/shared\/vendor\/react-dom['"]/g,
                        "rendererPackageName: 'react-dom'")
              .replace(/ocument\.createElement\(/g,
                        "ocument.createElementNS('http://www.w3.org/1999/xhtml', ")
      );
    },
  },
  // Apply dead code elimination and/or minification.
  false &&
  ```
  - Find `await createBundle` and remove all bundles in that block except for `UMD_DEV`, `UMD_PROD` and `NODE_DEV`.

## Building

```bash
npm install --global yarn
yarn
yarn build
```

### Package Testing Utilities

Go through `build/packages/react-test-renderer` and in each file remove all code meant for a production build e.g.

Change this:

```js
if (process.env.NODE_ENV === 'production') {
  module.exports = require('./cjs/react-test-renderer-shallow.production.min.js');
} else {
  module.exports = require('./cjs/react-test-renderer-shallow.development.js');
}
```

To this:
```js
module.exports = require('./cjs/react-test-renderer-shallow.development.js');
```

**NOTE: Be sure to remove all `process.env` conditions from inside the files in the cjs folder.**

Also in the cjs folder replace the React require paths to point at the current React version:

```js
var React = require('../../../dist/react.development');
```

From within `build/packages/react-test-renderer`:

```bash
browserify shallow.js -o react-test-renderer-shallow.js --standalone ShallowRenderer
```

### Copy the Files Into your Firefox Repo

```bash
cd <react repo root>
cp build/dist/react.production.min.js <gecko-dev>/devtools/client/shared/vendor/react.js
cp build/dist/react-dom.production.min.js <gecko-dev>/devtools/client/shared/vendor/react-dom.js
cp build/dist/react-dom-server.browser.production.min.js <gecko-dev>/devtools/client/shared/vendor/react-dom-server.js
cp build/dist/react-dom-test-utils.production.min.js <gecko-dev>/devtools/client/shared/vendor/react-dom-test-utils.js
cp build/dist/react.development.js <gecko-dev>/devtools/client/shared/vendor/react-dev.js
cp build/dist/react-dom.development.js <gecko-dev>/devtools/client/shared/vendor/react-dom-dev.js
cp build/dist/react-dom-server.browser.development.js <gecko-dev>/devtools/client/shared/vendor/react-dom-server-dev.js
cp build/dist/react-dom-test-utils.development.js <gecko-dev>/devtools/client/shared/vendor/react-dom-test-utils-dev.js
cp build/packages/react-test-renderer/react-test-renderer-shallow.js <gecko-dev>/devtools/client/shared/vendor/react-test-renderer-shallow.js
```

From this point we will no longer need your react repository so feel free to delete it.

## Patching

### Patching react-dom

Open `devtools/client/shared/vendor/react-dom.js` and `devtools/client/shared/vendor/react-dom-dev.js`.

The following change should be made to **BOTH** files.

To have React's event system working correctly in certain XUL situations, ReactDOM must be monkey patched with a fix.

Turn this:

```js
var ReactDOM$2 = Object.freeze({
  default: ReactDOM
});
```

Into this:

```js
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
      if (name === "render") {
        return reactDomRender;
      }
      return lazyFunctionBinding(ReactDOM, name);
    },
    set: (target, name, value) => {
      ReactDOM[name] = value;
      return true;
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

ReactDOM = monkeyPatchReactDOM(ReactDOM);

var ReactDOM$2 = Object.freeze({
  default: ReactDOM
});
```
