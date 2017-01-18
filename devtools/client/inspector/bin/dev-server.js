/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global __dirname */

"use strict";

const toolbox = require("../node_modules/devtools-launchpad/index");
const feature = require("devtools-config");
const envConfig = require("../configs/development.json");

const fs = require("fs");
const path = require("path");

feature.setConfig(envConfig);
const webpackConfig = require("../webpack.config")(envConfig);

let {app} = toolbox.startDevServer(envConfig, webpackConfig);

function sendFile(res, src, encoding) {
  const filePath = path.join(__dirname, src);
  const file = encoding ? fs.readFileSync(filePath, encoding) : fs.readFileSync(filePath);
  res.send(file);
}

function addFileRoute(from, to) {
  app.get(from, function (req, res) {
    sendFile(res, to, "utf-8");
  });
}

// Routes
addFileRoute("/", "../inspector.xhtml");
addFileRoute("/markup/markup.xhtml", "../markup/markup.xhtml");

app.get("/devtools/skin/images/:file.png", function (req, res) {
  res.contentType("image/png");
  sendFile(res, "../../themes/images/" + req.params.file + ".png");
});

app.get("/devtools/skin/images/:file.svg", function (req, res) {
  res.contentType("image/svg+xml");
  sendFile(res, "../../themes/images/" + req.params.file + ".svg", "utf-8");
});

app.get("/images/:file.svg", function (req, res) {
  res.contentType("image/svg+xml");
  sendFile(res, "../../themes/images/" + req.params.file + ".svg", "utf-8");
});

// Redirect chrome:devtools/skin/file.css to ../../themes/file.css
app.get("/devtools/skin/:file.css", function (req, res) {
  res.contentType("text/css; charset=utf-8");
  sendFile(res, "../../themes/" + req.params.file + ".css", "utf-8");
});

// Redirect chrome:devtools/client/path/to/file.css to ../../path/to/file.css
//      and chrome:devtools/content/path/to/file.css to ../../path/to/file.css
app.get(/^\/devtools\/(?:client|content)\/(.*)\.css$/, function (req, res) {
  res.contentType("text/css; charset=utf-8");
  sendFile(res, "../../" + req.params[0] + ".css");
});
