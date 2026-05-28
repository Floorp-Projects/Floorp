// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type { PaletteCommand, CommandStepChoice } from "../types.ts";
import { getJapaneseReadings } from "../utils/getJapaneseReadings.ts";

export async function loadContainers(): Promise<CommandStepChoice[]> {
  try {
    const { ContextualIdentityService } = ChromeUtils.importESModule(
      "resource://gre/modules/ContextualIdentityService.sys.mjs",
    );

    ContextualIdentityService.ensureDataReady();

    const identities = await ContextualIdentityService.getPublicIdentities();

    const noContainer: CommandStepChoice = {
      label: i18next.t("commandPalette.reopenInContainerNoContainer", {
        defaultValue: "No Container",
      }),
      value: "0",
      description: i18next.t(
        "commandPalette.reopenInContainerNoContainerDesc",
        {
          defaultValue: "Reopen in the default context",
        },
      ),
    };

    const containerChoices: CommandStepChoice[] = identities
      .filter(
        (identity: unknown) =>
          !(identity as { floorpPrivateContainer?: boolean })
            .floorpPrivateContainer,
      )
      .map((container: unknown) => {
        const userContextId = (container as { userContextId: number })
          .userContextId;
        // getUserContextLabel handles both l10nId (built-in) and name (user-created)
        const label =
          ContextualIdentityService.getUserContextLabel(userContextId);
        return {
          label: label || "Unknown",
          value: String(userContextId),
          description: `${(container as { color: string }).color} • ${(container as { icon: string }).icon}`,
        };
      });

    return [noContainer, ...containerChoices];
  } catch (err) {
    console.error("reopenInContainer loader failed", err);
    return [];
  }
}

export const reopenInContainerCommand: PaletteCommand = {
  id: "floorp-reopen-in-container",
  label: i18next.t("commandPalette.reopenInContainer", {
    defaultValue: "Reopen in Container",
  }),
  description: i18next.t("commandPalette.reopenInContainerDescription", {
    defaultValue: "Reopen the current tab in a specific container",
  }),
  category: "tabs",
  keywords: [
    "container",
    "reopen",
    "context",
    "identity",
    "reopen in",
    ...getJapaneseReadings("floorp-reopen-in-container"),
  ],
  steps: [
    {
      id: "container",
      label: i18next.t("commandPalette.reopenInContainerStepLabel", {
        defaultValue: "Select a container",
      }),
      placeholder: i18next.t(
        "commandPalette.reopenInContainerStepPlaceholder",
        {
          defaultValue: "Choose a container...",
        },
      ),
      choicesLoader: loadContainers,
    },
  ],
  fn: (_win: Window, args?: Record<string, string>) => {
    const containerId = Number.parseInt(args?.container ?? "0");
    if (Number.isNaN(containerId)) {
      console.error(
        "[ReopenInContainer] Invalid container ID:",
        args?.container,
      );
      return;
    }
    const tab = globalThis.gBrowser.selectedTab;

    if (!tab) return;

    let triggeringPrincipal: nsIPrincipal;

    if ((tab as unknown as { linkedPanel?: unknown }).linkedPanel) {
      triggeringPrincipal = (
        tab.linkedBrowser as unknown as { contentPrincipal: nsIPrincipal }
      ).contentPrincipal;
    } else {
      const getTabState = globalThis.SessionStore?.getTabState;
      if (!getTabState) {
        console.error(
          "[ReopenInContainer] SessionStore.getTabState is not available",
        );
        return;
      }
      try {
        const tabState = JSON.parse(getTabState(tab)) as Record<
          string,
          unknown
        >;
        triggeringPrincipal = globalThis.E10SUtils.deserializePrincipal(
          tabState.triggeringPrincipal_base64,
        ) as nsIPrincipal;
      } catch (error) {
        console.error(
          "[ReopenInContainer] Failed to deserialize tab state/principal",
          error,
        );
        return;
      }
    }

    if (!triggeringPrincipal || triggeringPrincipal.isNullPrincipal) {
      triggeringPrincipal = Services.scriptSecurityManager.createNullPrincipal({
        userContextId: containerId,
      }) as nsIPrincipal;
    } else if (triggeringPrincipal.isContentPrincipal) {
      triggeringPrincipal = Services.scriptSecurityManager.principalWithOA(
        triggeringPrincipal,
        {
          userContextId: containerId,
        },
      );
    }

    const tabLinkedBrowser = tab.linkedBrowser as unknown as {
      currentURI?: { spec: string };
    };
    const url = tabLinkedBrowser.currentURI?.spec ?? "about:blank";
    const tabPos = (tab as unknown as { _tPos?: number })._tPos ?? 0;
    const tabPinned = (tab as unknown as { pinned?: boolean }).pinned;

    try {
      const newTab = globalThis.gBrowser.addTab(url, {
        userContextId: containerId,
        pinned: tabPinned,
        tabIndex: tabPos + 1,
        triggeringPrincipal,
      } as {
        skipAnimation?: boolean;
        inBackground?: boolean;
        userContextId?: number;
        triggeringPrincipal?: unknown;
        pinned?: boolean;
        index?: number;
        tabIndex?: number;
      });

      if (globalThis.gBrowser.selectedTab === tab) {
        globalThis.gBrowser.selectedTab = newTab;
      }

      const tabMuted = (tab as unknown as { muted?: boolean }).muted;
      const newTabMuted = (newTab as unknown as { muted?: boolean }).muted;
      const toggleFn = (
        newTab as unknown as { toggleMuteAudio?: (reason?: unknown) => void }
      ).toggleMuteAudio;
      if (tabMuted && !newTabMuted && toggleFn) {
        toggleFn(
          (tab as unknown as { muteReason?: unknown }).muteReason as string,
        );
      }

      globalThis.gBrowser.removeTab(tab);
    } catch (error) {
      console.error(
        "[ReopenInContainer] Failed to reopen tab in container",
        error,
      );
    }
  },
};
