[//]: # (
  This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
)

# Upgrading React

## Introduction

We use a version of React that has a few minor tweaks. We want to use an un-minified production version anyway, and because of all of this you need to build React yourself to upgrade it for devtools.

## Getting the Source

```bash
git clone https://github.com/facebook/react.git
cd react
git checkout v15.6.1 # or the version you are targetting
```

## Building

```bash
npm install
grunt build
```

If you did not receive any errors go to the section entitled "[Patching (XUL Workarounds)](#patching-xul-workarounds)."

If you receive the following error:

> Current npm version is not supported for development,
> expected "x.x.x." to satisfy "2.x || 3.x || 4.x"

Your npm version is too recent. `"2.x || 3.x || 4.x"` is a hint that the React project only supports npm versions 2.x, 3.x and 4.x. To fix this let's start by removing all of your node versions:

```bash
# If you use ubuntu
sudo apt-get remove --purge nodejs
# If you use homebrew
brew uninstall node
# If yu use macports
sudo port uninstall nodejs
# If you use nvm
LINK="https://github.com/creationix/nvm/issues/298"
xdg-open $LINK || open $LINK
```

You will need to setup a node version manager. These instructions cover "n" but many people prefer to use "nvm". If you choose to use nvm them you will need to set it up yourself.

Run the n-install script and it will set "n" it up for you:

```bash
curl -L -o /tmp/n-install-script https://git.io/n-install
bash /tmp/n-install-script -y
exec $SHELL # To re-initialize the PATH variable
```

To match node versions with npm versions see:
<https://nodejs.org/en/download/releases/>

The latest 4.x version of npm is installed with node 7.10.1 so install that version using `sudo n 7.10.1`.

Running `node --version` should now show v7.10.1 and `npm --version` should show 4.2.0.

Now try again:

```bash
npm install
grunt build
```

## Patching

### Patching build/react-with-addons.js

- Open `build/react-with-addons.js`. Search for all `document.createElement` calls and replace them with `document.createElementNS('http://www.w3.org/1999/xhtml', ...)`.

  **Note**: some code may be `ownerDocument.createElement` so don't do a blind search/replace. At the time of writing there was only 1 place to change.

- If you are editing the production version then change this:

  ```js
  if ("production" !== 'production') {
    exports.getReactPerf = function () {
      return getReactDOM().__SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED.ReactPerf;
    };

    exports.getReactTestUtils = function () {
      return getReactDOM().__SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED.ReactTestUtils;
    };
  }
  ```

  To this:

  ```js
  if ("production" !== 'production') {
    exports.getReactPerf = function () {
      return getReactDOM().__SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED.ReactPerf;
    };
  }

  exports.getReactTestUtils = function () {
    return getReactDOM().__SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED.ReactTestUtils;
  };
  ```

- If you are editing the production version then change this:

  ```js
  if ("production" !== 'production') {
    // For the UMD build we get these lazily from the global since they're tied
    // to the DOM renderer and it hasn't loaded yet.
    Object.defineProperty(React.addons, 'Perf', {
      enumerable: true,
      get: function () {
        return ReactAddonsDOMDependencies.getReactPerf();
      }
    });
    Object.defineProperty(React.addons, 'TestUtils', {
      enumerable: true,
      get: function () {
        return ReactAddonsDOMDependencies.getReactTestUtils();
      }
    });
  }
  ```

  To this:
  ```js
  if ("production" !== 'production') {
    // For the UMD build we get these lazily from the global since they're tied
    // to the DOM renderer and it hasn't loaded yet.
    Object.defineProperty(React.addons, 'Perf', {
      enumerable: true,
      get: function () {
        return ReactAddonsDOMDependencies.getReactPerf();
      }
    });
  }

  Object.defineProperty(React.addons, 'TestUtils', {
    enumerable: true,
    get: function () {
      return ReactAddonsDOMDependencies.getReactTestUtils();
    }
  });
  ```

### Patching build/react-dom.js

- Open `build/react-dom.js` and replace all of the document.createElement calls as you did for `build/react-with-addons.js`.

- Change `require('react')` near the top of the file to `require('devtools/client/shared/vendor/react')`.

- About four lines below that require there is a `define(['react'], f);`. Change this to the full path e.g.`define(['devtools/client/shared/vendor/react'], f);`

- If you are editing the production version then change this:

  ```js
  if ("production" !== 'production') {
    ReactDOMUMDEntry.__SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED = {
      // ReactPerf and ReactTestUtils currently only work with the DOM renderer
      // so we expose them from here, but only in DEV mode.
      ReactPerf: _dereq_(71),
      ReactTestUtils: _dereq_(80)
    };
  }
  ```

  Into this:

  ```js
  if ("production" !== 'production') {
    ReactDOMUMDEntry.__SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED = {
      // ReactPerf and ReactTestUtils currently only work with the DOM renderer
      // so we expose them from here, but only in DEV mode.
      ReactPerf: _dereq_(71)
    };
  }
  ReactDOMUMDEntry.__SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED = {
    ReactTestUtils: _dereq_(80)
  }
  ```

To have React's event system working correctly in certain XUL situations, ReactDOM must be monkey patched with a fix. This fix is currently applied in devtools/client/shared/vendor/react-dom.js. When upgrading, copy and paste the existing block of code into the new file in the same location. It is delimited by a header and footer, and then the monkeyPatchReactDOM() needs to be applied to the returned value.

e.g. Turn this:

```js
module.exports = ReactDOM;
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
      switch (name) {
        case "render":
          return reactDomRender;
        case "TestUtils":
          // Bind ReactTestUtils and return it when a request is made for
          // ReactDOM.TestUtils.
          let ReactInternals = ReactDOM.__SECRET_INTERNALS_DO_NOT_USE_OR_YOU_WILL_BE_FIRED;
          return lazyFunctionBinding(ReactInternals, "ReactTestUtils");
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

module.exports = monkeyPatchReactDOM(ReactDOM);
```

### Patching build/react-dom-server.js

- Open `build/react-dom-server.js` and replace all of the document.createElement calls as you did for `build/react-with-addons.js`.

- Change `require('react')` near the top of the file to `require('devtools/client/shared/vendor/react')`.

- About four lines below that require there is a `define(['react'], f);`. Change this to the full path e.g.`define(['devtools/client/shared/vendor/react'], f);`

### Copy the Files Across

- Now we need to copy `react-with-addons.js`, `react-dom.js` and `react-dom-server.js` into our repo (note the destination filenames all have -dev added e.g. `react-dev.js`, these are part of the dev version):

  ```bash
  cp build/react-with-addons.js
     <gecko-dev>/devtools/client/shared/vendor/react-dev.js
  cp build/react-dom.js
     <gecko-dev>/devtools/client/shared/vendor/react-dom-dev.js
  cp build/react-dom-server.js
     <gecko-dev>/devtools/client/shared/vendor/react-dom-server-dev.js
  ```

### Generate a Production Build

```bash
NODE_ENV=production grunt build

# Or if using the fish shell:

env NODE_ENV=production grunt build
```

### More Patching

Unfortunately, you will need to repeat the following sections **(See note below)**:

- [Patching build/react-with-addons.js](#patching-buildreact-with-addonsjs)
- [Patching build/react-dom.js](#patching-buildreact-domjs)
- [Patching build/react-dom-server.js](#patching-buildreact-dom-serverjs)

**NOTE**: This time you need to save the files with their original filenames so the commands in the "Copy the Files Across" section become:

  ```bash
  cp build/react-with-addons.js
     <gecko-dev>/devtools/client/shared/vendor/react.js
  cp build/react-dom.js
     <gecko-dev>/devtools/client/shared/vendor/react-dom.js
  cp build/react-dom-server.js
     <gecko-dev>/devtools/client/shared/vendor/react-dom-server.js
  ```
