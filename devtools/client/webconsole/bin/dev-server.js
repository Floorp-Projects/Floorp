/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

"use strict";

const toolbox = require("devtools-launchpad/index");
const feature = require("devtools-config");
const { getConfig } = require("./configure");

const envConfig = getConfig();

feature.setConfig(envConfig);

let webpackConfig = require("../webpack.config");

toolbox.startDevServer(envConfig, webpackConfig, __dirname);
