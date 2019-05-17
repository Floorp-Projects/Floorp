/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

module.exports = {
  title: "Reps",
  hotReloading: true,
  defaultURL: "https://nchevobbe.github.io/demo/console-test-app.html",
  environment: "development",
  theme: "light",
  firefox: {
    webSocketConnection: false,
    host: "localhost",
    webSocketPort: 9000,
    tcpPort: 6080,
  },
  development: {
    serverPort: 8000,
  },
};
