/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { components } = require("chrome");
const Services = require("Services");
const {
  actorActorSpec,
  actorRegistrySpec,
} = require("devtools/shared/specs/actor-registry");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

loader.lazyImporter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");

class ActorActorFront extends FrontClassWithSpec(actorActorSpec) {}

exports.ActorActorFront = ActorActorFront;
registerFront(ActorActorFront);

function request(uri) {
  return new Promise((resolve, reject) => {
    try {
      uri = Services.io.newURI(uri);
    } catch (e) {
      reject(e);
    }

    NetUtil.asyncFetch(
      {
        uri,
        loadUsingSystemPrincipal: true,
      },
      (stream, status, req) => {
        if (!components.isSuccessCode(status)) {
          reject(
            new Error(
              "Request failed with status code = " +
                status +
                " after NetUtil.asyncFetch for url = " +
                uri
            )
          );
          return;
        }

        const source = NetUtil.readInputStreamToString(
          stream,
          stream.available()
        );
        stream.close();
        resolve(source);
      }
    );
  });
}

class ActorRegistryFront extends FrontClassWithSpec(actorRegistrySpec) {
  constructor(client) {
    super(client);

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "actorRegistryActor";
  }

  registerActor(uri, options) {
    return request(uri, options).then(sourceText => {
      return super.registerActor(sourceText, uri, options);
    });
  }
}

exports.ActorRegistryFront = ActorRegistryFront;
registerFront(ActorRegistryFront);
