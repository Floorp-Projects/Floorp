/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental",
  "engines": {
    "Firefox": "*",
    "SeaMonkey": "*"
  }
};

const { Cc, Ci } = require('chrome');
const taggingService = Cc["@mozilla.org/browser/tagging-service;1"].
                       getService(Ci.nsITaggingService);
const ios = Cc['@mozilla.org/network/io-service;1'].
            getService(Ci.nsIIOService);
const { URL } = require('../../url');
const { newURI } = require('../../url/utils');
const { request, response } = require('../../addon/host');
const { on, emit } = require('../../event/core');
const { filter } = require('../../event/utils');

const EVENT_MAP = {
  'sdk-places-tags-tag': tag,
  'sdk-places-tags-untag': untag,
  'sdk-places-tags-get-tags-by-url': getTagsByURL,
  'sdk-places-tags-get-urls-by-tag': getURLsByTag
};

function tag (message) {
  let data = message.data;
  let resData = {
    id: message.id,
    event: message.event
  };

  if (data.tags && data.tags.length > 0)
    resData.data = taggingService.tagURI(newURI(data.url), data.tags);
  respond(resData);
}

function untag (message) {
  let data = message.data;
  let resData = {
    id: message.id,
    event: message.event
  };

  if (!data.tags || data.tags.length > 0)
    resData.data = taggingService.untagURI(newURI(data.url), data.tags);
  respond(resData);
}

function getURLsByTag (message) {
  let data = message.data;
  let resData = {
    id: message.id,
    event: message.event
  };

  resData.data = taggingService
    .getURIsForTag(data.tag).map(uri => uri.spec);
  respond(resData);
}

function getTagsByURL (message) {
  let data = message.data;
  let resData = {
    id: message.id,
    event: message.event
  };

  resData.data = taggingService.getTagsForURI(newURI(data.url), {});
  respond(resData);
}

/*
 * Hook into host
 */

var reqStream = filter(request, function (data) {
  return /sdk-places-tags/.test(data.event);
});

on(reqStream, 'data', function (e) {
  if (EVENT_MAP[e.event]) EVENT_MAP[e.event](e);
});

function respond (data) {
  emit(response, 'data', data);
}
