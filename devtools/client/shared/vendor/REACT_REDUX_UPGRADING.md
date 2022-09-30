[//]: # (
  This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
)

# Upgrading react-redux

## Getting the Source

```bash
git clone https://github.com/reactjs/react-redux
cd react-redux
git checkout v5.0.7 # checkout the right version tag
```

## Building

```bash
npm install
npm run build:umd
cp dist/react-redux.js <gecko-dev>/devtools/client/shared/vendor/react-redux.js
```

We no longer need the react-redux repo so feel free to delete it.

## Patching react-redux

- open `react-redux.js`
- Add the version number to the top of the file:
  ```
  /**
   * react-redux v5.0.7
   */
  ```
- Replace all instances of `'react'` with `'resource://devtools/client/shared/vendor/react.js'` (including the quotes).
- Replace all instances of `'redux'` with `'resource://devtools/client/shared/vendor/redux.js'` (including the quotes).
- Replace all instances of `Function('return this')()` with `globalThis`. See Bug 1473549.
