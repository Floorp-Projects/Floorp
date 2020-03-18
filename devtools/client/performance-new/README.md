# Performance New

This folder contains the code for the new performance panel that is a simplified recorder that works to record a performance profile, and inject it into profiler.firefox.com. This tool is not in charge of any of the analysis, only the recording.

## TypeScript

This project contains TypeScript types in JSDoc comments. To run the type checker point your terminal to this directory, and run `yarn install`, then `yarn test`. In addition type hints should work if your editor is configured to speak TypeScript.

## Overall Architecture

This project has a few different views explained below.

### DevTools

This is a simplified recording panel that includes a preset dropdown. It's embedded in the DevTools. It's not the preferred way to use the profiler, but is included so that users are comfortable with existing workflows. This is built using React/Redux. The store's code is shared by all the views, but each view initializes it separately. The popup does not use React/Redux (explained later). When editing a custom preset, it takes you to "about:profiling" in a new tab.

This panel works similarly to the other DevTools panels. The `devtools/client/performance-new/initializer.js` is in charge of initializing the page specifically for the DevTools workflow. This script creates a `PerfActor` that is then used for talking to the Gecko Profiler component.

### DevTools Remote

This is the same UI and codebase as the DevTools panel, but it's accessible from about:debugging for remote targets. It uses the PerfFront for a remote target to profile on the remote device. When editing a custom preset, it takes you to "about:profiling" in the same modal.

This page is initialized with the `PerfActor`, but it will target a remote debuggee, like an Android Phone.

### about:profiling

This view uses React/Redux for the UI, and is a full page for configuring the profiler. There are no controls for recording a profile, only editing the settings. It shares the same Redux store code as DevTools (instantiated separately), but uses different React components.

### about:profiling Remote

This is the remote view of the about:profiling page. It is embedded in the about:debugging profiler modal dialog, and it is initialized by about:debugging. It uses preferences that are postfixed with ".remote", so that a second set of preferences are shared for how remote profiling is configured.

### Profiler Popup

The popup can be enabled by going to `Tools -> Web Developer -> Enable Profiler Toolbar Icon", or by visiting [profiler.firefox.com] and clicking `Enable Profiler Menu Button`. This UI is not a React Redux app, but has a vanilla browser chrome implementation. This was done to make the popup as fast as possible, with a trade-off of some complexity with dealing with the non-standard (i.e. not a normal webpage) browser chrome environment. The popup is designed to be as low overhead as possible in order to get the cleanest performance profiles. Special care must be taken to not impact browser startup times when working with this implementation, as it also turns on the global profiler shortcuts.

At the time of this writing, the popup feature is not enabled by default. This pref is `"devtools.performance.popup.feature-flag"`.

## Injecting profiles into [profiler.firefox.com]

After a profile has been collected, it needs to be sent to [profiler.firefox.com] for analysis. This is done by using browser APIs to open a new tab, and then injecting the profile into the page through a frame script. See `frame-script.js` for implementation details. Both the DevTools Panel and the Popup use this frame script.

[profiler.firefox.com]: https://profiler.firefox.com
