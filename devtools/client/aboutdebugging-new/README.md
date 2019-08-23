# about:debugging-new

## What is about:debugging-new
The purpose of about:debugging is to be a debugging hub to start inspecting your addons, processes, tabs and workers. This new version of about:debugging will also allow you to debug remote devices (Firefox for Android on a smartphone). The user should be able to connect either via USB or WiFi. This solution is supposed to replace the various existing remote debugging solutions available in Firefox DevTools, WebIDE and the Connect page.

To try out about:debugging, type `about:debugging` in the Firefox URL bar.

## Technical overview

The about:debugging-new UI is built using React and Redux. The various React/Redux files should be organized as follows:
- devtools/client/aboutdebugging-new/src/actions
- devtools/client/aboutdebugging-new/src/components
- devtools/client/aboutdebugging-new/src/middleware
- devtools/client/aboutdebugging-new/src/reducers

The folder `devtools/client/aboutdebugging-new/src/modules` contains various helpers and classes that are not related to React/Redux. For instance modules/usb-runtimes.js provides an abstraction layer to enable USB runtimes scanning, to list USB runtimes etc...

### Firefox Component Registration
about:debugging-new is an "about" page registered via a component manifest that is located in `/devtools/startup/aboutdebugging.manifest`. The component registration code is at `/devtools/startup/aboutdebugging-registration.js`.

### Actions
Actions should cover all user or external events that change the UI.

#### asynchronous actions
For asynchronous actions, we will use the thunk middleware, similar to what it done in the webconsole and netmonitor. An asynchronous action should be split in three actions:
- start
- success
- failure

As we will implement asynchronous, we should aim to keep a consistent naming for those actions.

A typical usecase for an asynchronous action here would be selecting a runtime. The selection will be immediate but should trigger actions to retrieve tabs, addons, workers etc… which will all be asynchronous.

### Components
Components should avoid dealing with specialized objects as much as possible.

They should never use any of the lifecycle methods that will be deprecated in React 17 (`componentWillMount`, `componentWillReceiveProps`, and `componentWillUpdate`).

When it comes to updating the state, components should do it via an action if the result of this action is something that should be preserved when reloading the UI.

### Middlewares
We use several middlewares in the application. The middlewares are required in the create-store module `devtools/client/aboutdebugging-new/src/create-store.js`.

#### thunk
This is a shared middleware used by other DevTools modules that allows to dispatch actions and allows to create multiple asynchronous actions.

#### debug-target-listener
This middleware is responsible for monitoring a client. It will add various listeners on the client when receiving the `WATCH_RUNTIME_SUCCESS` action and will remove the listeners when receiving `UNWATCH_RUNTIME_SUCCESS`. Could probably be ported to an action, no real added value as a middleware.

#### error-logging
This middleware will forward any error object dispatched by an action to the console. Typically, all the `FAILURE` actions of our asynchronous actions should dispatch an error object.

#### extension/tab/worker-component-data
This middleware takes the raw data of a debugging target returned by the devtools client and transforms it to presentation data that can be used by the debugtarget components. Could probably be ported to an action, no real added value as a middleware.

#### waitUntilService
This is a shared middleware used by other DevTools modules that allows to wait until a given action is dispatched. We use it in mochitests.

### state
The state represents the model for the UI of the application.

##### ui
Holds the global state of the application. This can contain generic preferences such as "is feature X enabled" or purely UI related "is category X collapsed".

##### runtimes
Holds the current list of runtimes known by the application as well as the currently selected runtime.

##### debug-targets
Holds all the debug targets (ie tabs, addons, workers etc...) for the currently monitored runtime. There is at most one monitored runtime at a given time.

### Constants
Constants can be found at `devtools/client/aboutdebugging-new/src/constants.js`. It contains all the actions available in about:debugging as well as several other "packages" of constants used in the application.

If a constant (string, number, etc...) is only used in a single module it can be defined as a `const` in this module. However as soon as the constant is shared by several modules, it should move to `devtools/client/aboutdebugging-new/src/constants.js`.

### Types
Types can be found at `devtools/client/aboutdebugging-new/src/types.js`. They serve both as documentation as well as validation. We do not aim at having types for every single element of the state, but only for complex objects.

## Contact
If you have any questions about the code, features, planning, the active team is:
- engineering: Belén Albeza (:ladybenko)
- engineering: Daisuke Akatsuka (:daisuke)
- engineering: Julian Descottes (:jdescottes)
- engineering: Ola Gasidlo (:ola))
- engineering management: Soledad Penadés (:sole)
- product management: Harald Kischner (:digitarald)

You can find us on [Slack](https://devtools-html-slack.herokuapp.com/) or IRC on #devtools at irc.mozilla.org.
