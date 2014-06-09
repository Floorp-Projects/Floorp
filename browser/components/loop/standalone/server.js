/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var express = require('express');
var app = express();

// This lets /test/ be mapped to the right place for running tests
app.use(express.static(__dirname + '/../'));
// This lets /content/ be mappy right for the static contents.
app.use(express.static(__dirname + '/'));

app.listen(3000);
console.log("Serving repository root over HTTP at http://localhost:3000/");
console.log("Static contents are available at http://localhost:3000/content/");
console.log("Tests are viewable at http://localhost:3000/test/");
console.log("Use this for development only.");
