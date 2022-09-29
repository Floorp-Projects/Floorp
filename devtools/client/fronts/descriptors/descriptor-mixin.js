/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * A Descriptor represents a debuggable context. It can be a browser tab, a tab on
 * a remote device, like a tab on Firefox for Android. But it can also be an add-on,
 * as well as firefox parent process, or just one of its content process.
 * It can be very similar to a Target. The key difference is the lifecycle of these two classes.
 * The descriptor is meant to be always alive and meaningful/usable until the end of the RDP connection.
 * Typically a Tab Descriptor will describe the tab and not the one document currently loaded in this tab,
 * while the Target, will describe this one document and a new Target may be created on each navigation.
 */
function DescriptorMixin(parentClass) {
  class Descriptor extends parentClass {
    constructor(client, targetFront, parentFront) {
      super(client, targetFront, parentFront);

      this._client = client;

      // Pass a true value in order to distinguish this event reception
      // from any manual destroy caused by the frontend
      this.on(
        "descriptor-destroyed",
        this.destroy.bind(this, { isServerDestroyEvent: true })
      );

      // Boolean flag to know if the DevtoolsClient should be closed
      // when this descriptor happens to be destroyed.
      // This is set by:
      // * target-from-url in case we are opening a toolbox
      //   with a dedicated DevToolsClient (mostly from about:debugging, when the client isn't "cached").
      // * TabDescriptor, when we are connecting to a local tab and expect
      //   the client, toolbox and descriptor to all follow the same lifecycle.
      this.shouldCloseClient = false;
    }

    get client() {
      return this._client;
    }

    async destroy({ isServerDestroyEvent } = {}) {
      if (this.isDestroyed()) {
        return;
      }
      // Cache the client attribute as in case of workers, TargetMixin class may nullify `_client`
      const { client } = this;

      // This workaround is mostly done for Workers, as WorkerDescriptor
      // extends the Target class, which causes some issue down the road:
      // In Target.destroy, we call WorkerDescriptorActor.detach *before* calling super.destroy(),
      // and so hold on before calling Front.destroy() which would reject all pending requests, including detach().
      // When calling detach, the server will emit "descriptor-destroyed", which will call Target.destroy again,
      // but will still be blocked on detach resolution and won't call Front.destroy, and won't reject pending requests either.
      //
      // So call Front.baseFrontClassDestroyed manually from here, so that we ensure rejecting the pending detach request
      // and unblock Target.destroy resolution.
      //
      // Here is the inheritance chain for WorkerDescriptor:
      // WorkerDescriptor -> Descriptor (from descriptor-mixin.js) -> Target (from target-mixin.js) -> Front (protocol.js) -> Pool (protocol.js) -> EventEmitter
      if (isServerDestroyEvent) {
        this.baseFrontClassDestroy();
      }

      await super.destroy();

      // See comment in DescriptorMixin constructor
      if (this.shouldCloseClient) {
        await client.close();
      }
    }
  }
  return Descriptor;
}
exports.DescriptorMixin = DescriptorMixin;
