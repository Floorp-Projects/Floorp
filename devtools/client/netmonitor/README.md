# Network Monitor

The Network Monitor (netmonitor) shows you all the network requests Firefox makes (for example, when a page is loaded or when an XMLHttpRequest is performed) , how long each request takes, and details of each request. You can edit the method, query, header and resend the request as well. Read [MDN](https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor) to learn all the features and how to use the tool.

## Prerequisite

If you want to build the Network Monitor inside of the DevTools toolbox (Firefox Devtools Panels), follow the [simple Firefox build](https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build) document in MDN. Start your compiled firefox and open the Firefox developer tool, you can see the Network Monitor inside.

### Run inside of the DevTools toolbox

Files used to run the Network Monitor inside of the DevTools toolbox.

* `panel.js` called by devtools toolbox to launch the Network Monitor panel.
* `index.html` panel UI and launch scripts.
* `src/connector/` wrap function call for Browser specific API. Current support Firefox and Chrome(experimental).

### Run in the browser tab (experimental)

Files used to run the Network Monitor in the browser tab

* `bin/` files to launch test server.
* `configs/` dev configs.
* `launchpad.js` the entry point, equivalent to `index.html`.
* `webpack.config.js` the webpack config file, including plenty of module alias map to shims and polyfills.
* `package.json` declare every required packages and available commands.

To run in the browser tab, the Network Monitor needs to get some dependencies from npm module. Check `package.json` to see all dependencies. Check `webpack.config.js` to find the module alias, and check [devtools-core](https://github.com/devtools-html/devtools-core) packages to dive into actual modules used by the Network Monitor and other Devtools.

### UI

The Network Monitor UI is built using [React](http://searchfox.org/mozilla-central/source/devtools/docs/frontend/react.md) components (in `src/components/`).

* **MonitorPanel** in `MonitorPanel.js` is the root element.
* Three major container components are
  - **Toolbar** Panel related functions.
  - **RequestList** Show each request information.
  - **NetworkDetailsPanel** Show detailed information per request.
  - **StatusBar** Show statistics while loading.
* `src/assets` Styles that affect the Network Monitor panel.

We prefer stateless component (define by function) instead of stateful component (define by class) unless the component has to maintain its internal state.

### State

![](https://hacks.mozilla.org/files/2017/06/image8.png)

Besides the UI, the Network Monitor manages the app state via [Redux](http://searchfox.org/mozilla-central/source/devtools/docs/frontend/redux.md). The following locations define the app state:

* `src/constants.js` constants used across the tool including action and event names.
* `src/actions/` for all actions that change the state.
* `src/reducers/` for all reducers that change the state.
* `src/selectors/` functions that return a formatted version of parts of the app state.

We use [reselect](https://github.com/reactjs/reselect) library to perform state calculations efficiently.
