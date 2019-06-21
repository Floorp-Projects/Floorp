[//]: # (
  This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
)

# Upgrading react-prop-types

## Getting the Source

```bash
git clone git@github.com:facebook/prop-types.git
cd prop-types
```

## Building

```bash
npm install
NODE_ENV=development browserify index.js -t envify --standalone PropTypes -o react-prop-types-dev.js
NODE_ENV=production browserify index.js -t envify --standalone PropTypes -o react-prop-types.js
```

## Copying files to your Firefox repo

```bash
mv react-prop-types.js /firefox/repo/devtools/client/shared/vendor/react-prop-types.js
mv react-prop-types-dev.js /firefox/repo/devtools/client/shared/vendor/react-prop-types-dev.js
```

## Adding Version Info

Add the version to the top of `react-prop-types.js` and `react-prop-types-dev.js`.

```js
 /**
  * react-prop-types v15.6.0
  */
```
