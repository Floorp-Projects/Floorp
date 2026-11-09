// SPDX-License-Identifier: MPL-2.0

import type { TFormOption } from "#features-chrome/common/modal-parent/utils/type.ts";
import type { TWorkspaceContainerIdentity } from "./type.ts";

export type WorkspaceContainerOptionTexts = {
  noContainer: string;
  unavailableContainer: (userContextId: number) => string;
};

/**
 * Builds the container dropdown options for the workspace edit dialog.
 *
 * Gecko no longer stores `name` or `l10nId` on the built-in containers
 * (ContextualIdentityService migrate7to8), so labels must come from
 * `getUserContextLabel`, which resolves both built-in and user-created names.
 * A workspace that still points at a removed container keeps its value as an
 * explicit "unavailable" option so the dialog never shows a bare numeric ID and
 * saving without touching the field does not silently change the container.
 */
export function createWorkspaceContainerOptions(
  containers: readonly TWorkspaceContainerIdentity[],
  currentUserContextId: number,
  getUserContextLabel: (userContextId: number) => string,
  texts: WorkspaceContainerOptionTexts,
): TFormOption[] {
  const options: TFormOption[] = [
    { value: "0", label: texts.noContainer, icon: "" },
  ];
  for (const container of containers) {
    const id = container.userContextId;
    options.push({
      value: String(id),
      label: getUserContextLabel(id) || container.name ||
        texts.unavailableContainer(id),
      icon: "",
    });
  }
  if (
    currentUserContextId > 0 &&
    !containers.some((c) => c.userContextId === currentUserContextId)
  ) {
    options.push({
      value: String(currentUserContextId),
      label: texts.unavailableContainer(currentUserContextId),
      icon: "",
    });
  }
  return options;
}
