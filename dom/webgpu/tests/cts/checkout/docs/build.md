# Building

Building the project is not usually needed for local development.
However, for exports to WPT, or deployment (https://gpuweb.github.io/cts/),
files can be pre-generated.

The project builds into two directories:

- `out/`: Built framework and test files, needed to run standalone or command line.
- `out-wpt/`: Build directory for export into WPT. Contains:
    - An adapter for running WebGPU CTS tests under WPT
    - A copy of the needed files from `out/`
    - A copy of any `.html` test cases from `src/`

To build and run all pre-submit checks (including type and lint checks and
unittests), use:

```sh
npm test
```

For checks only:

```sh
npm run check
```

For a quicker iterative build:

```sh
npm run standalone
```

## Run

To serve the built files (rather than using the dev server), run `npx grunt serve`.

## Export to WPT

Run `npm run wpt`.

Copy (or symlink) the `out-wpt/` directory as the `webgpu/` directory in your
WPT checkout or your browser's "internal" WPT test directory.
