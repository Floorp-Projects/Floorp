# Console Tool Architecture

The Console panel is responsible for rendering all logs coming from the current page.

## Architecture

Internal architecture of the Console panel (the client side) is described
on the following diagram.

<figure class="hero">
  <pre class="diagram">
┌──────────────────────────────┐               ┌────────────────────────┐
│           DevTools           │               │    WebConsolePanel     │
│[client/framework/devtools.js]│               │       [panel.js]       │
└──────────────────────────────┘               └────────────────────────┘
                │                                           │
                                                            │
     openBrowserConsole() or                                │
     toggleBrowserConsole()                                 │
                │                                           │
                ▼                                           │
┌──────────────────────────────┐                          {hud}
│    BrowserConsoleManager     │                            │
│ [browser-console-manager.js] │                            │
└──────────────────────────────┘                            │
                │                                           │
                │                                           │
      {_browserConsole}                                     │
                │                                           │
                ▼ 0..1                                      ▼ 1
┌──────────────────────────────┐               ┌────────────────────────┐
│        BrowserConsole        │               │       WebConsole       │
│     [browser-console.js]     │─ ─ extends ─ ▶│    [webconsole.js]     │
└──────────────────────────────┘               └──────────────1─────────┘
                                                            │
                                                          {ui}
                                                            │
                                                            ▼ 1
                                               ┌────────────────────────┐
                                               │      WebConsoleUI      │
                                               │   [webconsole-ui.js]   │
                                               └────────────────────────┘
                                                            │
                                                       {wrapper}
                                                            │
                                                            │
                                                            ▼ 1
                                               ┌────────────────────────┐
                                               │   WebConsoleWrapper    │
                                               │[webconsole-wrapper.js] │
                                               └────────────────────────┘
                                                            │
                                                        <renders>
                                                            │
                                                            ▼
                                               ┌────────────────────────┐
                                               │          App           │
                                               └────────────────────────┘
    </pre>
  <figcaption>Elements between curly bracket on arrows represent the property name of the reference (for example, the WebConsolePanel as a `hud` property that is a reference to the WebConsole instance)</figcaption>
</figure>

## Components

The Console panel UI is built on top of [React](../frontend/react.md). It defines set of React components in `components` directory
The React architecture is described on the following diagram.

<figure class="hero">
  <pre class="diagram">
┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─                                                              
  WebConsole React components                                                                                                                                          │                                                             
│ [/components]                                                       ┌────────────────────────┐                                                                                                                                     
                                                                      │          App           │                                                                       │                                                             
│                                                                     └────────────────────────┘                                                                                                                                     
                                                                                   │                                                                                   │                                                             
│                                                                                  │                                                                                                                                                 
        ┌───────────────────┬──────────────────────┬───────────────────┬───────────┴─────────┬───────────────────────┬────────────────────┬─────────────────┐          │                                                             
│       │                   │                      │                   │                     │                       │                    │                 │                                                                        
        ▼                   ▼                      ▼                   ▼                     ▼                       ▼                    ▼                 ▼          │       ┌────────────────────────────────────────┐            
│ ┌──────────┐     ┌────────────────┐     ┌────────────────┐     ┌───────────┐    ┌────────────────────┐     ┌──────────────┐     ┌──────────────┐     ┌─────────┐             │                 Editor                 │            
  │ SideBar  │     │NotificationBox │     │ ConfirmDialog  │     │ FilterBar │    │ ReverseSearchInput │     │ConsoleOutput │     │EditorToolbar │     │ JSTerm  │──.editor───▶│              <CodeMirror>              │            
│ └──────────┘     └────────────────┘     │    <portal>    │     └───────────┘    └────────────────────┘     └──────────────┘     └──────────────┘     └─────────┘             │ [client/shared/sourceeditor/editor.js] │            
        │                                 └────────────────┘           │                                             │                                                 │       └────────────────────────────────────────┘            
│       │                                                    ┌─────────┴─────────────┐                               │                                                                                                               
        │                                                    │                       │                               │                                                 │                                                             
│       │                                                    ▼                       ▼                               ▼                                                                                                               
        │                                          ┌──────────────────┐    ┌──────────────────┐            ┌──────────────────┐                                        │                                                             
│       │                                          │   FilterButton   │    │  FilterCheckbox  │            │ MessageContainer │                                                                                                      
        │                                          └──────────────────┘    └──────────────────┘            └──────────────────┘                                        │                                                             
│       │                                                                                                            │                                                                                                               
        │                                                                                                            │                                                 │                                                             
│       │                                                                                                            │                                                                                                               
        │                                                                                                            ▼                                                 │                                                             
│       │                                                                                                  ┌──────────────────┐                                                                                                      
        │                                                                                                  │     Message      │                                        │                                                             
│       │                                                                                                  └──────────────────┘                                                                                                      
        │                                                                                                            │                                                 │         ┌─────────────────────────────────────┐             
│       │                                                                                                            │                                                           │                Frame                │             
        │                                  ┌─────────────────────┬─────────────────────┬─────────────────────┬───────┴─────────────┬─────────────────────┬─────────────┼─────┬──▶│ [client/shared/components/Frame.js] │             
│       │                                  │                     │                     │                     │                     │                     │                   │   └─────────────────────────────────────┘             
        │                                  ▼                     ▼                     ▼                     ▼                     ▼                     ▼             │     │                                                       
│       │                        ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐          │   ┌────────────────────────────────────────┐          
        │                        │  MessageIndent   │  │   MessageIcon    │  │  CollapseButton  │  │ GripMessageBody  │  │   ConsoleTable   │  │  MessageRepeat   │    │     │   │               SmartTrace               │          
│       │                        └──────────────────┘  └──────────────────┘  └──────────────────┘  └──────────────────┘  └──────────────────┘  └──────────────────┘          ├──▶│[client/shared/components/SmartTrace.js]│          
        │                                                                                                    │                     │                                   │     │   └────────────────────────────────────────┘          
└ ─ ─ ─ ┼ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─│─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─│─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─      │                                                       
        │                                                                                                    │                     │                                         │   ┌──────────────────────────────────────────────────┐
        │                                                                                                    │                     │                                         │   │                   TabboxPanel                    │
        │                                                                                                    ├─────────────────────┘                                         └──▶│[client/netmonitor/src/components/TabboxPanel.js] │
        │                                                                                                    │                                                                   └──────────────────────────────────────────────────┘
        │                                                                                                    │                                                                                                                       
        │                                                                                                    │                                                                                                                       
        │                                                                                                    ▼                                                                                                                       
        │                                                                ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─                                                                                     
        │                                                                  Reps                                       ┌──────────────────────┐  │                                                                                    
        │                                                                │ [client/shared/components/reps/reps.js]    │   ObjectInspector    │                                                                                       
        │                                                                                                             └──────────────────────┘  │                                                                                    
        │                                                                │                                                        │                                                                                                  
        │                                                                                                                         ▼             │                                                                                    
        │                                                                │                                            ┌──────────────────────┐                                                                                       
        │                                                                                                             │ ObjectInspectorItem  │  │                                                                                    
        │                                                                │                                            └──────────────────────┘                                                                                       
        └───────────────────────────────────────────────────────────────▶                                                         │             │                                                                                    
                                                                         │                                                        ▼                                                                                                  
                                                                                                                      ┌──────────────────────┐  │                                                                                    
                                                                         │                                         ┌─▶│         Rep          │                                                                                       
                                                                                                                   │  └──────────────────────┘  │                                                                                    
                                                                         │                                         │              │                                                                                                  
                                                                                                                   │              │             │                                                                                    
                                                                         │                                         └──────────────┘                                                                                                  
                                                                          ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘                                                                                                                                                         
  </pre>
</figure>

There are several external components we use from the WebConsole:
- ObjectInspector/Rep: Used to display a variable in the console output and handle expanding the variable when it's not a primitive.
- Frame: Used to display the location of messages.
- SmartTrace: Used to display the stack trace of messages and errors
- TabboxPanel: Used to render a network message detail. This is directly using the component from the Netmonitor so we are consistent in how we display a request internals.

## Actions

The Console panel implements a set of actions divided into several groups.

- **Filters** Actions related to content filtering.
- **Messages** Actions related to list of messages rendered in the panel.
- **UI** Actions related to the UI state.

### State

The Console panel manages the app state via [Redux](../frontend/redux.md).

There are following reducers defining the panel state:

- `reducers/filters.js` state for panel filters. These filters can be set from within the panel's toolbar (e.g. error, info, log, css, etc.)
- `reducers/messages.js` state of all messages rendered within the panel.
- `reducers/prefs.js` Preferences associated with the Console panel (e.g. logLimit)
- `reducers/ui.js` UI related state (sometimes also called a presentation state). This state contains state of the filter bar (visible/hidden), state of the time-stamp (visible/hidden), etc.
