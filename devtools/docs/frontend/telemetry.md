# Telemetry

We use telemetry to get metrics of usage of the different features and panels in DevTools. This will help us take better, informed decisions when prioritising our work.

## Adding metrics to a tool

The process to add metrics to a tool roughly consists in:

1. Adding the probe to Firefox
2. Using Histograms.json probes in DevTools code
3. Using Scalars.yaml probes in DevTools code
4. Using Events.yaml probes in DevTools code for analysis in Amplitude.
5. Getting approval from the data team

### 1. Adding the probe to Firefox

The first step involves creating entries for the probe in one of the files that contain declarations for all data that Firefox might report to Mozilla.

These files are:

- `toolkit/components/telemetry/Histograms.json`
- `toolkit/components/telemetry/Scalars.yaml`
- `toolkit/components/telemetry/Events.yaml`

Scalars allow collection of simple values, like counts, booleans and strings and are to be used whenever possible instead of histograms.

Histograms allow collection of multiple different values, but aggregate them into a number of buckets. Each bucket has a value range and a count of how many values we recorded.

Events allow collection of a number of properties keyed to a category, method, object and value. Event telemetry helps us tell a story about how a user is interacting with the browser.

Both scalars & histograms allow recording by keys. This allows for more flexible, two-level data collection.

#### The different file formats

The data team chose YAML for `Scalars.yaml` and `Events.yaml` because it is easy to write and provides a number of features not available in JSON including comments, extensible data types, relational anchors, strings without quotation marks, and mapping types preserving key order.

While we previously used JSON for similar purposes in histograms.json, we have used YAML here because it allows for comments and is generally easier to write.

The data team are considering moving the histograms over to YAML format at some point.

If it's the first time you add one of these, it's advised to follow the style of existing entries.

New data types have been added over the years, so it's quite feasible that some of our probes are not the most suitable nowadays.

There's more information about types (and telemetry in general) on [this page](https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/start/adding-a-new-probe.html) and [this other page](https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/collection/index.html).

And of course, in case of doubt, ask!

### Adding probes to `Histograms.json`

Our entries are prefixed with `DEVTOOLS_`. For example:

```json
  "DEVTOOLS_DOM_OPENED_COUNT": {
    "alert_emails": ["dev-developer-tools@lists.mozilla.org"],
    "expires_in_version": "never",
    "kind": "count",
    "bug_numbers": [1343501],
    "description": "Number of times the DevTools DOM Inspector has been opened.",
    "releaseChannelCollection": "opt-out"
  },
  "DEVTOOLS_DOM_TIME_ACTIVE_SECONDS": {
    "alert_emails": ["dev-developer-tools@lists.mozilla.org"],
    "expires_in_version": "never",
    "kind": "exponential",
    "bug_numbers": [1343501],
    "high": 10000000,
    "n_buckets": 100,
    "description": "How long has the DOM inspector been active (seconds)"
  },
```

There are different types of probes you can use. These are specified by the `kind` field. Normally we use `count` for counting how many times the tools are opened, and `exponential` for how many times a panel is active.

### Adding probes to `Scalars.yaml`

Our entries are prefixed with `devtools.`. For example:

```yaml
devtools.toolbar.eyedropper:
  opened:
    bug_numbers:
      - 1247985
      - 1352115
    description: Number of times the DevTools Eyedropper has been opened via the inspector toolbar.
    expires: never
    kind: uint
    notification_emails:
      - dev-developer-tools@lists.mozilla.org
    release_channel_collection: opt-out
    record_in_processes:
      - 'main'

devtools.copy.unique.css.selector:
  opened:
    bug_numbers:
      - 1323700
      - 1352115
    description: Number of times the DevTools copy unique CSS selector has been used.
    expires: "57"
    kind: uint
    notification_emails:
      - dev-developer-tools@lists.mozilla.org
    release_channel_collection: opt-out
    record_in_processes:
      - 'main'
```

### Adding probes to `Events.yaml`

Our entries are prefixed with `devtools.`. For example:

```yaml
devtools.main:
  open:
    objects: ["tools"]
    bug_numbers: [1416024]
    notification_emails: ["dev-developer-tools@lists.mozilla.org", "hkirschner@mozilla.com"]
    record_in_processes: ["main"]
    description: User opens devtools toolbox.
    release_channel_collection: opt-out
    expiry_version: never
    extra_keys:
      entrypoint: How was the toolbox opened? CommandLine, ContextMenu, DeveloperToolbar, HamburgerMenu, KeyShortcut, SessionRestore or SystemMenu
      first_panel: The name of the first panel opened.
      host: "Toolbox host (positioning): bottom, side, window or other."
      splitconsole: Indicates whether the split console was open.
      width: Toolbox width (px).
```

### 2. Using Histograms.json probes in DevTools code

Once the probe has been declared in the `Histograms.json` file, you'll need to actually use it in our code.

First, you need to give it an id in `devtools/client/shared/telemetry.js`. Similarly to the `Histograms.json` case, you'll want to follow the style of existing entries. For example:

```js
dom: {
  histogram: "DEVTOOLS_DOM_OPENED_COUNT",
  timerHistogram: "DEVTOOLS_DOM_TIME_ACTIVE_SECONDS"
},
```

... would correspond to the probes we declared in the previous section.

Then, include that module on each tool that requires telemetry:

```js
let Telemetry = require("devtools/client/shared/telemetry");
```

Create a telemetry instance on the tool constructor:

```js
this._telemetry = new Telemetry();
```

And use the instance to report e.g. tool opening...

```js
this._telemetry.toolOpened("mytoolname");
```

... or closing:

```js
this._telemetry.toolClosed("mytoolname");
```

Note that `mytoolname` is the id we declared in the `telemetry.js` module.

### 3. Using Scalars.yaml probes in DevTools code

Once the probe has been declared in the `Scalars.yaml` file, you'll need to actually use it in our code.

First, you need to give it an id in `devtools/client/shared/telemetry.js`. You will want to follow the style of existing lowercase histogram entries. For example:

```js
toolbareyedropper: {
  scalar: "devtools.toolbar.eyedropper.opened", // Note that the scalar is lowercase
},
copyuniquecssselector: {
  scalar: "devtools.copy.unique.css.selector.opened",
},
```

... would correspond to the probes we declared in the previous section.

Then, include that module on each tool that requires telemetry:

```js
let Telemetry = require("devtools/client/shared/telemetry");
```

Create a telemetry instance on the tool constructor:

```js
this._telemetry = new Telemetry();
```

And use the instance to report e.g. tool opening...

```js
this._telemetry.toolOpened("mytoolname");
```

Notes:

- `mytoolname` is the id we declared in the `Scalars.yaml` module.
- Because we are not logging tool's time opened in `Scalars.yaml` we don't care
  about toolClosed. Of course, if there was an accompanying `timerHistogram`
  field defined in `telemetry.js` and `histograms.json` then `toolClosed` should
  also be added.

### 4. Using Events.yaml probes in DevTools code

Once the probe has been declared in the `Events.yaml` file, you'll need to actually use it in our code.

It is crucial to understand that event telemetry have a string identifier which is constructed from the `category`, `method`, `object` (name) and `value` on which the event occured. This key points to an "extra" object that contains further information about the event (we will give examples later in this section).

Because these "extra" objects can be from completely independant code paths we
can send events and leave them in a pending state until all of the expected extra properties have been received.

First, include the telemetry module in each tool that requires telemetry:

```js
let Telemetry = require("devtools/client/shared/telemetry");
```

Create a telemetry instance on the tool constructor:

```js
this._telemetry = new Telemetry();
```

And use the instance to report e.g. tool opening...

```js
// Event telemetry is disabled by default so enable it for your category.
this._telemetry.setEventRecordingEnabled("devtools.main", true);

// If you already have all the properties for the event you can send the
// telemetry event using:
// this._telemetry.recordEvent(category, method, object, value, extra) e.g.
this._telemetry.recordEvent("devtools.main", "open", "tools", null, {
  "entrypoint": "ContextMenu",
  "first_panel": "Inspector",
  "host": "bottom",
  "splitconsole": false,
  "width": 1024,
  "session_id": this.toolbox.sessionId
});

// If your "extra" properties are in different code paths you will need to
// create a "pending event." These events contain a list of expected properties
// that can be populated before or after creating the pending event.

// Use the category, method, object, value combinations above to add a
// property... we do this before creating the pending event simply to
// demonstrate that properties can be sent before the pending event is created.
this._telemetry.addEventProperty(
  "devtools.main", "open", "tools", null, "entrypoint", "ContextMenu");

// In this example `"devtools.main", "open", "tools", null` make up the
// signature of the event and needs to be sent with all properties.

// Create the pending event using
// this._telemetry.preparePendingEvent(category, method, object, value,
// expectedPropertyNames) e.g.
this._telemetry.preparePendingEvent("devtools.main", "open", "tools", null,
  ["entrypoint", "first_panel", "host", "splitconsole", "width", "session_id"]
);

// Use the category, method, object, value combinations above to add each
// property.
this._telemetry.addEventProperty(
  "devtools.main", "open", "tools", null, "first_panel", "inspector");
this._telemetry.addEventProperty(
  "devtools.main", "open", "tools", null, "host", "bottom");
this._telemetry.addEventProperty(
  "devtools.main", "open", "tools", null, "splitconsole", false);
this._telemetry.addEventProperty(
  "devtools.main", "open", "tools", null, "width", 1024);

// You can also add properties in batches using e.g.:
this._telemetry.addEventProperties("devtools.main", "open", "tools", null, {
  "first_panel": "inspector",
  "host": "bottom",
  "splitconsole": false,
  "width": 1024
});

```

Notes:

- `mytoolname` is the id we declared in the `Scalars.yaml` module.
- Because we are not logging tool's time opened in `Scalars.yaml` we don't care
  about toolClosed. Of course, if there was an accompanying `timerHistogram`
  field defined in `telemetry.js` and `histograms.json` then `toolClosed` should
  also be added.

#### Note on top level panels

The code for the tabs uses their ids to automatically report telemetry when you switch between panels, so you don't need to explicitly call `toolOpened` and `toolClosed` on top level panels.

You will still need to call those functions on subpanels, or tools such as about:debugging which are not opened as tabs.

#### Testing

The telemetry module will print warnings to stdout if there are missing ids. It is strongly advisable to ensure this is working correctly, as the module will attribute usage for undeclared ids to a generic `custom` bucket. This is not good for accurate results!

To see these warnings, you need to have the `browser.dom.window.dump.enabled` browser preference set to `true` in `about:config` (and restart the browser).

Then, try doing things that trigger telemetry calls (e.g. opening a tool). Imagine we had a typo when reporting the tool was opened:

```js
this._telemetry.toolOpened('mytoolnmae');
                                  ^^^^ typo, should be *mytoolname*
```

Would report an error to stdout:

```text
Warning: An attempt was made to write to the mytoolnmae histogram, which is not defined in Histograms.json
```

So watch out for errors.

#### Testing Event Telemetry

This is best shown via an example:

```js
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Toolbox } = require("devtools/client/framework/toolbox");

const URL = "data:text/html;charset=utf8,browser_toolbox_telemetry_close.js";
const OPTOUT = Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT;
const { SIDE, BOTTOM } = Toolbox.HostType;
const DATA = [
  {
    timestamp: null,
    category: "devtools.main",
    method: "close",
    object: "tools",
    value: null,
    extra: {
      host: "right",
      width: "1440"
    }
  },
  {
    timestamp: null,
    category: "devtools.main",
    method: "close",
    object: "tools",
    value: null,
    extra: {
      host: "bottom",
      width: "1440"
    }
  }
];

add_task(async function() {
  // Let's reset the counts.
  Services.telemetry.clearEvents();

  // Ensure no events have been logged
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);
  ok(!snapshot.parent, "No events have been logged for the main process");

  await openAndCloseToolbox("webconsole", SIDE);
  await openAndCloseToolbox("webconsole", BOTTOM);

  checkResults();
});

async function openAndCloseToolbox(toolId, host) {
  const tab = await addTab(URL);
  const target = TargetFactory.forTab(tab);
  const toolbox = await gDevTools.showToolbox(target, toolId);

  await toolbox.switchHost(host);
  await toolbox.destroy();
}

function checkResults() {
  const snapshot = Services.telemetry.snapshotEvents(OPTOUT, true);
  const events = snapshot.parent.filter(event => event[1] === "devtools.main" &&
                                                 event[2] === "close" &&
                                                 event[3] === "tools" &&
                                                 event[4] === null
  );

  for (let i in DATA) {
    const [ timestamp, category, method, object, value, extra ] = events[i];
    const expected = DATA[i];

    // ignore timestamp
    ok(timestamp > 0, "timestamp is greater than 0");
    is(category, expected.category, "category is correct");
    is(method, expected.method, "method is correct");
    is(object, expected.object, "object is correct");
    is(value, expected.value, "value is correct");

    is(extra.host, expected.extra.host, "host is correct");
    ok(extra.width > 0, "width is greater than 0");
  }
}
```

#### Compile it

You need to do a full Firefox build if you have edited either `Histograms.json` or `Events.yaml`, as they are processed at build time, and various checks will be run on them to guarantee they are valid.

```bash
./mach build
```

If you use `mach build faster` or artifact builds, the checks will not be performed, and your try builds will fail ("bust") when the checks are run there.

Save yourself some time and run the checks locally.

NOTE: Changes to `Scalars.yaml` *are* processed when doing an artifact build.

### 4. Getting approval from the data team

This is required before the changes make their way into `mozilla-central`.

To get approval, attach your patch to the bug in Bugzilla, and set two flags:

- a `review?` flag for a data steward.
- a `needinfo?` flag to hkirschner (our product manager, so he vouches that we're using the data)

Be sure to explain very clearly what is the new probe for. E.g. "We're seeking approval for tracking opens of a new panel for debugging Web API ABCD" is much better than just asking for feedback without background info.

This review shouldn't take too long: if there's something wrong, they should tell you what to fix. If you see no signs of activity after a few days, you can ask in `#developers`.

Note that this review is *in addition* to normal colleague reviews.

Click [here](https://wiki.mozilla.org/Firefox/Data_Collection#Requesting_Data_Collection) for more details.

## Accessing existing data

### Local data

Go to [about:telemetry](about:telemetry) to see stats relating to your local instance.

### Global data

Data aggregated from large groups of Firefox users is available at [telemetry.mozilla.org](https://telemetry.mozilla.org).

Reports are written with SQL. For example, here's one comparing [usage of some DevTools panels](https://sql.telemetry.mozilla.org/queries/1000#table).

If you want to get better understanding of how people are using the tools, you are encouraged to explore this data set by writing your own reports.

The easiest way to get started is to *fork* an existing report and modify it to get used to the syntax, as SQL for massive data tables is very different from SQL for a humble blog engine, and you'll find some new operators that might look unfamiliar.

It's also recommended to take small steps and run the queries often to detect errors before they're too complicated to solve, particularly if you're not experienced with this (yet).

Slow queries will be interrupted by the system, so don't worry about "fetching too much data" or "using too many resources". There's built-in protection to avoid your code eating up the Telemetry database.

Funnily, if you're based in Europe, you might be in luck, as the website tends to be more responsive during European working hours than it is at Pacific working hours, as seemingly there's less people in Europe interacting with it.
