// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type { CommandStepChoice, StepChoicesResult } from "../types.ts";
import Workspaces from "#features-chrome/common/workspaces";

/**
 * Loads container choices for command-palette commands that open a URL.
 * Returns a workspace-aware list: "Workspace Default" (pre-selected) first,
 * then "No Container", then every public container. The two built-in choices
 * are preserved on identity-enumeration failure so the step never goes empty.
 */
export async function loadContainerChoices(
  targetWindow?: Window,
): Promise<StepChoicesResult> {
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

  // Workspace Default is built without the identity service so it survives
  // import / readiness / label-lookup failures. Its description is enriched
  // with the resolved container name once identities become available.
  const workspaceContext = Workspaces.getCtx(targetWindow);
  const workspaceId = workspaceContext?.getSelectedWorkspaceID() ?? null;
  const workspaceDefaultId =
    workspaceContext?.getCurrentWorkspaceUserContextId() ?? 0;
  const workspaceSnapshotIsCurrent = (): boolean => {
    const currentContext = Workspaces.getCtx(targetWindow);
    return currentContext === workspaceContext &&
      (currentContext?.getSelectedWorkspaceID() ?? null) === workspaceId &&
      (currentContext?.getCurrentWorkspaceUserContextId() ?? 0) ===
        workspaceDefaultId;
  };
  const baseWorkspaceDesc = i18next.t(
    "commandPalette.openUrlContainerWorkspaceDefaultDesc",
    { defaultValue: "Uses the workspace's default context" },
  );
  const workspaceDefault: CommandStepChoice = {
    label: i18next.t("commandPalette.openUrlContainerWorkspaceDefault", {
      defaultValue: "Workspace Default",
    }),
    value: "workspace",
    description: baseWorkspaceDesc,
  };

  try {
    const { ContextualIdentityService } = ChromeUtils.importESModule(
      "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
    );

    ContextualIdentityService.ensureDataReady();

    // Enrich the description with the resolved container name.
    const workspaceDisplayName = workspaceDefaultId > 0
      ? (ContextualIdentityService.getUserContextLabel(workspaceDefaultId) ||
        "")
      : i18next.t("commandPalette.reopenInContainerNoContainer", {
        defaultValue: "No Container",
      });
    if (workspaceDisplayName) {
      workspaceDefault.description =
        `${baseWorkspaceDesc} (${workspaceDisplayName})`;
    }

    const identities = await ContextualIdentityService.getPublicIdentities();
    if (!workspaceSnapshotIsCurrent()) {
      return loadContainerChoices(targetWindow);
    }

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
        const label = ContextualIdentityService.getUserContextLabel(
          userContextId,
        );
        return {
          label: label || "Unknown",
          value: String(userContextId),
          description: `${(container as { color: string }).color} • ${
            (container as { icon: string }).icon
          }`,
        };
      });

    return {
      choices: [workspaceDefault, noContainer, ...containerChoices],
      defaultIndex: 0,
    };
  } catch (e) {
    console.error("[command-palette] Failed to load containers:", e);
    if (!workspaceSnapshotIsCurrent()) {
      return loadContainerChoices(targetWindow);
    }
    // workspaceDefault and noContainer are both service-independent, so both
    // built-in choices are always available here.
    return { choices: [workspaceDefault, noContainer], defaultIndex: 0 };
  }
}
