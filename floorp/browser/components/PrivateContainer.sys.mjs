/* eslint-disable mozilla/use-static-import */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ContextualIdentityService } = ChromeUtils.importESModule(
   "resource://gre/modules/ContextualIdentityService.sys.mjs"
);

export const EXPORTED_SYMBOLS = ["PrivateContainer"];

export let PrivateContainer = {
    ENABLE_PRIVATE_CONTAINER_PREF: "floorp.privateContainer.enabled",
    PRIVATE_CONTAINER_L10N_ID: "floorp-private-container-name",

    Functions: {
        getPrivateContainer() {
            const currentContainers = ContextualIdentityService._identities;
            for (const container of currentContainers) {
                if (container.floorpPrivateContainer === true) {
                    return container;
                }
            }
            return null;
        },

        getPrivateContainerUserContextId() {
            const currentContainers = ContextualIdentityService._identities;
            for (const container of currentContainers) {
                if (container.floorpPrivateContainer === true) {
                    return container.userContextId;
                }
            }
            return null;
        },

        removePrivateContainerData() {
            const privateContainer = PrivateContainer.Functions.getPrivateContainer();
            if (!privateContainer || !privateContainer.userContextId) {
                return;
            }

            Services.clearData.deleteDataFromOriginAttributesPattern({ userContextId: privateContainer.userContextId });
        },
    
        async StartupCreatePrivateContainer() {
            const l10n = new Localization(["browser/floorp.ftl"], true);
            ContextualIdentityService.ensureDataReady();

            if (PrivateContainer.Functions.getPrivateContainer()) {
                return;
            }

            let userContextId = ++ContextualIdentityService._lastUserContextId;
        
            let identity = {
              userContextId,
              public: true,
              icon: "chill",
              color: "purple",
              name: await l10n.formatValue(PrivateContainer.PRIVATE_CONTAINER_L10N_ID),
              floorpPrivateContainer: true,
            };
        
            ContextualIdentityService._identities.push(identity);
            ContextualIdentityService.saveSoon();
            Services.obs.notifyObservers(
                ContextualIdentityService.getIdentityObserverOutput(identity),
              "contextual-identity-created"
            );        
        },
    },
}
