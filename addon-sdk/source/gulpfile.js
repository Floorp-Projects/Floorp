/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var gulp = require('gulp');
var patch = require("./bin/node-scripts/apply-patch");
var ini = require("./bin/node-scripts/update-ini");

gulp.task('test', function(done) {
  require("./bin/jpm-test").run().then(done);
});

gulp.task('test:addons', function(done) {
  require("./bin/jpm-test").run("addons").catch(console.error).then(done);
});

gulp.task('test:docs', function(done) {
  require("./bin/jpm-test").run("docs").catch(console.error).then(done);
});

gulp.task('test:examples', function(done) {
  require("./bin/jpm-test").run("examples").catch(console.error).then(done);
});

gulp.task('test:modules', function(done) {
  require("./bin/jpm-test").run("modules").catch(console.error).then(done);
});

gulp.task('test:ini', function(done) {
  require("./bin/jpm-test").run("ini").catch(console.error).then(done);
});

gulp.task('test:firefox-bin', function(done) {
  require("./bin/jpm-test").run("firefox-bin").catch(console.error).then(done);
});

gulp.task('patch:clean', function(done) {
  patch.clean().catch(console.error).then(done);
});

gulp.task('patch:apply', function(done) {
  patch.apply().catch(console.error).then(done);
});
