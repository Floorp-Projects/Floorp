// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import ModalParent from "../modal-parent/index.ts";
import type {
  TForm,
  TFormItem,
  TFormResult,
} from "../modal-parent/utils/type.ts";
import { workspacesDataStore } from "../workspaces/data/data.ts";
import type { TWorkspace, TWorkspaceID } from "../workspaces/utils/type.ts";
import { getWorkspaceBookmarks } from "./service.ts";
import {
  BookmarkImportReviewChangedError,
  reviewBookmarkImport,
} from "./import-review.ts";
import type {
  BookmarkEditingWindow,
  BookmarkImportMapping,
  BookmarkOperationConfirmation,
  ScopedBookmarkFolder,
} from "./types.ts";

const tr = (
  key: string,
  fallback: string,
  options?: Record<string, unknown>,
): string =>
  i18next.t(`workspaceBookmarks.${key}`, {
    defaultValue: fallback,
    ...options,
  }) as string;

function workspaceChoices(): Array<{ value: string; label: string }> {
  const data = workspacesDataStore.data as unknown as Map<
    TWorkspaceID,
    TWorkspace
  >;
  return workspacesDataStore.order.flatMap((id) => {
    const workspace = data.get(id);
    return workspace ? [{ value: id, label: workspace.name }] : [];
  });
}

function alert(win: Window, message: string): void {
  Services.prompt.alert(
    win as unknown as mozIDOMWindowProxy,
    tr("title", "Workspace bookmark visibility"),
    message,
  );
}

async function showForm(form: TForm): Promise<TFormResult | null> {
  const modal = ModalParent.getInstance();
  modal.init();
  return await modal.showNoraModal(form, { width: 620, height: 720 }, () => {});
}

export function scopeName(ids: readonly string[] | null): string {
  if (ids === null) return tr("common", "All workspaces");
  if (!ids.length) return tr("unassigned", "No display destination");
  const names = new Map(
    workspaceChoices().map((item) => [item.value, item.label]),
  );
  return ids.map((id) =>
    names.get(id) ??
      tr("unavailable", "Unavailable workspace ({{id}})", {
        id: id.slice(0, 8),
      })
  ).join(", ");
}

export async function showFolderScope(
  win: Window,
  guid: string,
): Promise<void> {
  try {
    const service = getWorkspaceBookmarks();
    await service.ready;
    if (win.closed) return;
    const folder = (await service.listFolders()).find((item) =>
      item.guid === guid
    );
    if (!folder) {
      throw new Error(
        tr("missingFolder", "This bookmark folder no longer exists."),
      );
    }
    const oldScope = JSON.stringify(service.getScope(guid));
    const choices = workspaceChoices();
    const knownIds = new Set(choices.map((item) => item.value));
    const unavailable = (folder.scope ?? []).filter((id) => !knownIds.has(id));
    const result = await showForm({
      title: tr("editTitle", "Where to display {{name}}", {
        name: folder.title || folder.path,
      }),
      description: tr(
        "scopeDescription",
        "The folder and its contents remain in your bookmark library. Display destinations are local to this device; bookmarks still sync normally. Parent folder restrictions also apply.",
      ) +
        `\n${tr("effective", "Effective destinations")}: ${
          scopeName(folder.effectiveScope.workspaceIds)
        }`,
      forms: [
        {
          id: "mode",
          type: "select",
          label: tr("displayIn", "Display in"),
          value: folder.scope === null ? "all" : "selected",
          options: [
            {
              value: "all",
              label: tr(
                "inherit",
                "Inherit from the parent (all workspaces when unrestricted)",
              ),
            },
            { value: "selected", label: tr("selected", "Selected workspaces") },
          ],
        },
        ...choices.map((item): TFormItem => ({
          id: `workspace-${item.value}`,
          type: "checkbox",
          label: item.label,
          value: String(folder.scope?.includes(item.value) ?? false),
          when: { id: "mode", value: "selected" },
        })),
        ...(unavailable.length
          ? [{
            id: "keepUnavailable",
            type: "checkbox" as const,
            label: tr(
              "keepUnavailable",
              "Keep {{count}} archived or unavailable destinations",
              { count: unavailable.length },
            ),
            value: "true",
            when: { id: "mode", value: "selected" },
          }]
          : []),
      ],
      submitLabel: tr("save", "Save"),
      cancelLabel: tr("cancel", "Cancel"),
    });
    if (!result || win.closed) return;
    if (JSON.stringify(service.getScope(guid)) !== oldScope) {
      alert(
        win,
        tr(
          "changed",
          "This folder's settings changed while the form was open. Open it again to review the latest settings.",
        ),
      );
      return;
    }
    const next = result.mode === "all" ? null : [
      ...choices.filter((item) => result[`workspace-${item.value}`] === "true")
        .map((item) => item.value),
      ...(result.keepUnavailable === "true" ? unavailable : []),
    ];
    let effective = next;
    for (const ancestor of service.getEffectiveScope(guid).inheritedFrom) {
      if (ancestor === guid) continue;
      const restriction = service.getScope(ancestor);
      if (restriction) {
        effective = effective === null
          ? [...restriction]
          : effective.filter((id) => restriction.includes(id));
      }
    }
    if (
      effective?.length === 0 &&
      !Services.prompt.confirm(
        win as unknown as mozIDOMWindowProxy,
        tr("title", "Workspace bookmark visibility"),
        tr(
          "emptyConfirm",
          "This folder will not appear in any workspace. It will remain available in the full bookmark manager. Save this setting?",
        ),
      )
    ) return;
    await service.setFolderScope(guid, next);
  } catch (error) {
    console.error("[WorkspaceBookmarks] Scope edit failed", error);
    if (!win.closed) {
      alert(
        win,
        error instanceof Error
          ? error.message
          : tr("error", "The bookmark settings could not be updated."),
      );
    }
  }
}

async function pickSettingsFile(
  win: Window,
  save: boolean,
): Promise<nsIFile | null> {
  const picker = Cc["@mozilla.org/filepicker;1"].createInstance(
    Ci.nsIFilePicker,
  );
  picker.init(
    (win as BookmarkEditingWindow).browsingContext,
    tr(
      save ? "exportTitle" : "importTitle",
      save
        ? "Back up workspace bookmark settings"
        : "Restore workspace bookmark settings",
    ),
    save ? Ci.nsIFilePicker.modeSave : Ci.nsIFilePicker.modeOpen,
  );
  picker.appendFilter(tr("settingsFile", "Floorp bookmark settings"), "*.json");
  picker.defaultExtension = "json";
  if (save) picker.defaultString = "floorp-workspace-bookmarks.json";
  const result = await new Promise<nsIFilePicker.ResultCode>((resolve) =>
    picker.open(resolve)
  );
  return result === Ci.nsIFilePicker.returnCancel ? null : picker.file;
}

async function confirmImport(win: Window, raw: string | object): Promise<void> {
  const service = getWorkspaceBookmarks();
  const applied = await reviewBookmarkImport({
    service,
    raw,
    workspaceIds: () => workspaceChoices().map((item) => item.value),
    isOpen: () => !win.closed,
    map: async (preview) => {
      const workspaces = workspaceChoices();
      const folders = await service.listFolders();
      if (win.closed) return null;
      const forms: TFormItem[] = [
        ...preview.unresolvedWorkspaces.map((id, index): TFormItem => ({
          id: `workspace-${index}`,
          type: "select",
          label: tr("mapWorkspace", "Restore workspace {{id}} to", {
            id: id.slice(0, 8),
          }),
          value: "",
          options: [
            { value: "", label: tr("skip", "Leave unresolved") },
            ...workspaces,
          ],
        })),
        ...preview.unresolvedFolders.map((id, index): TFormItem => ({
          id: `folder-${index}`,
          type: "select",
          label: tr("mapFolder", "Restore folder {{id}} to", { id }),
          value: "",
          options: [
            { value: "", label: tr("skip", "Leave unresolved") },
            ...folders.map((folder) => ({
              value: folder.guid,
              label: folder.path,
            })),
          ],
        })),
      ];
      const result = await showForm({
        title: tr("importTitle", "Restore workspace bookmark settings"),
        description: tr(
          "mappingDescription",
          "Choose destinations for unresolved folders and workspaces. The next screen shows the resulting changes before anything is applied. Entries left unresolved will be skipped.",
        ),
        forms,
        submitLabel: tr("review", "Review changes"),
        cancelLabel: tr("cancel", "Cancel"),
      });
      if (!result || win.closed) return null;
      const mapping: BookmarkImportMapping = { folders: {}, workspaces: {} };
      preview.unresolvedFolders.forEach((id, index) => {
        const value = result[`folder-${index}`];
        if (typeof value === "string" && value) mapping.folders![id] = value;
      });
      preview.unresolvedWorkspaces.forEach((id, index) => {
        const value = result[`workspace-${index}`];
        if (typeof value === "string" && value) mapping.workspaces![id] = value;
      });
      return mapping;
    },
    confirm: async (preview) => {
      const details = preview.changes.map((change) =>
        `${change.title || change.guid}: ${scopeName(change.before)} → ${
          scopeName(change.after)
        }`
      ).join("\n");
      return Boolean(
        await showForm({
          title: tr("reviewTitle", "Review bookmark display settings"),
          description: tr(
            "reviewDescription",
            "Apply {{count}} reviewed folder changes. {{folders}} folders and {{workspaces}} workspaces remain unresolved and will be skipped. Settings not listed here are preserved.",
            {
              count: preview.changes.length,
              folders: preview.unresolvedFolders.length,
              workspaces: preview.unresolvedWorkspaces.length,
            },
          ) + `\n${details}`,
          forms: [],
          submitLabel: tr("apply", "Apply reviewed settings"),
          cancelLabel: tr("cancel", "Cancel"),
        }),
      );
    },
  });
  if (!applied || win.closed) return;
  alert(
    win,
    tr(
      "importComplete",
      "Settings restored. {{folders}} folders and {{workspaces}} workspaces remain unresolved.",
      {
        folders: applied.unresolvedFolders.length,
        workspaces: applied.unresolvedWorkspaces.length,
      },
    ),
  );
}

export async function showBookmarkScopeManager(win: Window): Promise<void> {
  try {
    const service = getWorkspaceBookmarks();
    await service.ready;
    if (win.closed) return;
    const status = service.getStatus();
    const folders: ScopedBookmarkFolder[] = await service.listFolders();
    const result = await showForm({
      title: tr("title", "Workspace bookmark visibility"),
      description: status.state === "error"
        ? tr(
          "recoveryDescription",
          "The display settings could not be read. Normal bookmark results are paused. Your bookmarks remain available in the full manager. Restore a settings backup or explicitly turn off filtering.",
        )
        : tr(
          "managerDescription",
          "Choose a folder to display in selected workspaces. Unconfigured folders inherit their parent. This organizes display; history, extensions and other devices can still show the same URLs.",
        ),
      forms: [
        {
          id: "action",
          type: "select",
          label: tr("action", "Action"),
          value: "scope",
          options: [
            {
              value: "scope",
              label: tr("editAction", "Change folder destinations"),
            },
            {
              value: "library",
              label: tr("library", "Open the full bookmark manager"),
            },
            {
              value: "export",
              label: tr("export", "Back up display settings"),
            },
            {
              value: "import",
              label: tr("import", "Restore display settings from a file"),
            },
            ...(status.hasRecovery
              ? [{
                value: "recover",
                label: tr("recover", "Restore the last valid settings"),
              }]
              : []),
            {
              value: "toggle",
              label: status.enabled
                ? tr("disable", "Turn off filtering and display all bookmarks")
                : tr("enable", "Enable workspace filtering"),
            },
          ],
        },
        {
          id: "folder",
          type: "select",
          label: tr("folder", "Folder"),
          value: folders[0]?.guid ?? "",
          when: { id: "action", value: "scope" },
          options: folders.map((folder) => ({
            value: folder.guid,
            label: `${folder.path} — ${
              scopeName(folder.effectiveScope.workspaceIds)
            }`,
          })),
        },
      ],
      submitLabel: tr("continue", "Continue"),
      cancelLabel: tr("cancel", "Cancel"),
    });
    if (!result || win.closed) return;
    switch (result.action) {
      case "scope":
        if (typeof result.folder === "string" && result.folder) {
          await showFolderScope(win, result.folder);
        } else {alert(
            win,
            tr(
              "noFolders",
              "Create a bookmark folder first, then choose its display destinations.",
            ),
          );}
        break;
      case "library": {
        const command = Reflect.get(win, "PlacesCommandHook") as {
          showPlacesOrganizer(root: string): void;
        };
        command.showPlacesOrganizer("AllBookmarks");
        break;
      }
      case "toggle":
        service.setEnabled(!status.enabled);
        break;
      case "export": {
        const file = await pickSettingsFile(win, true);
        if (file) {
          const settings = await service.exportSettings({
            workspaceNames: Object.fromEntries(
              workspaceChoices().map((item) => [item.value, item.label]),
            ),
          });
          await IOUtils.writeUTF8(
            file.path,
            JSON.stringify(settings, null, 2),
            { tmpPath: `${file.path}.tmp` },
          );
        }
        break;
      }
      case "import": {
        const file = await pickSettingsFile(win, false);
        if (file) await confirmImport(win, await IOUtils.readUTF8(file.path));
        break;
      }
      case "recover": {
        const recovery = await service.readRecoverySettings();
        if (recovery) await confirmImport(win, recovery);
        break;
      }
    }
  } catch (error) {
    console.error("[WorkspaceBookmarks] Settings failed", error);
    if (!win.closed) {
      alert(
        win,
        error instanceof BookmarkImportReviewChangedError
          ? tr(
            "importChanged",
            "Bookmark display settings changed while the review was open. Restore the file again to review the latest changes.",
          )
          : error instanceof Error
          ? error.message
          : tr("error", "The bookmark settings could not be updated."),
      );
    }
  }
}

export function confirmBookmarkOperation(
  win: Window,
  operation: BookmarkOperationConfirmation,
): Promise<boolean> {
  if (win.closed) return Promise.resolve(false);
  const message = operation.kind === "delete"
    ? tr(
      "deleteConfirm",
      "Delete this folder and its contents, including {{count}} bookmarks hidden in this workspace? Shared bookmarks will also disappear from their other destinations.",
      { count: operation.hiddenCount ?? 0 },
    )
    : tr(
      "transferConfirm",
      "This operation changes where the bookmarks are displayed. New destinations: {{destinations}}. Continue?",
      { destinations: scopeName(operation.scope ?? null) },
    );
  return Promise.resolve(
    Services.prompt.confirm(
      win as unknown as mozIDOMWindowProxy,
      tr("title", "Workspace bookmark visibility"),
      message,
    ),
  );
}
