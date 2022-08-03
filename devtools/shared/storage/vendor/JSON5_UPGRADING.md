[//]: # (
  This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
)

# Upgrading json5

## Getting the Source

```bash
git clone https://github.com/json5/json5
cd json5
git checkout v2.2.1 # checkout the right version tag
```

## Building

```bash
npm install
npm run build
cp dist/index.js <gecko-dev>/devtools/shared/storage/vendor/json5.js
```

## Patching json5

- open `json5.js`
- Add the version number to the top of the file:
  ```
  /**
   * json5 v2.2.1
   */
  ```
- Replace instances of `Function('return this')()` with `globalThis`. See Bug 1473549.

## Update the version:

The current version is 2.2.1. Update this version number everywhere in this file.
