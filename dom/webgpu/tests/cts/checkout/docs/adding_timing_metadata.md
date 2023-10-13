## Problem

When adding new tests to the CTS you may occasionally see an error like this
when running `npm test` or `npm run standalone`

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
approximately same sized chunks.

If a value has been defaulted to 0 by someone, you will see warnings like this
```
...
WARNING: subcaseMS≤0 found in listing_meta.json (allowed, but try to avoid):
  webgpu:shader,execution,expression,binary,af_matrix_addition:matrix:*
...
```

These messages should be resolved by adding appropriate entries to the JSON
file.

## Solution

There exists tooling in the CTS repo for generating appropriate estimates for
these values, though they do require some manual intervention. The rest of this
doc will be a walkthrough of running these tools.

### Default Value

The first step is to add a default value for entry to 
`src/webgpu/listing_meta.json`, since there is a chicken-and-egg problem for 
updating these values.

```
  "webgpu:shader,execution,expression,binary,af_matrix_addition:matrix:*": { "subcaseMS": 0 },
```

(It should have a value of 0, since later tooling updates the value if the newer
value is higher)

### Websocket Logger

The first tool that needs to be run is `websocket-logger`, which uses a side
channel from WPT to report timing data when CTS is run via a websocket. This
should be run in a separate process/terminal, since it needs to stay running
throughout the following steps.

At `tools/websocket-logger/`
```
npm ci
npm start
```

The output from this command will indicate where the results are being logged,
which will be needed later
```
...
Writing to wslog-2023-09-11T18-57-34.txt
...
```

### Running CTS

Now we need to run the specific cases in CTS, which requires serving the CTS 
locally.

At project root
```
npm run standalone
npm start
```

Once this is started you can then direct a WebGPU enabled browser to the
specific CTS entry and run the tests, for example
```
http://127.0.0.1:8080/standalone/q?webgpu:shader,execution,expression,binary,af_matrix_addition:matrix:*
```

### Merging metadata

The final step is to merge the new data that has been captured into the JSON
file.

This can be done using the following command
```
tools/merge_listing_times webgpu -- tools/websocket-logger/wslog-2023-09-11T18-57-34.txt
```

where the text file is the result file from websocket-logger.

Now you just need to commit the pending diff in your repo.
