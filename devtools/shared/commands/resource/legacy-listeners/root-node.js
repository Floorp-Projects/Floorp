/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");

module.exports = async function({ targetFront, onAvailable, onDestroyed }) {
  // XXX: When watching root node for a non top-level target, this will also
  // ensure the inspector & walker fronts for the target are initialized.
  // This also implies that we call reparentRemoteFrame on the new walker, which
  // will create the link between the parent frame NodeFront and the inner
  // document NodeFront.
  //
  // This is not something that will work when the resource is moved to the
  // server. When it becomes a server side resource, a RootNode would be emitted
  // directly by the target actor.
  //
  // This probably means that the root node resource cannot remain a NodeFront.
  // It should not be a front and the client should be responsible for
  // retrieving the corresponding NodeFront.
  //
  // The other thing that we are missing with this patch is that we should only
  // create inspector & walker fronts (and call reparentRemoteFrame) when we get
  // a RootNode which is directly under an iframe node which is currently
  // visible and tracked in the markup view.
  //
  // For instance, with the following markup:
  //   html
  //     body
  //       div
  //         iframe
  //           remote doc
  //
  // If the markup view only sees nodes down to `div`, then the client is not
  // currently tracking the nodeFront for the `iframe`, and getting a new root
  // node for the remote document should NOT force the iframe to be tracked on
  // on the client.
  //
  // When we get a RootNode resource, we will need a way to check this before
  // initializing & reparenting the walker.
  //
  if (!targetFront.getTrait("isBrowsingContext")) {
    // The root-node resource is only available on browsing-context targets.
    return;
  }

  const inspectorFront = await targetFront.getFront("inspector");
  inspectorFront.walker.on("root-available", node => {
    node.resourceType = ResourceCommand.TYPES.ROOT_NODE;
    return onAvailable([node]);
  });

  inspectorFront.walker.on("root-destroyed", node => {
    node.resourceType = ResourceCommand.TYPES.ROOT_NODE;
    return onDestroyed([node]);
  });

  await inspectorFront.walker.watchRootNode();
};
