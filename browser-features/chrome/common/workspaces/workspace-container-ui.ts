// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type { WorkspaceContainerRef } from "#modules/common/workspace-container-types.ts";
import type { WorkspaceContainerResolution } from "#modules/common/workspace-container-bindings.ts";
import type {
  WorkspaceContainerBindingAPI,
  WorkspaceContainerPrompt,
  WorkspaceContainerSetupActions,
} from "./workspace-container-ui-types.ts";

export function getWorkspaceContainerBindings(): WorkspaceContainerBindingAPI {
  return (ChromeUtils.importESModule(
    "resource://noraneko/modules/workspaces/WorkspaceContainerBindings.sys.mjs",
  ) as { WorkspaceContainerBindings: WorkspaceContainerBindingAPI })
    .WorkspaceContainerBindings;
}

export function workspaceContainerNeedsReopen(
  currentId: number,
  currentInstance: string,
  resolved: WorkspaceContainerResolution,
): boolean {
  return resolved.status !== "resolved" ||
    currentId !== resolved.userContextId ||
    Boolean(
      currentInstance && resolved.instanceId &&
        currentInstance !== resolved.instanceId,
    );
}

export function workspaceContainerPendingText(
  resolution: WorkspaceContainerResolution,
): string | null {
  if (resolution.status === "resolved") return null;
  return i18next.t(`workspaces.containers.${resolution.reason}`, {
    name: resolution.definition?.name ?? "",
  });
}

export function nativeWorkspaceContainerPrompt(): WorkspaceContainerPrompt {
  const parent = window as unknown as mozIDOMWindowProxy;
  return {
    confirm: (title, message) =>
      Services.prompt.confirm(parent, title, message),
    select: (title, message, items) => {
      const selected = { value: 0 };
      return Services.prompt.select(parent, title, message, items, selected)
        ? selected.value
        : null;
    },
    alert: (title, message) => Services.prompt.alert(parent, title, message),
  };
}

/** Called only by an explicit menu command; incoming sync never opens a modal. */
export async function setupWorkspaceContainers(
  refs: WorkspaceContainerRef[],
  bindings: WorkspaceContainerBindingAPI,
  actions: WorkspaceContainerSetupActions,
  prompt: WorkspaceContainerPrompt = nativeWorkspaceContainerPrompt(),
): Promise<void> {
  if (!bindings.acquirePrompt()) return;
  const epoch = bindings.getEpoch();
  const current = () => bindings.isCurrentEpoch(epoch) && actions.isCurrent();
  const title = i18next.t("workspaces.containers.title");
  try {
    const seen = new Set<string>();
    for (const ref of refs) {
      if (!current()) return;
      if (ref.kind === "none") continue;
      const key = ref.kind === "container" ? ref.containerId : ref.kind;
      if (seen.has(key)) continue;
      seen.add(key);
      const resolution = await bindings.resolve(ref);
      if (!current()) return;
      if (resolution.status === "resolved") continue;
      if (ref.kind === "legacy-unresolved") {
        const scopes = ["default", "tabs", "all"] as const;
        const scopeIndex = prompt.select(
          title,
          i18next.t("workspaces.containers.legacy-scope"),
          scopes.map((scope) =>
            i18next.t(`workspaces.containers.legacy-scope-${scope}`)
          ),
        );
        if (scopeIndex === null || !scopes[scopeIndex] || !current()) return;
        const candidates = bindings.listLocalContainers();
        const items = candidates.map((candidate) =>
          `${candidate.name} (${candidate.color}, ${candidate.icon})`
        );
        const noneIndex = items.length;
        items.push(i18next.t("workspaces.containers.legacy-none"));
        const newIndex = items.length;
        items.push(i18next.t("workspaces.containers.legacy-create"));
        const selection = prompt.select(
          title,
          i18next.t("workspaces.containers.legacy-target"),
          items,
        );
        if (selection === null || !current()) return;
        const target = selection === newIndex
          ? "new"
          : selection === noneIndex
          ? 0
          : candidates[selection]?.userContextId;
        if (target === undefined) return;
        await actions.resolveLegacy(target, scopes[scopeIndex]);
        continue;
      }
      if (ref.kind !== "container" || !resolution.definition) {
        prompt.alert(title, workspaceContainerPendingText(resolution) ?? "");
        continue;
      }
      const name = resolution.definition.name;
      if (
        resolution.reason === "confirmation" &&
        resolution.candidates.length === 1
      ) {
        const candidate = resolution.candidates[0];
        if (
          !prompt.confirm(
            title,
            i18next.t("workspaces.containers.confirm", { name }),
          )
        ) return;
        if (!current()) return;
        await bindings.bind(ref.containerId, candidate.userContextId, epoch);
      } else if (
        ["confirmation", "deleted", "creation-failed"].includes(
          resolution.reason,
        )
      ) {
        const candidates = resolution.reason === "confirmation"
          ? resolution.candidates
          : bindings.listLocalContainers();
        const items = candidates.map((candidate) =>
          `${candidate.name} (${candidate.color}, ${candidate.icon})`
        );
        const createIndex = items.length;
        items.push(i18next.t("workspaces.containers.create", { name }));
        const unsubscribeIndex = items.length;
        items.push(i18next.t("workspaces.containers.unsubscribe"));
        const selected = prompt.select(
          title,
          workspaceContainerPendingText(resolution) ?? name,
          items,
        );
        if (selected === null || !current()) return;
        if (selected === unsubscribeIndex) {
          await actions.unsubscribe();
          return;
        }
        if (selected === createIndex) {
          await bindings.create(ref.containerId, epoch);
        } else if (candidates[selected]) {
          await bindings.bind(
            ref.containerId,
            candidates[selected].userContextId,
            epoch,
          );
        }
      } else {
        prompt.alert(title, workspaceContainerPendingText(resolution) ?? "");
      }
    }
    if (current()) await actions.retry();
  } finally {
    bindings.releasePrompt(epoch);
  }
}
