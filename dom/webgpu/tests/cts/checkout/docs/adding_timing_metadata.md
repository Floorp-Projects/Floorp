# Adding Timing Metadata

## listing_meta.json files

`listing_meta.json` files are SEMI AUTO-GENERATED.

The raw data may be edited manually, to add entries or change timing values.

The **list** of tests must stay up to date, so it can be used by external
tools. This is verified by presubmit checks.

The `subcaseMS` values are estimates. They can be set to 0 if for some reason
you can't estimate the time (or there's an existing test with a long name and
slow subcases that would result in query strings that are too long), but this
will produce a non-fatal warning. Avoid creating new warnings whenever
possible. Any existing failures should be fixed (eventually).

### Performance

Note this data is typically captured by developers using higher-end
computers, so typical test machines might execute more slowly. For this
reason, the WPT chunking should be configured to generate chunks much shorter
than 5 seconds (a typical default time limit in WPT test executors) so they
should still execute in under 5 seconds on lower-end computers.

## Problem

When adding new tests to the CTS you may occasionally see an error like this
when running `npm test` or `npm run standalone`:

```
ERROR: Tests missing from listing_meta.json. Please add the new tests (set subcaseMS to 0 if you cannot estimate it):
  webgpu:shader,execution,expression,binary,af_matrix_addition:matrix:*

/home/runner/work/cts/cts/src/common/util/util.ts:38
    throw new Error(msg && (typeof msg === 'string' ? msg : msg()));
          ^
Error:
    at assert (/home/runner/work/cts/cts/src/common/util/util.ts:38:11)
    at crawl (/home/runner/work/cts/cts/src/common/tools/crawl.ts:155:11)
Warning: non-zero exit code 1
 Use --force to continue.

Aborted due to warnings.
```

What this error message is trying to tell us, is that there is no entry for
`webgpu:shader,execution,expression,binary,af_matrix_addition:matrix:*` in
`src/webgpu/listing_meta.json`.

These entries are estimates for the amount of time that subcases take to run,
and are used as inputs into the WPT tooling to attempt to portion out tests into
approximately same-sized chunks.

If a value has been defaulted to 0 by someone, you will see warnings like this:

```
...
WARNING: subcaseMS≤0 found in listing_meta.json (allowed, but try to avoid):
  webgpu:shader,execution,expression,binary,af_matrix_addition:matrix:*
...
```

These messages should be resolved by adding appropriate entries to the JSON
file.

## Solution 1 (manual, best for simple tests)

If you're developing new tests and need to update this file, it is sometimes
easiest to do so manually. Run your tests under your usual development workflow
and see how long they take. In the standalone web runner `npm start`, the total
time for a test case is reported on the right-hand side when the case logs are
expanded.

Record the average time per *subcase* across all cases of the test (you may need
to compute this) into the `listing_meta.json` file.

## Solution 2 (semi-automated)

There exists tooling in the CTS repo for generating appropriate estimates for
these values, though they do require some manual intervention. The rest of this
doc will be a walkthrough of running these tools.

Timing data can be captured in bulk and "merged" into this file using
the `merge_listing_times` tool. This is useful when a large number of tests
change or otherwise a lot of tests need to be updated, but it also automates the
manual steps above.

The tool can also be used without any inputs to reformat `listing_meta.json`.
Please read the help message of `merge_listing_times` for more information.

### Placeholder Value

If your development workflow requires a clean build, the first step is to add a
placeholder value for entry to `src/webgpu/listing_meta.json`, since there is a
chicken-and-egg problem for updating these values.

```
  "webgpu:shader,execution,expression,binary,af_matrix_addition:matrix:*": { "subcaseMS": 0 },
```

(It should have a value of 0, since later tooling updates the value if the newer
value is higher.)

### Websocket Logger

The first tool that needs to be run is `websocket-logger`, which receives data
on a WebSocket channel to capture timing data when CTS is run. This
should be run in a separate process/terminal, since it needs to stay running
throughout the following steps.

In the `tools/websocket-logger/` directory:

```
npm ci
npm start
```

The output from this command will indicate where the results are being logged,
which will be needed later. For example:

```
...
Writing to wslog-2023-09-12T18-57-34.txt
...
```

### Running CTS

Now we need to run the specific cases in CTS that we need to time.
This should be possible under any development workflow (as long as its runtime environment, like Node, supports WebSockets), but the most well-tested way is using the standalone web runner.

This requires serving the CTS locally. In the project root:

```
npm run standalone
npm start
```

Once this is started you can then direct a WebGPU enabled browser to the
specific CTS entry and run the tests, for example:

```
http://localhost:8080/standalone/?q=webgpu:shader,execution,expression,binary,af_matrix_addition:matrix:*
```

If the tests have a high variance in runtime, you can run them multiple times.
The longest recorded time will be used.

### Merging metadata

The final step is to merge the new data that has been captured into the JSON
file.

This can be done using the following command:

```
tools/merge_listing_times webgpu -- tools/websocket-logger/wslog-2023-09-12T18-57-34.txt
```

where the text file is the result file from websocket-logger.

Now you just need to commit the pending diff in your repo.
