[//]: # (
  This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
)

Follow this steps to upgrade the `react-router-dom` library:

1. Clone the repository (note: this is a monorepository, it includes other libraries too):
   `git clone git@github.com:ReactTraining/react-router.git`

2. Install build dependencies:
   `cd react-router`
   `npm install`

3. Run a build :
   `npm run build`

4. Grab the UMD build of `react-router-dom`, which is located in `packages/react-router-dom/umd/react-router-dom.js` and copy it into Firefox source tree.

5. Edit `react-router-dom.js` and change `require('react')` for `require('resource://devtools/client/shared/vendor/react.js')`

6. Update the version below:

The current version is 4.3.1
