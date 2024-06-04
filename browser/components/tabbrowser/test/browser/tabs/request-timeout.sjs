/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function handleRequest(request, response) {
  response.setStatusLine("1.1", 408, "Request Timeout");
}
