# Performance New

This folder contains the code for the new performance panel that is a simplified recorder that works to record a performance profile, and inject it into profiler.firefox.com. This tool is not in charge of any of the analysis, only the recording.

## TypeScript

This project contains TypeScript types in JSDoc comments. To run the type checker point your terminal to this directory, and run `yarn install`, then `yarn test`. In addition type hints should work if your editor is configured to speak TypeScript.

## Overall Architecture

The performance panel is split into two different modes. There is the DevTools panel mode, and the browser menu bar "popup" mode. The UI is implemented for both in `devtools/client/performance-new`, and many codepaths are shared. Both use the same React/Redux setup, but then each are configured with slightly different workflows. These are documented below.

### DevTools Panel

This panel works similarly to the other DevTools panels. The `devtools/client/performance-new/initializer.js` is in charge of initializing the page specifically for the DevTools workflow. This script creates a `PerfActor` that is then used for talking to the Gecko Profiler component on the debuggee. This path is specifically optimized for targeting Firefox running on Android phones. This workflow can also target the current browser, but the overhead from DevTools can skew the results, making the performance profile less accurate.

### Profiler Popup

The popup can be enabled by going to `Tools -> Web Developer -> Enable Profiler Toolbar Icon". This then runs the initializer in`devtools/client/performance-new/popup/initializer.js`, and configures the UI to work in a configuration that is optimized for profiling the current browser. The Gecko Profiler is turned on (using the ActorReadyGeckoProfilerInterface), and is queried directly–bypassing the DevTools' remote debugging protocol.

## Injecting profiles into [profiler.firefox.com]

After a profile has been collected, it needs to be sent to [profiler.firefox.com] for analysis. This is done by using browser APIs to open a new tab, and then injecting the profile into the page through a frame script. See `frame-script.js` for implementation details. Both the DevTools Panel and the Popup use this frame script.

[profiler.firefox.com]: https://profiler.firefox.com
