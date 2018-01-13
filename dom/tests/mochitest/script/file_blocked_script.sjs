/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function setGlobalState(data, key)
{
  x = { data: data, QueryInterface: function(iid) { return this } };
  x.wrappedJSObject = x;
  setObjectState(key, x);
}

function getGlobalState(key)
{
  var data;
  getObjectState(key, function(x) {
    data = x && x.wrappedJSObject.data;
  });
  return data;
}

function finishBlockedRequest(request, response, query)
{
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "application/javascript", false);
  response.write("scriptLoaded('" + query[1] + "');");
  response.finish();

  setGlobalState(undefined, query[1]);
}

function handleRequest(request, response)
{
  var query = request.queryString.split('&');
  switch (query[0]) {
    case "blocked":
      var alreadyUnblocked = getGlobalState(query[1]);

      response.processAsync();
      if (alreadyUnblocked === true) {
        // the unblock request came before the blocked request, just go on and finish synchronously
        finishBlockedRequest(request, response, query);
      } else {
        setGlobalState(response, query[1]);
      }
      break;

    case "unblock":
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Cache-Control", "no-cache", false);
      response.setHeader("Content-Type", "image/png", false);
      response.write("\x89PNG"); // just a broken image is enough for our purpose

      var blockedResponse = getGlobalState(query[1]);
      if (blockedResponse === undefined) {
        // the unblock request came before the blocked request, remember to not block it
        setGlobalState(true, query[1]);
      } else if (typeof blockedResponse == "object") {
        finishBlockedRequest(request, blockedResponse, query);
      }
      break;

    default:
      response.setStatusLine(request.httpVersion, 400, "Bad request");
      break;
  }
}
