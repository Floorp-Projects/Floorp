/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  'stability': 'experimental'
};

var { request: hostReq, response: hostRes } = require('./host');
var { defer: async } = require('../lang/functional');
var { defer } = require('../core/promise');
var { emit: emitSync, on, off } = require('../event/core');
var { uuid } = require('../util/uuid');
var emit = async(emitSync);

// Map of IDs to deferreds
var requests = new Map();

// May not be necessary to wrap this in `async`
// once promises are async via bug 881047
var receive = async(function ({data, id, error}) {
  let request = requests.get(id);
  if (request) {
    if (error) request.reject(error);
    else request.resolve(clone(data));
    requests.delete(id);
  }
});
on(hostRes, 'data', receive);

/*
 * Send is a helper to be used in client APIs to send
 * a request to host
 */
function send (eventName, data) {
  let id = uuid();
  let deferred = defer();
  requests.set(id, deferred);
  emit(hostReq, 'data', {
    id: id,
    data: clone(data),
    event: eventName
  });
  return deferred.promise;
}
exports.send = send;

/*
 * Implement internal structured cloning algorithm in the future?
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#internal-structured-cloning-algorithm
 */
function clone (obj) {
  return JSON.parse(JSON.stringify(obj || {}));
}
