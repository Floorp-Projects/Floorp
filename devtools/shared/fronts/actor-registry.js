/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { components } = require("chrome");
const Services = require("Services");
const { actorActorSpec, actorRegistrySpec } = require("devtools/shared/specs/actor-registry");
const protocol = require("devtools/shared/protocol");
const { custom } = protocol;

loader.lazyImporter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");

const ActorActorFront = protocol.FrontClassWithSpec(actorActorSpec, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  }
});

exports.ActorActorFront = ActorActorFront;

function request(uri) {
  return new Promise((resolve, reject) => {
    try {
      uri = Services.io.newURI(uri);
    } catch (e) {
      reject(e);
    }

    NetUtil.asyncFetch({
      uri,
      loadUsingSystemPrincipal: true,
    }, (stream, status, req) => {
      if (!components.isSuccessCode(status)) {
        reject(new Error("Request failed with status code = "
                         + status
                         + " after NetUtil.asyncFetch for url = "
                         + uri));
        return;
      }

      const source = NetUtil.readInputStreamToString(stream, stream.available());
      stream.close();
      resolve(source);
    });
  });
}

const ActorRegistryFront = protocol.FrontClassWithSpec(actorRegistrySpec, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client,
                                             { actor: form.actorRegistryActor });

    this.manage(this);
  },

  registerActor: custom(function(uri, options) {
    return request(uri, options)
      .then(sourceText => {
        return this._registerActor(sourceText, uri, options);
      });
  }, {
    impl: "_registerActor"
  })
});

exports.ActorRegistryFront = ActorRegistryFront;
