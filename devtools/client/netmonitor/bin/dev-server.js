/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

"use strict";

const path = require("path");

const toolbox = require("devtools-launchpad/index");
const feature = require("devtools-config");
const express = require("express");
const { getConfig } = require("./configure");

const envConfig = getConfig();

feature.setConfig(envConfig);

let webpackConfig = require("../webpack.config");

let { app } = toolbox.startDevServer(envConfig, webpackConfig, __dirname);

app.use(
  "/integration/examples",
  express.static("src/test/mochitest/examples")
);

app.get("/integration", function (req, res) {
  res.sendFile(path.join(__dirname, "../src/test/integration/index.html"));
});
