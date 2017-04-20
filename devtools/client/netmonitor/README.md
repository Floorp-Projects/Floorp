# Network Monitor

The Network Monitor (netmonitor) shows you all the network requests Firefox makes (for example, when a page is loaded or when an XMLHttpRequest is performed) , how long each request takes, and details of each request. You can edit the method, query, header and resend the request as well. Read [MDN](https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor) to learn how to use the tool.

## Prerequisite

If you want to build the Network Monitor inside of the DevTools toolbox (Firefox Devtools Panels), follow the [simple Firefox build](https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build) document in MDN. Start your compiled firefox and open the Firefox developer tool, you can see the Network Monitor inside.

If you would like to run the Network Monitor in the browser tab (experimental), you need following packages:

* [node](https://nodejs.org/) >= 6.9.x
* [npm](https://docs.npmjs.com/getting-started/installing-node) >= 3.x
* [yarn](https://yarnpkg.com/docs/install) >= 0.21.x
* [Firefox](https://www.mozilla.org/firefox/new/) either the released version or build from the source code.

Once `node` (`npm` included) is installed, use the following command to install `yarn`.

```
$ npm install -g yarn
```

## Quick Setup

Navigate to the `netmonitor` folder with your terminal, then run the following commands:

```bash
# Install packages
yarn install

# Create a dev server instance for hosting netmonitor on browser
yarn start

# Run firefox
firefox http://localhost:8000 --start-debugger-server 6080
```

Then open `localhost:8000` in any browser to see all tabs in Firefox.

### More detailed setup

If you have an opened Firefox browser, you can manually configure Firefox as well.

Type `about:config` in Firefox URL field, grant the warning to access preferences. We need to set two preferences:

* disable `devtools.debugger.prompt-connection` to remove the connection prompt.
* enable `devtools.debugger.remote-enabled` to allow remote debugging a browser tab via the Mozilla remote debugging protocol (RDP)

Go to the Web Developer menu in Firefox and select [Developer Toolbar](https://developer.mozilla.org/docs/Tools/GCLI). Run the command

`listen 6080 mozilla-rdp`

The command will make Firefox act as a remote debugging server.

Then run the command

`yarn start`

### How it works

The Network Monitor uses [webpack](https://webpack.js.org/) and several packages from [devtools-core](https://github.com/devtools-html/devtools-core) to run the Network Monitor as a normal web page. The Network Monitor uses [Mozilla remote debugging protocol](http://searchfox.org/mozilla-central/source/devtools/docs/backend/protocol.md) to fetch result and execute commands from Firefox browser.

Open `localhost:8000` in any browser to see the [launchpad](https://github.com/devtools-html/devtools-core/tree/master/packages/devtools-launchpad) interface. Devtools Launchpad will communicate with Firefox (the remote debugging server) and list all opened tabs from Firefox. Click one of the browser tab entry, now you can see the Network Monitor runs in a browser tab.

### Develop with related modules

When working on make the Network Monitor running in the browser tab, you may need to work on external modules. Besides the third party modules, here are modules required for the Network Monitor and is hosted under `devtools-html` (modules shared accross Devtools):

* [devtools-config](https://github.com/devtools-html/devtools-core/blob/master/packages/devtools-config/#readme) config used in dev server
* [devtools-launchpad](https://github.com/devtools-html/devtools-core/blob/master/packages/devtools-launchpad/#readme) provide the dev server, landing page and the bootstrap functions to run devtools in the browser tab.
* [devtools-modules](https://github.com/devtools-html/devtools-core/blob/master/packages/devtools-modules/#readme) Devtools shared and shim modules.
* [devtools-source-editor](https://github.com/devtools-html/devtools-core/blob/master/packages/devtools-source-editor/#readme) Source Editor component.
* [devtools-reps](https://github.com/devtools-html/reps/#readme) remote object formatter for variables representation.

Do `yarn link` modules in related module directory, then do `yarn link [module-name]` after `yarn install` modules.

## Code Structure

Top level files are used to launch the Network Monitor inside of the DevTools toolbox or run in the browser tab (experimental). The Network Monitor source is mainly located in the `src/` folder, the same code base is used to run in both environments.

We prefer use web standard API instead of FIrefox specific API, to make the Network Monitor can be opened in any browser tab.

### Run inside of the DevTools toolbox

Files used to run the Network Monitor inside of the DevTools toolbox.

* `panel.js` called by devtools toolbox to launch the Network Monitor panel.
* `index.html` panel UI and launch scripts.
* `src/utils/firefox/` wrap function call for Firefox specific API. Files in this folder should provide alias in `webpack.config.js` to mock or polyfill those function call. Therefore run the Network Monitor in the browser tab would work.

### Run in the browser tab (experimental)

Files used to run the Network Monitor in the browser tab

* `bin/` files to launch test server.
* `configs/` dev configs.
* `index.js` the entry point, equivalent to `index.html`.
* `webpack.config.js` the webpack config file, including plenty of module alias map to shims and polyfills.
* `package.json` declare every required packages and available commands.

To run in the browser tab, the Network Monitor needs to get some dependencies from npm module. Check `package.json` to see all dependencies. Check `webpack.config.js` to find the module alias, and check [devtools-core](https://github.com/devtools-html/devtools-core) packages to dive into actual modules used by the Network Monitor and other Devtools.

### UI

The Network Monitor UI is built using [React](http://searchfox.org/mozilla-central/source/devtools/docs/frontend/react.md) components (in `src/components/`).

* **MonitorPanel** in `monitor-panel.js` is the root element.
* Three major container components are
  - **Toolbar** Panel related functions.
  - **RequestList** Show each request information.
  - **NetworkDetailsPanel** Show detailed infomation per request.
* `src/assets` Styles that affect the Network Monitor panel.

We prefer stateless component (define by function) instead of stateful component (define by class) unless the component has to maintain its internal state.

### State

Besides the UI, the Network Monitor manages the app state via [Redux](http://searchfox.org/mozilla-central/source/devtools/docs/frontend/redux.md). The following locations define the app state:

* `src/constants.js` constants used across the tool including action and event names.
* `src/actions/` for all actions that change the state.
* `src/reducers/` for all reducers that change the state.
* `src/selectors/` functions that return a formatted version of parts of the app state.

We use [immutable.js](http://facebook.github.io/immutable-js/) and [reselect](https://github.com/reactjs/reselect) libraries to return new a state object efficiently.
