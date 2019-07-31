[//]: # (
  This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
)

# Upgrading lodash

## Getting lodash

- download the full uncompressed build of lodash from https://lodash.com/
- copy the content to /devtools/client/shared/vendor/lodash.js

## Patching lodash

- open `lodash.js`
- replace the following instance of `Function('return this')`
```
  var root = freeGlobal || freeSelf || Function('return this')();
```

by

```
  var root = freeGlobal || freeSelf || globalThis;
```
- remove the `template` helper (look for `function template`). It relies on `new Function` and this is not allowed in privileged code.
- remove the `template` export (look for `lodash.template = template;`)

See Bug 1473549 for more details.