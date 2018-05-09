# WebConsole

The WebConsole (webconsole) shows you all the console API calls made by scripts and alerts
you when javascript errors are thrown by a script.
It can also display network logs, and you can evaluate expressions using the console
input, a.k.a. JsTerm. You can read more about it on [MDN](https://developer.mozilla.org/en-US/docs/Tools/Web_Console)
to learn all the features and how to use the tool.

## Old / New frontend

The current console used in the toolbox is called the new frontend, and the code lives at
the root of the `devtools/client/webconsole/` folder.
The old console code is located in the `devtools/client/webconsole/old` folder.
Both frontends use the same code for the console input, also called JsTerm (see `jsterm.js`).
The old frontend is still used for the Browser Console, but is planned to be removed in the
near future (see Bug 1381834).

## Run WebConsole in DevTools panel

If you want to build the WebConsole inside of the DevTools toolbox (Firefox Devtools Panels),
follow the [simple Firefox build](http://docs.firefox-dev.tools/getting-started/build.html)
document in MDN. Start your compiled firefox and open the Firefox developer tool, you can
then see the WebConsole tab.

## Run WebConsole in a browser tab (experimental)

### Prerequisite

If you would like to run the WebConsole in the browser tab, you need following packages:

* [node](https://nodejs.org/) >= 7.10.0 JavaScript runtime.
* [yarn](https://yarnpkg.com/docs/install) >= 1.0.0 the package dependency management tool.
* [Firefox](https://www.mozilla.org/firefox/new/) any version or build from the source code.

### Run WebConsole

Navigate to the `mozilla-central/devtools/client/webconsole` folder with your terminal.
Note that this folder is available after `mozilla-central` was cloned in order to get a
local copy of the repository. Then run the following commands:

```bash
# Install packages
yarn install

# Create a dev server instance for hosting webconsole on browser
yarn start
```

Open `localhost:8000` to see what we call the [launchpad](https://github.com/devtools-html/devtools-core/tree/master/packages/devtools-launchpad).
The UI that let you start a new Firefox window which will be the target (or debuggee).
Launchpad will communicate with Firefox (the remote debugging server) and list all opened tabs from Firefox.
You can then navigate to a website you want in the target window, and you should see it appears
in the `localhost:8000` page. Clicking on it will start the WebConsole in the browser tab.

### How it works

The WebConsole uses [webpack](https://webpack.js.org/) and several packages from [devtools-core](https://github.com/devtools-html/devtools-core)
to run as a normal web page. The WebConsole uses [Mozilla remote debugging protocol](http://searchfox.org/mozilla-central/source/devtools/docs/backend/protocol.md)
to fetch messages and execute commands against Firefox.

Open `localhost:8000` in any browser to see the
interface. Devtools Launchpad will communicate with Firefox (the remote debugging server)
and list all opened tabs from Firefox. Click one of the browser tab entry, now you can see
the WebConsole runs in a browser tab.

### DevTools shared modules

When working on console running via launchpad, you may need to modify code on external modules.
Besides the third party modules, here are modules required for the WebConsole
(hosted under the `devtools-core` Github repo, which contains modules shared across Devtools).

* [devtools-config](https://github.com/devtools-html/devtools-core/blob/master/packages/devtools-config/#readme) config used in dev server
* [devtools-launchpad](https://github.com/devtools-html/devtools-core/blob/master/packages/devtools-launchpad/#readme) provide the dev server, landing page and the bootstrap functions to run devtools in the browser tab.
* [devtools-modules](https://github.com/devtools-html/devtools-core/blob/master/packages/devtools-modules/#readme) Devtools shared and shim modules.
* [devtools-source-editor](https://github.com/devtools-html/devtools-core/blob/master/packages/devtools-source-editor/#readme) Source Editor component.
* [devtools-reps](https://github.com/devtools-html/debugger.html/blob/master/packages/devtools-reps/#readme) remote object formatter for variables representation.

Changes to those modules need to be done on Github, using the Pull Request workflow.
Then, a new version of the modified package need to be released on npm so the version number
can be updated in WebConsole's `package.json`. Some modules have a release process,
look for `RELEASE.md` file in the module folder, or ask a maintainer if you are
unsure about the release process.

## Code Structure

Top level files are used to launch the WebConsole inside of the DevTools toolbox or run in
the browser tab (experimental). The same code base is used to run in both environments.

### Run inside of the DevTools toolbox

Files used to run the WebConsole inside of the DevTools toolbox.

* `main.js` called by devtools toolbox to launch the WebConsole panel.
* `webconsole.html` panel UI and launch scripts.

### Run in the browser tab (experimental)

Files used to run the WebConsole in the browser tab

* `bin/` files to launch test server.
* `configs/` dev configs.
* `local-dev/index.js` the entry point, equivalent to `webconsole.html`.
* `webpack.config.js` the webpack config file, including plenty of module aliases map to shims and polyfills.
* `package.json` declare every required packages and available commands.

To run in the browser tab, the WebConsole needs to get some dependencies from npm module.
Check `package.json` to see all dependencies. Check `webpack.config.js` to find the module alias,
and check [devtools-core](https://github.com/devtools-html/devtools-core) packages to dive
into actual modules used by the WebConsole and other Devtools.

### UI

The WebConsole UI is built using [React](http://docs.firefox-dev.tools/frontend/react.html)
components (in `components/`).

The React application is rendered from `new-console-output-wrapper.js`.
It contains 3 top components:
* **ConsoleOutput** (in `ConsoleOutput.js`) is the component where messages are rendered.
* **FilterBar** (in `FilterBar.js`) is the component for the filter bars (filter input and toggle buttons).
* **SideBar** (in `SideBar.js`) is the component that render the sidebar where objects can be placed in.

We prefer stateless component (defined by function) instead of stateful component
(defined by class) unless the component has to maintain its internal state.

### State

Besides the UI, the WebConsole manages the app state via [Redux](When working on console running via launchpad).
The following locations define the app state:

* `src/constants.js` constants used across the tool including action and event names.
* `src/actions/` for all actions that change the state.
* `src/reducers/` for all reducers that change the state.
* `src/selectors/` functions that return a formatted version of parts of the app state.

The redux state is a plain javascript object with the following properties:
```js
{
  // State of the filter input and toggle buttons
  filters,
  // Console messages data and state (hidden, expanded, groups, …)
  messages,
  // Preferences (persist message, message limit, …)
  prefs,
  // Interface state (filter bar visible, sidebar visible, …)
  ui,
}
```

### Tests

[See test/README.md](test/README.md)