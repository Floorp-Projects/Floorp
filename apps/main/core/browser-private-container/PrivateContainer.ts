/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ContextualIdentityService } = ChromeUtils.importESModule(
  "resource://gre/modules/ContextualIdentityService.sys.mjs",
);

export const PrivateContainer = {
  ENABLE_PRIVATE_CONTAINER_PREF: "floorp.privateContainer.enabled",
  PRIVATE_CONTAINER_L10N_ID: "floorp-private-container-name",
  getPrivateContainer() {
    const currentContainers = ContextualIdentityService._identities;
    return currentContainers.find(
      (container: { floorpPrivateContainer: boolean }) =>
        container.floorpPrivateContainer === true,
    );
  },

  getPrivateContainerUserContextId() {
    const privateContainer = this.getPrivateContainer();
    return privateContainer ? privateContainer.userContextId : null;
  },

  removePrivateContainerData() {
    const privateContainer = this.getPrivateContainer();
    if (!privateContainer || !privateContainer.userContextId) {
      return;
    }

    Services.clearData.deleteDataFromOriginAttributesPattern({
      userContextId: privateContainer.userContextId,
    });
  },

  async StartupCreatePrivateContainer() {
    ContextualIdentityService.ensureDataReady();

    if (PrivateContainer.getPrivateContainer()) {
      return;
    }

    const userContextId = ++ContextualIdentityService._lastUserContextId;

    const identity = {
      userContextId,
      public: true,
      icon: "chill",
      color: "purple",
      name: "Private Container",
      floorpPrivateContainer: true,
    };

    ContextualIdentityService._identities.push(identity);
    ContextualIdentityService.saveSoon();
    Services.obs.notifyObservers(
      ContextualIdentityService.getIdentityObserverOutput(identity),
      "contextual-identity-created",
    );
  },
};
