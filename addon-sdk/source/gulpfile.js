/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var gulp = require('gulp');

gulp.task('test', function(done) {
  require("./bin/jpm-test").run().then(done);
});

gulp.task('test:addons', function(done) {
  require("./bin/jpm-test").run("addons").then(done);
});

gulp.task('test:examples', function(done) {
  require("./bin/jpm-test").run("examples").then(done);
});

gulp.task('test:modules', function(done) {
  require("./bin/jpm-test").run("modules").then(done);
});

