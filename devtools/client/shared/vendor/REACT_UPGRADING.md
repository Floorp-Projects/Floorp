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
git checkout v16.4.1 # or the version you are targetting
```

## Preparing to Build

We need to disable minification and tree shaking as they overcomplicate the upgrade process without adding any benefits.

- Open scripts/rollup/build.js
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
                        "'devtools/client/shared/vendor/react'")
              .replace(/createElementNS\(['"]http:\/\/www\.w3\.org\/1999\/xhtml['"], ['"]devtools\/client\/shared\/vendor\/react['"]\)/g,
                        "createElementNS('http://www.w3.org/1999/xhtml', 'react'")
              .replace(/['"]react-dom['"]/g,
                        "'devtools/client/shared/vendor/react-dom'")
              .replace(/rendererPackageName:\s['"]devtools\/client\/shared\/vendor\/react-dom['"]/g,
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
```

From this point we will no longer need your react repository so feel free to delete it.
