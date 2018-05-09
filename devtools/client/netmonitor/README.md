# Network Monitor

The Network Monitor (netmonitor) shows you all the network requests Firefox makes (for example, when a page is loaded or when an XMLHttpRequest is performed) , how long each request takes, and details of each request. You can edit the method, query, header and resend the request as well. Read [MDN](https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor) to learn all the features and how to use the tool.

## Prerequisite

If you want to build the Network Monitor inside of the DevTools toolbox (Firefox Devtools Panels), follow the [simple Firefox build](https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build) document in MDN. Start your compiled firefox and open the Firefox developer tool, you can see the Network Monitor inside.

If you would like to run the Network Monitor in the browser tab (experimental), you need following packages:

* [node](https://nodejs.org/) >= 6.9.x JavaScript runtime.
* [yarn](https://yarnpkg.com/docs/install) >= 0.21.x the package dependency management tool.
* [Firefox](https://www.mozilla.org/firefox/new/) any version or build from the source code.

## Quick Setup

Navigate to the `mozilla-central/devtools/client/netmonitor` folder with your terminal.
Note that this folder is available after `mozilla-central` was cloned in order to get a local copy of the repository. Then run the following commands:

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

Instead of running command to open a new Firefox window like

```
firefox http://localhost:8000 --start-debugger-server 6080
```

If you have an opened Firefox browser, you can manually configure Firefox via type `about:config` in Firefox URL field, grant the warning to access preferences. And set these two preferences:

* disable `devtools.debugger.prompt-connection` to remove the connection prompt.
* enable `devtools.debugger.remote-enabled` to allow remote debugging a browser tab via the Mozilla remote debugging protocol (RDP)

Go to the Web Developer menu in Firefox and select [Developer Toolbar](https://developer.mozilla.org/docs/Tools/GCLI). Run the command

`listen 6080 mozilla-rdp`

The command will make Firefox act as a remote debugging server.

Run the command

`yarn start`

Then open `localhost:8000` in any browser to see all tabs in Firefox.

### How it works

The Network Monitor uses [webpack](https://webpack.js.org/) and several packages from [devtools-core](https://github.com/devtools-html/devtools-core) to run the Network Monitor as a normal web page. The Network Monitor uses [Mozilla remote debugging protocol](http://searchfox.org/mozilla-central/source/devtools/docs/backend/protocol.md) to fetch result and execute commands from Firefox browser.

![](https://hacks.mozilla.org/files/2017/06/image4.png)

Open `localhost:8000` in any browser to see the [launchpad](https://github.com/devtools-html/devtools-core/tree/master/packages/devtools-launchpad) interface. Devtools Launchpad will communicate with Firefox (the remote debugging server) and list all opened tabs from Firefox. Click one of the browser tab entry, now you can see the Network Monitor runs in a browser tab.

### Develop with related modules

When working on make the Network Monitor running in the browser tab, you may need to work on external modules. Besides the third party modules, here are modules required for the Network Monitor and is hosted under `devtools-html` (modules shared across Devtools):

* [devtools-config](https://github.com/devtools-html/devtools-core/blob/master/packages/devtools-config/#readme) config used in dev server
* [devtools-launchpad](https://github.com/devtools-html/devtools-core/blob/master/packages/devtools-launchpad/#readme) provide the dev server, landing page and the bootstrap functions to run devtools in the browser tab.
* [devtools-modules](https://github.com/devtools-html/devtools-core/blob/master/packages/devtools-modules/#readme) Devtools shared and shim modules.
* [devtools-source-editor](https://github.com/devtools-html/devtools-core/blob/master/packages/devtools-source-editor/#readme) Source Editor component.
* [devtools-reps](https://github.com/devtools-html/debugger.html/blob/master/packages/devtools-reps/#readme) remote object formatter for variables representation.

Do `yarn link` modules in related module directory, then do `yarn link [module-name]` after `yarn install` modules.

## Code Structure

Top level files are used to launch the Network Monitor inside of the DevTools toolbox or run in the browser tab (experimental). The Network Monitor source is mainly located in the `src/` folder, the same code base is used to run in both environments.

We prefer use web standard API instead of FIrefox specific API, to make the Network Monitor can be opened in any browser tab.

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
