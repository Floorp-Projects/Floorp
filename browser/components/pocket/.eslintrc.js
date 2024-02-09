/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

module.exports = {
  plugins: [
    "react", // require("eslint-plugin-react")
  ],
  settings: {
    react: {
      version: "17.0.2",
    },
  },
  rules: {
    "react/jsx-uses-react": 2,
    "react/jsx-uses-vars": 2,
  },
};
