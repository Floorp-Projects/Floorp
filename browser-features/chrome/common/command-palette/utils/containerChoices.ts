// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type {
  CommandStepChoice,
  StepChoicesResult,
} from "../types.ts";
import Workspaces from "#features-chrome/common/workspaces";

/**
 * Loads container choices for command-palette commands that open a URL.
 * Returns a workspace-aware list: "Workspace Default" (pre-selected) first,
 * then "No Container", then every public container. The two built-in choices
 * are preserved on identity-enumeration failure so the step never goes empty.
 */
export async function loadContainerChoices(): Promise<StepChoicesResult> {
  // "No Container" never depends on the identity service, so build it first.
  const noContainer: CommandStepChoice = {
    label: i18next.t("commandPalette.reopenInContainerNoContainer", {
      defaultValue: "No Container",
    }),
    value: "0",
    description: i18next.t(
      "commandPalette.reopenInContainerNoContainerDesc",
      {
        defaultValue: "Open in the default context",
      },
    ),
  };

  // Built in outer scope so it survives an identity-enumeration failure.
  let workspaceDefault: CommandStepChoice | null = null;

  try {
    const { ContextualIdentityService } = ChromeUtils.importESModule(
      "resource://gre/modules/ContextualIdentityService.sys.mjs",
    );

    ContextualIdentityService.ensureDataReady();

    const workspaceDefaultId = Workspaces.getCtx()
      ?.getCurrentWorkspaceUserContextId() ?? 0;

    const workspaceDisplayName = workspaceDefaultId > 0
      ? (ContextualIdentityService.getUserContextLabel(workspaceDefaultId) || "")
      : i18next.t("commandPalette.reopenInContainerNoContainer", {
        defaultValue: "No Container",
      });

    workspaceDefault = {
      label: i18next.t("commandPalette.openUrlContainerWorkspaceDefault", {
        defaultValue: "Workspace Default",
      }),
      value: "workspace",
      description: i18next.t(
        "commandPalette.openUrlContainerWorkspaceDefaultDesc",
        { defaultValue: "Uses the workspace's default context" },
      ) + (workspaceDisplayName ? ` (${workspaceDisplayName})` : ""),
    };

    const identities = await ContextualIdentityService.getPublicIdentities();

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

    return {
      choices: [workspaceDefault, noContainer, ...containerChoices],
      defaultIndex: 0,
    };
  } catch (e) {
    console.error("[command-palette] Failed to load containers:", e);
    // Preserve the built-in choices that do not require identity enumeration.
    const preserved = workspaceDefault
      ? [workspaceDefault, noContainer]
      : [noContainer];
    return { choices: preserved, defaultIndex: 0 };
  }
}
