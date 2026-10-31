import React, { useEffect, useRef, useState } from "react";
import { ExternalLink, Folder, X } from "lucide-react";
import { groups, initialState } from "./data.ts";
import { usePrototype } from "./store.tsx";
import { BrandIcon, Button, IconButton } from "./ui.tsx";
import { Button as SharedButton } from "../../../../libs/ui/button.tsx";
import {
  destinationValue,
  folderDestinationOptions,
  getDialEntries,
  getFolderDescendantIds,
  getFolderPath,
  moveDialEntry,
  nextDialOrder,
  removeDialFolder,
  resolveDestination,
} from "./dial-tree.ts";
import type { DialFolder, Site } from "./types.ts";
import type { DialogState } from "./context-types.ts";
import "./dialog-folder.css";

function normaliseUrl(value: string): string {
  const url = new URL(/^[a-z]+:/i.test(value) ? value : `https://${value}`);
  if (url.protocol !== "http:" && url.protocol !== "https:")
    throw new Error("HTTP または HTTPS の URL を入力してください。");
  return url.href;
}

function ActiveDialog({ dialog }: { dialog: NonNullable<DialogState> }) {
  const { data, setData, groupId, folderId, setDialog, showToast } =
    usePrototype();
  const nativeDialog = useRef<HTMLDialogElement>(null);
  const editing = dialog.kind === "site" ? dialog.site : undefined;
  const editingFolder =
    dialog.kind === "edit-folder" ? dialog.folder : undefined;
  const initialGroup =
    editing?.groupId ??
    editingFolder?.groupId ??
    (dialog.kind === "site" ? dialog.initialGroup : undefined) ??
    groupId;
  const initialFolder = editing
    ? (editing.folderId ?? null)
    : editingFolder
      ? editingFolder.parentId
      : (dialog.kind === "site" || dialog.kind === "edit-folder") &&
          dialog.initialFolderId !== undefined
        ? dialog.initialFolderId
        : folderId;
  const [title, setTitle] = useState(
    editing?.title ?? editingFolder?.title ?? "",
  );
  const [url, setUrl] = useState(editing?.url ?? "");
  const [destination, setDestination] = useState(() =>
    destinationValue(initialGroup, initialFolder),
  );
  const [error, setError] = useState("");
  const close = () => setDialog(null);
  const destinationOptions = folderDestinationOptions(data, editingFolder?.id);

  useEffect(() => {
    const element = nativeDialog.current;
    const returnFocus = document.activeElement;
    element?.showModal();
    const firstInput = element?.querySelector<HTMLInputElement>(
      "input:not([disabled])",
    );
    const cancel = element?.querySelector<HTMLButtonElement>(
      "button[data-dialog-cancel]",
    );
    (firstInput ?? cancel)?.focus();
    return () => {
      element?.close();
      if (returnFocus instanceof HTMLElement && returnFocus.isConnected) {
        returnFocus.focus();
      } else {
        requestAnimationFrame(() => {
          const target =
            document.getElementById("dial-location") ??
            document.getElementById("main-content");
          if (target instanceof HTMLElement) {
            target.tabIndex = -1;
            target.focus();
          }
        });
      }
    };
  }, []);

  const submit = (event: React.FormEvent) => {
    event.preventDefault();
    try {
      if (dialog.kind === "edit-folder") {
        const name = title.trim();
        if (!name) throw new Error("フォルダー名を入力してください。");
        const target = resolveDestination(data, destination);
        if (editingFolder) {
          const moved = moveDialEntry(
            data,
            "folder",
            editingFolder.id,
            target.groupId,
            target.folderId,
          );
          setData({
            ...moved,
            folders: moved.folders.map((folder) =>
              folder.id === editingFolder.id
                ? { ...folder, title: name }
                : folder,
            ),
          });
        } else {
          const folder: DialFolder = {
            id: crypto.randomUUID(),
            title: name,
            groupId: target.groupId,
            parentId: target.folderId,
            order: nextDialOrder(data, target.groupId, target.folderId),
          };
          setData((previous) => ({
            ...previous,
            folders: [...previous.folders, folder],
          }));
        }
        showToast(
          editingFolder
            ? "フォルダーを更新しました"
            : "フォルダーを作成しました",
        );
        close();
        return;
      }
      const safeUrl = normaliseUrl(url.trim());
      const name = title.trim() || new URL(safeUrl).hostname;
      if (dialog.kind === "feed") {
        if (data.feeds.some((feed) => feed.url === safeUrl)) {
          setError("このフィードは追加済みです。");
          return;
        }
        setData((previous) => ({
          ...previous,
          feeds: [
            ...previous.feeds,
            { id: crypto.randomUUID(), title: name, url: safeUrl, icon: "rss" },
          ],
        }));
        showToast("フィードを追加しました");
      } else if (dialog.kind === "site") {
        const target = resolveDestination(data, destination);
        const moved = editing
          ? moveDialEntry(
              data,
              "site",
              editing.id,
              target.groupId,
              target.folderId,
            )
          : data;
        const movedSite = editing
          ? moved.sites.find((site) => site.id === editing.id)
          : undefined;
        const site: Site = {
          id: editing?.id ?? crypto.randomUUID(),
          title: name,
          url: safeUrl,
          icon:
            editing?.icon ??
            (safeUrl.includes("github.com")
              ? "github"
              : safeUrl.includes("floorp.app")
                ? "floorp"
                : "globe"),
          groupId: target.groupId,
          folderId: target.folderId,
          order:
            movedSite?.order ??
            nextDialOrder(data, target.groupId, target.folderId),
          ...(editing?.sponsorId ? { sponsorId: editing.sponsorId } : {}),
        };
        setData({
          ...moved,
          sites: editing
            ? moved.sites.map((item) => (item.id === editing.id ? site : item))
            : [...moved.sites, site],
        });
        showToast(editing ? "サイトを更新しました" : "サイトを追加しました");
      }
      close();
    } catch (cause) {
      setError(
        cause instanceof Error ? cause.message : "入力内容を確認してください。",
      );
    }
  };

  const heading =
    dialog.kind === "feed"
      ? "フィードを追加"
      : dialog.kind === "folder"
        ? "ブックマークフォルダー"
        : dialog.kind === "session"
          ? "前回のつづき"
          : dialog.kind === "delete-folder"
            ? "フォルダーを削除"
            : dialog.kind === "edit-folder"
              ? editingFolder
                ? "フォルダーを編集"
                : "フォルダーを作成"
              : editing
                ? "サイトを編集"
                : "サイトを追加";
  const folderSites =
    dialog.kind === "session"
      ? initialState.sites
          .filter((site) => site.groupId === "development")
          .slice(0, 3)
      : getDialEntries(data, groupId, folderId).flatMap((entry) =>
          entry.type === "site" ? [entry.site] : [],
        );
  const deletedIds =
    dialog.kind === "delete-folder"
      ? getFolderDescendantIds(data.folders, dialog.folder.id)
      : new Set<string>();
  const deletedSites = data.sites.filter(
    (site) => site.folderId && deletedIds.has(site.folderId),
  ).length;
  const deletedSubfolders = Math.max(0, deletedIds.size - 1);

  return (
    <dialog
      ref={nativeDialog}
      className="app-dialog"
      aria-labelledby="dialog-title"
      onCancel={close}
      onClick={(event) => {
        if (event.target === event.currentTarget) close();
      }}
    >
      <div className="dialog-header">
        <h2 id="dialog-title">{heading}</h2>
        <IconButton label="閉じる" onClick={close}>
          <X size={20} />
        </IconButton>
      </div>
      {dialog.kind === "delete-folder" ? (
        <>
          <p className="folder-delete-title">
            「{dialog.folder.title}」を削除しますか？
          </p>
          <p className="folder-delete-summary">
            {deletedSites || deletedSubfolders
              ? `中にあるサイト ${deletedSites} 件とサブフォルダー ${deletedSubfolders} 件も削除します。`
              : "このフォルダーは空です。"}
          </p>
          <div className="dialog-actions">
            <Button variant="secondary" onClick={close} data-dialog-cancel>
              キャンセル
            </Button>
            <SharedButton
              variant="danger"
              className="ui-button"
              onClick={() => {
                setData((previous) =>
                  removeDialFolder(previous, dialog.folder.id),
                );
                showToast("フォルダーを削除しました");
                close();
              }}
            >
              削除
            </SharedButton>
          </div>
        </>
      ) : dialog.kind === "folder" || dialog.kind === "session" ? (
        <>
          <p className="dialog-subtitle">
            {dialog.kind === "folder" ? (
              <>
                <Folder size={17} />
                スピードダイアル /{" "}
                {groups.find((group) => group.id === groupId)?.title}
                {getFolderPath(data.folders, folderId)
                  .map((folder) => ` / ${folder.title}`)
                  .join("")}
              </>
            ) : (
              "ブラウザー調査・3 タブ"
            )}
          </p>
          <div className="folder-list">
            {folderSites.length ? (
              folderSites.map((site) => (
                <a
                  href={site.url}
                  target="_blank"
                  rel="noopener noreferrer"
                  key={site.id}
                >
                  <BrandIcon name={site.icon} size={24} />
                  <span>
                    <strong>{site.title}</strong>
                    <small>{new URL(site.url).hostname}</small>
                  </span>
                  <ExternalLink size={16} />
                </a>
              ))
            ) : (
              <p>このフォルダーにサイトはありません。</p>
            )}
          </div>
          <div className="dialog-actions">
            <Button variant="secondary" onClick={close} data-dialog-cancel>
              閉じる
            </Button>
          </div>
        </>
      ) : (
        <form onSubmit={submit}>
          {dialog.kind !== "edit-folder" && (
            <label className="dialog-field">
              <span>{dialog.kind === "feed" ? "フィードの URL" : "URL"}</span>
              <input
                value={url}
                required
                onChange={(event) => setUrl(event.target.value)}
                placeholder={
                  dialog.kind === "feed"
                    ? "https://example.com/feed.xml"
                    : "https://example.com"
                }
                inputMode="url"
                autoComplete="url"
              />
            </label>
          )}
          <label className="dialog-field">
            <span>
              {dialog.kind === "edit-folder"
                ? "フォルダー名"
                : dialog.kind === "feed"
                  ? "表示名"
                  : "サイト名"}
            </span>
            <input
              value={title}
              required={dialog.kind === "edit-folder"}
              onChange={(event) => setTitle(event.target.value)}
              placeholder={
                dialog.kind === "edit-folder"
                  ? "例：リファレンス"
                  : dialog.kind === "feed"
                    ? "例：デザインノート"
                    : "例：ドキュメント"
              }
              maxLength={100}
            />
          </label>
          {(dialog.kind === "site" || dialog.kind === "edit-folder") && (
            <label className="dialog-field">
              <span>保存先</span>
              <select
                value={destination}
                onChange={(event) => setDestination(event.target.value)}
              >
                {destinationOptions.map((option) => (
                  <option key={option.value} value={option.value}>
                    {option.label}
                  </option>
                ))}
              </select>
            </label>
          )}
          {error && (
            <p className="form-error" role="alert">
              {error}
            </p>
          )}
          <div className="dialog-actions">
            <Button variant="secondary" onClick={close} data-dialog-cancel>
              キャンセル
            </Button>
            <Button type="submit">
              {editing || editingFolder
                ? "保存"
                : dialog.kind === "edit-folder"
                  ? "作成"
                  : "追加"}
            </Button>
          </div>
        </form>
      )}
    </dialog>
  );
}

export function Dialogs() {
  const { dialog } = usePrototype();
  if (!dialog) return null;
  const key =
    dialog.kind === "site"
      ? (dialog.site?.id ?? "new-site")
      : dialog.kind === "edit-folder"
        ? `edit-${dialog.folder?.id ?? "new-folder"}`
        : dialog.kind === "delete-folder"
          ? `delete-${dialog.folder.id}`
          : dialog.kind;
  return <ActiveDialog dialog={dialog} key={key} />;
}
