/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const contents = `
<div>Hello COUNTER</div>
<script>
function f() {
  console.log("First Inline Script " + COUNTER);
}
setInterval(f, 1000);
</script>
<script>
function f() {
  console.log("Second Inline Script " + COUNTER);
}
setInterval(f, 1000);
</script>
`;

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-store");
  response.setHeader("Content-Type", "text/html");

  let counter = 1 + (+getState("counter") % 4);
  setState("counter", "" + counter);

  response.write(contents.replace(/COUNTER/g, counter));
}
