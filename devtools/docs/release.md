# Recurring DevTools tasks

There are a few things we should do on each Nightly cycle to keep our code clean and up-to-date.

## Update MDN data for the Compatibility panel

Follow instructions from [devtools/client/inspector/compatibility/README.md](https://searchfox.org/mozilla-central/source/devtools/client/inspector/compatibility/README.md).

## Generate webidl-pure-allowlist.js and webidl-deprecated-list.js

The `webidl-pure-allowlist.js` file is used by the console instant evaluation, in order to know
if a given method does not have side effects. The file might be updated if new APIs are added,
or if methods are tagged as pure (or untagged).

The `webidl-deprecated-list.js` file will be used to avoid calling deprecated getters from devtools code.

1. Generating those files requires a non-artifact build. If you're mostly working with artifact builds, you might want to run `./mach bootstrap` in order to have a proper build environment.
2. Once the build is over, you should be able to follow instructions at the top of [GenerateDataFromWebIdls.py](https://searchfox.org/mozilla-central/source/devtools/shared/webconsole/GenerateDataFromWebIdls.py), which should be:
   2.1. Run the script with `./mach python devtools/shared/webconsole/GenerateDataFromWebIdls.py`

## Remove backwards compatibility code

In order to accommodate connecting to older server, we sometimes need to introduce specific branches in the code. At the moment, we only support connecting to server 2 versions older than the client (e.g. if the client is 87, we support connecting to 86 and 85).
This means that on each release there's an opportunity to cleanup backward compatibility code that was introduced for server we don't have to support anymore. If I go back to my example with the 87 client, we can remove any backward-compatibility code that was added in 85.

Luckily, when adding compatibility code, we also add comments that follow a specific pattern: `@backward-compat { version XX }`, where `XX` is the version number the code is supporting.

Back to our example where the current version is 87, we need to list all the comments added for 85. This can be done by doing a search with the following expression: `@backward-compat { version 85 }` (here's the searchfox equivalent: [searchfox query](https://searchfox.org/mozilla-central/search?q=%40backward-compat%5Cs*%7B%5Cs*version+85%5Cs*%7D&path=&case=false&regexp=true)).

Try to file a specific bug for each backward compatibility code you are removing (you can have broader bugs though, for example if you are removing a trait). Those bugs should block a META bug that will reference all the cleanups. You can check if a bug already exists in the main cleanup META bug ([Bug 1677944](https://bugzilla.mozilla.org/show_bug.cgi?id=1677944)), and if not, you can create it by visiting [this bugzilla link](https://bugzilla.mozilla.org/enter_bug.cgi?format=__default__&blocked=1677944&product=DevTools&component=General&short_desc=[META]%20Cleanup%20backward%20compatibility%20code%20added%20in%20YY&comment=YY%20is%20now%20in%20release,%20so%20we%20can%20remove%20any%20backward%20compatibility%20code%20that%20was%20added%20to%20support%20older%20servers&keywords=meta&bug_type=task) (make sure to replace `YY` with a version number that is equal to the current number minus 2; so if current release is 87, YY is 87 - 2 = 85).

## Smoke test remote debugging

### Setup

We will run the remote debugging smoke tests twice. Once to exercise backward compatibility, and once without backward compatibility (same version). The tests to run are the same in both cases (see Tests section).

You can use either desktop or mobile versions of Firefox as the server. Mobile is preferable as some codepaths are specific to Firefox mobile, but if you don't have access to an Android device, using a Desktop server is a decent alternative.

Instructions to setup remote debugging for Firefox mobile: https://developer.mozilla.org/en-US/docs/Tools/about:debugging#connecting_to_a_remote_device.
Instructions to setup remote debugging for Firefox desktop: https://gist.github.com/juliandescottes/b0d3d83154d9ea8a84db5d32aa35d2c1.

#### Backward compatibility test

- Start the current Nightly (release XX) as Client
- Prepare Firefox (release XX -1) as the Server. Either
  https://play.google.com/store/apps/details?id=org.mozilla.firefox_beta (mobile beta) or
  Desktop Beta or DevEdition

#### Same version test

- Start the current Nightly (release XX) as Client
- Prepare Firefox (also for release XX) as the Server. Either
  https://play.google.com/store/apps/details?id=org.mozilla.fenix (mobile nightly)
  or Desktop Nightly

### Tests

#### Basic connection:

- On the Client Firefox Nightly, open about:debugging
- Connect to the Server (either via network or USB)
- Open the corresponding Runtime Page

#### Debug targets:

- On the Server Firefox, open a tab to https://serviceworke.rs/strategy-network-or-cache_demo.html
- On the Client Firefox, check in the Runtime Page for the Server Firefox that you can see the new tab as well as the corresponding service worker
- On the Client Firefox, open the Profiler by clicking the Profile Performance button and record a short profile by clicking the Start, then the Stop button. Verify that the profiler opens a new tab with the recording.
- On the Server Firefox, close the tab you just opened
- On the Client Firefox, check that the corresponding tab is removed
- On the Client Firefox, unregister the service worker, check that the corresponding SW is removed from the list

#### Inspect a remote target:

- On the Server Firefox, open a tab to https://juliandescottes.github.io/webcomponents-playground/debugger-example/
- On the Client Firefox, click on Inspect for this tab. Check that toolbox opens. Now we will verify that the toolbox is working.
- Open Inspector, check that no panel is blank. Check that selecting another element in the markup-view updates the computed view.
- Open Console, check that you see the "script loaded" message. Type "1+1" in the console, check you get "2".
- Open Debugger, check that you can see the script.js source. Open it, put a breakpoint inside the clickMe() method (line 6). On the Server Firefox, click on the button in the page, check that you hit the breakpoint.
- Open the Network tab. If it is empty and tells you to "perform a request…", reload the page on the Server Firefox. Check that requests are displayed.

#### Inspect a remote extension:

- On the Server Firefox, install any extension (for instance https://addons.mozilla.org/en-US/firefox/addon/devtools-highlighter/ )
- On the Client Firefox, check the extension is displayed in the Extensions category
- Click on Inspect, check the toolbox opens.
- Check the Inspector, Console, Debugger and Netmonitor UIs for empty panels.

## Remove expired or renew telemetry probes

Bugs are automatically filed for all expired probes. You can find them using [this bugzilla query](https://bugzilla.mozilla.org/buglist.cgi?quicksearch=[probe-expiry-alert]%20devtools).

We should review the list of bugs and make sure each of them block the following [META bug](https://bugzilla.mozilla.org/show_bug.cgi?id=1566383).

Reviewing a probe means either to update the expiration field because we are still monitoring the data, or to remove the probe and all the related code for recording it. This discussion can happen on the bug.

## Check if third-party library should be updated

This is not a mandatory task to do on each cycle, but having up-to-date libraries can help us getting new features and most importantly bug fixes that will improve the user experience of DevTools users.

### WasmDis and WasmParser

These modules are used by the debugger to be able to parse and debug WASM sources.

Follow the [upgrade documentation](https://searchfox.org/mozilla-central/source/devtools/client/shared/vendor/WASMPARSER_UPGRADING)

### jsbeautify

This module is used by the inspector and the webconsole to pretty print user input.

Follow the [upgrade documentation](https://searchfox.org/mozilla-central/source/devtools/shared/jsbeautify/UPGRADING.md)

### CodeMirror should be updated

CodeMirror is used by our source editor component, which is used all over DevTools.

Follow the [upgrade section in the documentation](https://searchfox.org/mozilla-central/source/devtools/client/shared/sourceeditor/README)

### fluent-react

This module is used in several panels to manage localization in React applications.

Follow the [upgrade documentation](https://searchfox.org/mozilla-central/source/devtools/client/shared/vendor/FLUENT_REACT_UPGRADING)

### reselect

Follow the [upgrade documentation](https://searchfox.org/mozilla-central/source/devtools/client/shared/vendor/RESELECT_UPGRADING)

### pretty-fast

**TODO**
