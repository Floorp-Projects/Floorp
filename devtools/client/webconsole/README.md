# WebConsole

The WebConsole (webconsole) shows you all the console API calls made by scripts and alerts
you when javascript errors are thrown by a script.
It can also display network logs, and you can evaluate expressions using the console
input, a.k.a. JsTerm. You can read more about it on [MDN](https://developer.mozilla.org/en-US/docs/Tools/Web_Console)
to learn all the features and how to use the tool.

## Run WebConsole

If you want to build the WebConsole inside of the DevTools toolbox (Firefox Devtools Panels),
follow the [simple Firefox build](http://docs.firefox-dev.tools/getting-started/build.html)
documentation. Start your compiled firefox and open the Firefox developer tool, you can
then see the WebConsole tab.

## Code Structure

Top level files are used to launch the WebConsole inside of the DevTools toolbox.
The main files used to run the WebConsole are:

* `main.js` called by devtools toolbox to launch the WebConsole panel.
* `index.html` panel UI and launch scripts.

### UI

The WebConsole UI is built using [React](http://docs.firefox-dev.tools/frontend/react.html)
components (in `components/`).

The React application is rendered from `webconsole-output-wrapper.js`.
It contains 4 top components:
* **ConsoleOutput** (in `ConsoleOutput.js`) is the component where messages are rendered.
* **FilterBar** (in `FilterBar.js`) is the component for the filter bars (filter input and toggle buttons).
* **SideBar** (in `SideBar.js`) is the component that render the sidebar where objects can be placed in.
* **JsTerm** (in `JsTerm.js`) is the component that render the console input.

We prefer stateless component (defined by function) instead of stateful component
(defined by class) unless the component has to maintain its internal state.

### State

Besides the UI, the WebConsole manages the app state via [Redux].
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
  // State of the input history
  history,
  // Console messages data and state (hidden, expanded, groups, …)
  messages,
  // State of notifications displayed on the output (e.g. self-XSS warning message)
  notifications,
  // Preferences (persist message, message limit, …)
  prefs,
  // Interface state (filter bar visible, sidebar visible, …)
  ui,
}
```

### Tests

[See test/README.md](test/README.md)
