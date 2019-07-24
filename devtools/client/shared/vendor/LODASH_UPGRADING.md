[//]: # (
  This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
)

# Upgrading lodash

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
- remove the `template` helper if it is included. It relies on `new Function` and this is not allowed in privileged code.

See Bug 1473549 for more details.