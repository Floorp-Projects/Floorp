# Network Monitor

The Network Monitor (netmonitor) shows you all the network requests Firefox makes (for example, when a page is loaded or when an XMLHttpRequest is performed) , how long each request takes, and details of each request. You can edit the method, query, header and resend the request as well. Read [more](https://firefox-source-docs.mozilla.org/devtools-user/network_monitor/) to learn all the features and how to use the tool.

### UI

The Network Monitor UI is built using [React](http://searchfox.org/mozilla-central/source/devtools/docs/frontend/react.md) components (in `src/components/`).

* **MonitorPanel** in `MonitorPanel.js` is the root element.
* Three major container components are
  - **Toolbar** Panel related functions.
  - **RequestList** Show each request information.
  - **NetworkDetailsBar** Show detailed information per request.
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
