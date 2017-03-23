# Telemetry

We use telemetry to get metrics of usage of the different features and panels in DevTools. This will help us take better, informed decisions when prioritising our work.

## Adding metrics to a tool

The process to add metrics to a tool roughly consists in:

1. Adding the probe to Firefox
2. Using the probe in DevTools code
3. Getting approval from the data team

### 1. Adding the probe to Firefox

The first step involves creating entries for the probe in the file that contains declarations for all data that Firefox might report to Mozilla.

This file is at `toolkit/components/telemetry/Histograms.json`.

If it's the first time you add one of these, it's advised to follow the style of existing entries. Our entries are prepended with `DEVTOOLS_`. For example:

```javascript
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

New data types have been added over the years, so it's quite feasible that some of our probes are not the most suitable nowadays.

There's more information about types (and telemetry in general) on [this page](https://developer.mozilla.org/en-US/docs/Mozilla/Performance/Adding_a_new_Telemetry_probe) and [this other page](https://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/collection/index.html).

And of course, in case of doubt, ask!

### 2. Using the probe in DevTools code

Once the probe has been declared in the `Histograms.json` file, you'll need to actually use it in our code.

First, you need to give it an id in `devtools/client/shared/telemetry.js`. Similarly to the `Histograms.json` case, you'll want to follow the style of existing entries. For example:

```javascript
dom: {
  histogram: "DEVTOOLS_DOM_OPENED_COUNT",
  timerHistogram: "DEVTOOLS_DOM_TIME_ACTIVE_SECONDS"
},
```

... would correspond to the probes we declared in the previous section.

Then, include that module on each tool that requires telemetry:

```javascript
let Telemetry = require("devtools/client/shared/telemetry");
```

Create telemetry instance on the tool constructor:

```javascript
this._telemetry = new Telemetry();
```

And use the instance to report e.g. tool opening...

```javascript
this._telemetry.toolOpened("mytoolname");
```

... or closing:

```javascript
this._telemetry.toolClosed("mytoolname");
```

Note that `mytoolname` is the id we declared in the `telemetry.js` module.

#### Note on top level panels

The code for the tabs uses their ids to automatically report telemetry when you switch between panels, so you don't need to explicitly call `toolOpened` and `toolClosed` on top level panels.

You will still need to call those functions on subpanels, or tools such as about:debugging which are not opened as tabs.

#### Testing

The telemetry module will print warnings to stdout if there are missing ids. It is strongly advisable to ensure this is working correctly, as the module will attribute usage for undeclared ids to a generic `custom` bucket. This is not good for accurate results!

To see these warnings, you need to have the `browser.dom.window.dump.enabled` browser preference set to `true` in `about:config` (and restart the browser).

Then, try doing things that trigger telemetry calls (e.g. opening a tool). Imagine we had a typo when reporting the tool was opened:

```javascript
this._telemetry.toolOpened('mytoolnmae');
                                  ^^^^ typo, should be *mytoolname*
```

Would report an error to stdout:

```
Warning: An attempt was made to write to the mytoolnmae histogram, which is not defined in Histograms.json
```

So watch out for errors.

#### Compile it!

It's strongly recommended that you do a full Firefox build if you have edited `Histograms.json`, as it is processed at build time, and various checks will be run on it to guarantee it is valid.

```
./mach build
```

If you use `mach build faster` or artifact builds, the checks will not be performed, and your try builds will fail ("bust") when the checks are run there.

Save yourself some time and run the checks locally.

### 3. Getting approval from the data team

This is required before the changes make their way into `mozilla-central`.

To get approval, attach your patch to the bug in Bugzilla, and set two flags:

* a `feedback?` flag for bsmedberg (or someone else from the data team)
* a `needinfo?` flag to clarkbw (our product manager, so he vouches that we're using the data)

Be sure to explain very clearly what is the new probe for. E.g. "We're seeking approval for tracking opens of a new panel for debugging Web API ABCD" is much better than just asking for feedback without background info.

This review shouldn't take too long: if there's something wrong, they should tell you what to fix. If you see no signs of activity after a few days, you can ask in `#developers`.

Note that this review is *in addition* to normal colleague reviews.

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

