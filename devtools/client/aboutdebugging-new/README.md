# about:debugging-new architecture document

## document purpose
The purpose of this document is to explain how the new about:debugging UI is built using React and Redux. It should serve both as a documentation resource and as a technical specification. Changes impacting the architecture should preferably be discussed and reflected here before implementation.

## high level architecture
about:debugging-new is an "about" page registered via a component manifest that is located in `/devtools/startup/aboutdebugging.manifest`. The component registration code is at `/devtools/startup/aboutdebugging-registration.js` and mostly contains the logic to switch between the old and the new about:debugging UI, based on the value of the preference `devtools.aboutdebugging.new-enabled`.

The about:debugging-new UI is built using React and Redux. The various React/Redux files should be organized as follows:
- devtools/client/aboutdebugging-new/src/actions
- devtools/client/aboutdebugging-new/src/components (js and css)
- devtools/client/aboutdebugging-new/src/middleware
- devtools/client/aboutdebugging-new/src/reducers

### actions
Actions should cover all user or external events that change the UI.

#### external actions

##### to be implemented
  - Runtime information updated
  - List of devices updated
  - List of addons/tabs/workers(...) updated
  - Connection status of device changes
  - ADBExtension is enabled/disabled/in-progress
  - WiFi debugging is enabled/disabled/in-progress
  - Network location list is updated

#### user actions

##### already implemented
  - sidebar
   - actions/ui:selectPage returns PAGE_SELECTED action

##### to be implemented
  - sidebar
    - Select device (extended version of selectPage)
    - Connect to device
    - Cancel connection to device
  - device page
    - Disconnect from device
    - Take screenshot
    - Toggle collapsible section
    - Add tab on device
    - Inspect a debugging target
  - device page - addon controls
    - enable addon debugging
    - Load temporary addon
    - Reload temporary addon
    - Remove temporary addon
  - device page - worker controls
    - Start worker
    - Debug worker
    - Push worker
    - Unregister worker
  - connect page
    - Add network location
    - Remove network location

#### asynchronous actions
For asynchronous actions, we will use the thunk middleware, similar to what it done in the webconsole and netmonitor. An asynchronous action should be split in three actions:
- start
- success
- failure

As we will implement asynchronous, we should aim to keep a consistent naming for those actions.

A typical usecase for an asynchronous action here would be selecting a runtime. The selection will be immediate but should trigger actions to retrieve tabs, addons, workers etc… which will all be asynchronous.

### components
Components should avoid dealing with specialized objects as much as possible.

They should never use any of the lifecycle methods that will be deprecated in React 17 (componentWillMount, componentWillReceiveProps, and componentWillUpdate).

When it comes to updating the state, components should do it via an action if the result of this action is something that should be preserved when reloading the UI.

### state and reducers
The state represents the model for the UI of the application.

#### state
This is a representation of our current state.

  ui:
    - selectedPage: String

##### to be implemented
Some ideas of how we could extend the state. Names, properties, values are all up for discussion.

  runtimes: Array of runtimes (only currently detected runtimes)
  targets: (currently visible targets, only relevant on a device page)
    - addons: Array of target
    - processes: Array of target
    - tabs: Array of target
    - workers: Array of target
  config:
    - adbextension: boolean
    - wifi: boolean
    - network-locations: Array of network-location

#### types
Other panels are sometimes relying on a types.js file committed under src, providing types that can be shared and reused in various places of the application.

Example of types.js file:
  https://searchfox.org/mozilla-central/source/devtools/client/inspector/fonts/types.js

We might follow that kind of approach when we need to reuse similar objects in several components.

Examples of types that we could introduce here:
  runtime:
    - type
    - name
    - icon
    - version

  target:
    - type
    - name
    - icon
    - details

  network-location:
    - host: String
    - port: String
