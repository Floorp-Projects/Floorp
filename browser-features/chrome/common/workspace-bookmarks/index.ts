// SPDX-License-Identifier: MPL-2.0

import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { createEffect, onCleanup } from "solid-js";
import i18next from "i18next";
import { selectedWorkspaceID } from "../workspaces/data/data.ts";
import { enabled } from "../workspaces/data/config.ts";
import {
  BOOKMARK_SCOPES_CHANGED,
  getWorkspaceBookmarks,
  isBookmarkContextCurrent,
} from "./service.ts";
import {
  confirmBookmarkOperation,
  scopeName,
  showBookmarkScopeManager,
  showFolderScope,
} from "./ui.ts";
import type {
  BookmarkEditingWindow,
  LocalizedBookmarkTitles,
  WorkspaceBookmarksUI,
} from "./types.ts";

export { showBookmarkScopeManager, showFolderScope } from "./ui.ts";

const XHTML = "http://www.w3.org/1999/xhtml";
const tr = (key: string, fallback: string): string =>
  i18next.t(`workspaceBookmarks.${key}`, { defaultValue: fallback }) as string;

function contextBookmarkGuid(menu: Element): string | null {
  let node = Reflect.get(menu, "triggerNode") ??
    Reflect.get(document, "popupNode");
  while (node instanceof Element) {
    const result: unknown = Reflect.get(node, "_placesNode") ??
      Reflect.get(node, "placesNode");
    if (result && typeof result === "object") {
      for (const key of ["bookmarkGuid", "itemGuid", "guid"]) {
        const value: unknown = Reflect.get(result, key);
        if (typeof value === "string" && value) return value;
      }
    }
    node = node.parentElement;
  }
  return null;
}

@noraComponent(import.meta.hot)
export default class WorkspaceBookmarkFeature extends NoraComponentBase {
  init(): void {
    const win = window as BookmarkEditingWindow;
    const service = getWorkspaceBookmarks();
    const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
      "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
    ) as { PrivateBrowsingUtils: { isWindowPrivate(win: Window): boolean } };
    let disposed = false;
    const updateContext = () => {
      if (disposed || win.closed) return;
      service.setWindowContext(
        win,
        PrivateBrowsingUtils.isWindowPrivate(win)
          ? null
          : selectedWorkspaceID(),
        enabled(),
      );
    };
    createEffect(updateContext);
    void service.ready.then(updateContext).catch((error: unknown) => {
      console.error("[WorkspaceBookmarks] Initialization failed", error);
    });

    const labelManager = (manager: Window) => {
      const doc = manager.document;
      if (
        doc.documentElement.getAttribute("windowtype") !== "Places:Organizer" ||
        doc.getElementById("floorp-all-bookmarks-notice")
      ) return;
      const notice = doc.createElementNS(XHTML, "div");
      notice.id = "floorp-all-bookmarks-notice";
      notice.setAttribute("role", "note");
      notice.style.cssText =
        "padding:6px 12px;font:menu;background:var(--toolbar-bgcolor);color:var(--toolbar-color)";
      notice.textContent = tr(
        "allManager",
        "All bookmarks — workspace display filters are not applied",
      );
      const view = doc.getElementById("placesView");
      if (view) view.before(notice);
    };
    const managerObserver = {
      observe: (subject: nsISupports) => {
        const manager = subject as unknown as Window;
        if (!manager || typeof manager.addEventListener !== "function") return;
        manager.addEventListener("load", () => labelManager(manager), {
          once: true,
        });
      },
    };
    Services.obs.addObserver(managerObserver, "domwindowopened");
    for (const manager of Services.wm.getEnumerator("Places:Organizer")) {
      labelManager(manager as unknown as Window);
    }

    const ui: WorkspaceBookmarksUI = {
      manage: () => showBookmarkScopeManager(win),
      showFolderScope: (guid) => showFolderScope(win, guid),
      confirmOperation: (operation) => confirmBookmarkOperation(win, operation),
    };
    win.floorpWorkspaceBookmarksUI = ui;
    const menu = document.getElementById("placesContext");
    const scopeItem = document.createXULElement("menuitem");
    scopeItem.id = "floorp-bookmark-folder-workspaces";
    scopeItem.setAttribute(
      "label",
      tr("contextMenu", "Display in workspaces…"),
    );
    scopeItem.setAttribute("hidden", "true");
    let menuGeneration = 0;
    const hideContextMenu = () => {
      menuGeneration++;
      scopeItem.removeAttribute("data-folder-guid");
      scopeItem.setAttribute("hidden", "true");
    };
    const showContextMenu = async () => {
      const generation = ++menuGeneration;
      scopeItem.removeAttribute("data-folder-guid");
      scopeItem.setAttribute("hidden", "true");
      const guid = menu ? contextBookmarkGuid(menu) : null;
      if (!guid) return;
      const context = service.getContext(win);
      try {
        const folder = (await service.listFolders()).find((item) =>
          item.guid === guid
        );
        if (
          generation !== menuGeneration ||
          !isBookmarkContextCurrent(win, context)
        ) return;
        if (folder) scopeItem.removeAttribute("hidden");
        scopeItem.setAttribute("data-folder-guid", guid);
      } catch (error) {
        console.error("[WorkspaceBookmarks] Folder context menu failed", error);
      }
    };
    const editFolderScope = () => {
      const guid = scopeItem.getAttribute("data-folder-guid");
      if (guid) void ui.showFolderScope(guid);
    };
    menu?.appendChild(scopeItem);
    menu?.addEventListener("popupshowing", showContextMenu);
    menu?.addEventListener("popuphidden", hideContextMenu);
    scopeItem.addEventListener("command", editFolderScope);

    let starPanel = document.getElementById("editBookmarkPanel") as
      | XULPopupElement
      | null;
    let starGeneration = 0;
    let destinationTimer = 0;
    let workspaceSaveInProgress = false;
    const removeStarControls = () => {
      starGeneration++;
      win.clearInterval(destinationTimer);
      document.getElementById("floorp-workspace-bookmark-controls")?.remove();
    };
    const showStarControls = async () => {
      removeStarControls();
      const generation = starGeneration;
      const context = service.getContext(win);
      const star = win.StarUI;
      if (!starPanel || !star?.getWorkspaceBookmarks || !context.ready) return;
      try {
        const records = await star.getWorkspaceBookmarks();
        if (
          generation !== starGeneration ||
          !isBookmarkContextCurrent(win, context)
        ) return;
        const box = document.createElementNS(XHTML, "div");
        box.id = "floorp-workspace-bookmark-controls";
        box.style.cssText =
          "display:flex;flex-direction:column;gap:8px;padding:8px 0;color:inherit";
        const folders = await service.listFolders();
        if (
          generation !== starGeneration ||
          !isBookmarkContextCurrent(win, context)
        ) return;
        const { PlacesUtils } = ChromeUtils.importESModule(
          "resource://gre/modules/PlacesUtils.sys.mjs",
        ) as { PlacesUtils: LocalizedBookmarkTitles };
        const folderTitle = (guid: string) =>
          folders.find((item) => item.guid === guid)?.path ||
          PlacesUtils.bookmarks.getLocalizedTitle({
            guid,
            title: tr("folder", "Folder"),
          });
        const editingState = win.gEditItemOverlay?.bookmarkState;
        const destination = document.createElementNS(XHTML, "div");
        destination.id = "floorp-bookmark-destination-scope";
        destination.setAttribute("aria-live", "polite");
        const updateDestination = () => {
          if (
            !editingState ||
            win.gEditItemOverlay?.bookmarkState !== editingState ||
            starPanel?.state !== "open" ||
            !isBookmarkContextCurrent(win, context)
          ) return;
          const guid = editingState.parentGuid;
          if (!guid) return;
          const folderName = folderTitle(guid);
          const scope = service.getEffectiveScope(guid);
          const text = `${
            tr("saveDestination", "Save destination")
          }: ${folderName}\n${tr("effective", "Effective destinations")}: ${
            scopeName(scope.workspaceIds)
          }${
            context.scopeEnabled ? "" : `\n${
              tr(
                "filteringOff",
                "Workspace filtering is off; this folder currently appears everywhere.",
              )
            }`
          }`;
          if (destination.textContent !== text) destination.textContent = text;
        };
        destination.style.whiteSpace = "pre-line";
        box.appendChild(destination);
        updateDestination();
        destinationTimer = win.setInterval(updateDestination, 200);
        if (records.length > 1 && star.selectWorkspaceBookmark) {
          if (
            generation !== starGeneration ||
            !isBookmarkContextCurrent(win, context)
          ) return;
          const label = document.createElementNS(XHTML, "label");
          label.textContent = tr("editCopy", "Bookmark to edit");
          const select = document.createElementNS(
            XHTML,
            "select",
          ) as HTMLSelectElement;
          select.style.cssText =
            "width:100%;font:inherit;color:inherit;background:Field";
          for (const record of records) {
            const option = document.createElementNS(
              XHTML,
              "option",
            ) as HTMLOptionElement;
            option.value = record.guid;
            option.textContent = `${record.title || record.url} — ${
              folderTitle(record.parentGuid)
            }`;
            select.appendChild(option);
          }
          select.value = star.selectedWorkspaceBookmarkGuid ??
            win.gEditItemOverlay?.bookmarkState?.guid ??
            records[0].guid;
          select.addEventListener("change", () => {
            if (isBookmarkContextCurrent(win, context)) {
              void star.selectWorkspaceBookmark?.(select.value).then(() =>
                showStarControls()
              );
            }
          });
          label.appendChild(select);
          box.appendChild(label);
        }
        if (context.enabled && context.scopeEnabled && context.workspaceId) {
          const save = document.createElementNS(
            XHTML,
            "button",
          ) as HTMLButtonElement;
          save.type = "button";
          save.disabled = workspaceSaveInProgress;
          save.id = "floorp-save-bookmark-for-workspace";
          save.textContent = tr("saveForWorkspace", "Save for this workspace");
          save.style.cssText = "font:inherit;padding:6px;cursor:pointer";
          save.addEventListener("click", async () => {
            if (workspaceSaveInProgress) return;
            workspaceSaveInProgress = true;
            save.disabled = true;
            let folder:
              | { guid: string; created: boolean; workspaceId: string }
              | null = null;
            const bookmarkState = win.gEditItemOverlay?.bookmarkState;
            try {
              if (
                !isBookmarkContextCurrent(win, context) || !bookmarkState ||
                star.panel.state !== "open"
              ) return;
              folder = await service.getOrCreateWorkspaceFolder(win);
              const current = service.getContext(win);
              if (
                win.closed || current.workspaceId !== context.workspaceId ||
                !current.enabled || !current.scopeEnabled ||
                current.windowGeneration !== context.windowGeneration ||
                current.windowId !== context.windowId
              ) throw new Error("Workspace changed while saving");
              if (
                win.gEditItemOverlay?.bookmarkState !== bookmarkState ||
                star.panel.state !== "open"
              ) {
                throw new Error(
                  "Bookmark editor changed while choosing its destination",
                );
              }
              await bookmarkState._parentGuidChanged(folder.guid);
              if (
                win.gEditItemOverlay?.bookmarkState !== bookmarkState ||
                star.panel.state !== "open" ||
                service.getContext(win).windowGeneration !==
                  context.windowGeneration
              ) {
                throw new Error("Bookmark editor changed while saving");
              }
              star.panel.hidePopup();
            } catch (error) {
              if (folder) await service.releaseCreatedWorkspaceFolder?.(folder);
              console.error(
                "[WorkspaceBookmarks] Workspace save failed",
                error,
              );
              if (!win.closed) {
                Services.prompt.alert(
                  win as unknown as mozIDOMWindowProxy,
                  tr("title", "Workspace bookmark visibility"),
                  tr(
                    "saveError",
                    "The bookmark could not be saved for this workspace. Please choose the destination again.",
                  ),
                );
              }
            } finally {
              workspaceSaveInProgress = false;
              save.disabled = false;
              const currentButton = document.getElementById(
                "floorp-save-bookmark-for-workspace",
              ) as HTMLButtonElement | null;
              if (currentButton) currentButton.disabled = false;
            }
          });
          box.appendChild(save);
        }
        (document.getElementById("editBookmarkPanelContent") ?? starPanel)
          .appendChild(box);
      } catch (error) {
        console.error("[WorkspaceBookmarks] Bookmark controls failed", error);
      }
    };
    const onStarShown = (event: Event) => {
      const target = event.target as Element | null;
      if (target?.id !== "editBookmarkPanel") return;
      starPanel = target as XULPopupElement;
      void showStarControls();
    };
    const onStarHidden = (event: Event) => {
      if ((event.target as Element | null)?.id === "editBookmarkPanel") {
        removeStarControls();
      }
    };
    document.addEventListener("popupshown", onStarShown);
    document.addEventListener("popuphidden", onStarHidden);
    const observer = {
      observe: () => {
        win.gURLBar?.controller?.cancelQuery();
        win.gURLBar?.view?.close();
        if (starPanel?.state === "open") void showStarControls();
      },
    };
    Services.obs.addObserver(observer, BOOKMARK_SCOPES_CHANGED);
    onCleanup(() => {
      disposed = true;
      Services.obs.removeObserver(managerObserver, "domwindowopened");
      menu?.removeEventListener("popupshowing", showContextMenu);
      menu?.removeEventListener("popuphidden", hideContextMenu);
      scopeItem.removeEventListener("command", editFolderScope);
      scopeItem.remove();
      document.removeEventListener("popupshown", onStarShown);
      document.removeEventListener("popuphidden", onStarHidden);
      removeStarControls();
      Services.obs.removeObserver(observer, BOOKMARK_SCOPES_CHANGED);
      service.clearWindowContext(win);
      if (win.floorpWorkspaceBookmarksUI === ui) {
        delete win.floorpWorkspaceBookmarksUI;
      }
    });
  }
}
