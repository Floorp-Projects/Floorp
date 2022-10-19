[//]: # (
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
)

# Upgrading React

## Introduction

We use a version of React that has a few minor tweaks. We want to use an un-minified production version anyway so you need to build React yourself.

## First, Upgrade react-prop-types.js

You should start by upgrading our prop-types library to match the latest version of React. Please follow the instructions in `devtools/client/shared/vendor/REACT_PROP_TYPES_UPGRADING.md` before continuing.

## Getting the Source

```bash
git clone https://github.com/facebook/react.git
cd react
git checkout v16.8.6 # or the version you are targetting
```

## Preparing to Build

We need to disable minification and tree shaking as they overcomplicate the upgrade process without adding any benefits.

- Open `scripts/rollup/build.js`
- Find a method called `function getRollupOutputOptions()`
- After `sourcemap: false` add `treeshake: false` and `freeze: false`
- Remove `freeze: !isProduction,` from the same section.
- Change this:

  ```js
  // Apply dead code elimination and/or minification.
  isProduction &&
  ```

  To this:

  ```js
  {
    transformBundle(source) {
      return (
        source.replace(/['"]react['"]/g,
                        "'resource://devtools/client/shared/vendor/react.js'")
              .replace(/createElementNS\(['"]http:\/\/www\.w3\.org\/1999\/xhtml['"], ['"]resource://devtools\/client\/shared\/vendor\/react.js['"]\)/g,
                        "createElementNS('http://www.w3.org/1999/xhtml', 'react'")
              .replace(/['"]react-dom['"]/g,
                        "'resource://devtools/client/shared/vendor/react-dom.js'")
              .replace(/rendererPackageName:\s['"]resource://devtools\/client\/shared\/vendor\/react-dom.js['"]/g,
                        "rendererPackageName: 'react-dom'")
              .replace(/ocument\.createElement\(/g,
                        "ocument.createElementNS('http://www.w3.org/1999/xhtml', ")
      );
    },
  },
  // Apply dead code elimination and/or minification.
  false &&
  ```

  - Find `await createBundle` and remove all bundles in that block except for `UMD_DEV`, `UMD_PROD` and `NODE_DEV`.

## Building

```bash
npm install --global yarn
yarn
yarn build
```

### Copy the Files Into your Firefox Repo

```bash
cd <react repo root>
cp build/dist/react.production.min.js <gecko-dev>/devtools/client/shared/vendor/react.js
cp build/dist/react-dom.production.min.js <gecko-dev>/devtools/client/shared/vendor/react-dom.js
cp build/dist/react-dom-server.browser.production.min.js <gecko-dev>/devtools/client/shared/vendor/react-dom-server.js
cp build/dist/react-dom-test-utils.production.min.js <gecko-dev>/devtools/client/shared/vendor/react-dom-test-utils.js
cp build/dist/react.development.js <gecko-dev>/devtools/client/shared/vendor/react-dev.js
cp build/dist/react-dom.development.js <gecko-dev>/devtools/client/shared/vendor/react-dom-dev.js
cp build/dist/react-dom-server.browser.development.js <gecko-dev>/devtools/client/shared/vendor/react-dom-server-dev.js
cp build/dist/react-dom-test-utils.development.js <gecko-dev>/devtools/client/shared/vendor/react-dom-test-utils-dev.js
cp build/dist/react-test-renderer-shallow.production.min.js <gecko-dev>/devtools/client/shared/vendor/react-test-renderer-shallow.js
cp build/dist/react-test-renderer.production.min.js <gecko-dev>/devtools/client/shared/vendor/react-test-renderer.js
```

From this point we will no longer need your react repository so feel free to delete it.

## Debugger

### Update React

- Open `devtools/client/debugger/package.json`
- Under `dependencies` update `react` and `react-dom` to the required version.
- Under `devDependencies` you may also need to update `enzyme`, `enzyme-adapter-react-16` and `enzyme-to-json` to versions compatible with the new react version.

### Build the debugger

#### Check your .mozconfig

- Ensure you are not in debug mode (`ac_add_options --disable-debug`).
- Ensure you are not using the debug version of react (`ac_add_options --disable-debug-js-modules`).

#### First build Firefox

```bash
cd <srcdir> # where sourcedir is the root of your Firefox repo.
./mach build
```

#### Now update the debugger source

```bash
# Go to the debugger folder.
cd devtools/client/debugger

# Remove all node_modules folders.
find . -name "node_modules" -exec rm -rf '{}' +

# Install the new react and enzyme modules.
yarn
```

### Run the debugger tests

#### First run locally

```bash
node bin/try-runner.js
```

If there any failures fix them.

**NOTE: If there are any jest failures you will get better output by running the jest tests directly using:**

```bash
yarn test
```

If any tests fail then fix them.

#### Commit your changes

Use `hg commit` or `hg amend` to commit your changes.

#### Push to try

Just because the tests run fine locally they may still fail on try. You should first ensure that `node bin/try-runner.js` passes on try:

```bash
cd <srcdir> # where sourcedir is the root of your Firefox repo.
`./mach try fuzzy`
```

- When the interface appears type `debugger`.
- Press `<enter>`.

Once these tests pass on try then push to try as normal e.g. `./mach try -b do -p all -u all -t all -e all`.

If try passes then go celebrate otherwise you are on your own.
