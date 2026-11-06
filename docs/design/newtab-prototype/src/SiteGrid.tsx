import React, { useEffect, useRef, useState } from "react";
import {
  ArrowLeft,
  ArrowRight,
  ArrowUp,
  BookmarkPlus,
  Check,
  ChevronRight,
  EllipsisVertical,
  Folder,
  FolderPlus,
  Pencil,
  Plus,
  Trash2,
  X,
} from "lucide-react";
import { groups, sponsors } from "./data.ts";
import {
  getDialEntries,
  getFolderPath,
  moveDialEntry,
  nextDialOrder,
  reorderDialEntry,
} from "./dial-tree.ts";
import { usePrototype } from "./store.tsx";
import { BrandIcon, Button, EmptyState, IconButton } from "./ui.tsx";
import type { DialEntry, DialFolder, GroupId, Site, Sponsor } from "./types.ts";
import "./dial-folders.css";

export function GroupSelect() {
  const { groupId, setGroupId } = usePrototype();
  return (
    <label className="group-select">
      <Folder size={18} aria-hidden="true" />
      <span className="sr-only">表示するグループ</span>
      <select
        value={groupId}
        aria-label="表示するグループ"
        onChange={(event) => setGroupId(event.target.value as GroupId)}
      >
        {groups.map((group) => (
          <option key={group.id} value={group.id}>
            {group.title}
          </option>
        ))}
      </select>
    </label>
  );
}

function useTileMenu() {
  const [menuOpen, setMenuOpen] = useState(false);
  const menuRef = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (!menuOpen) return;
    const close = (event: PointerEvent) => {
      if (!menuRef.current?.contains(event.target as Node)) setMenuOpen(false);
    };
    const key = (event: KeyboardEvent) => {
      if (event.key === "Escape") {
        setMenuOpen(false);
        menuRef.current?.querySelector("button")?.focus();
      }
    };
    window.addEventListener("pointerdown", close);
    window.addEventListener("keydown", key);
    return () => {
      window.removeEventListener("pointerdown", close);
      window.removeEventListener("keydown", key);
    };
  }, [menuOpen]);
  return { menuOpen, setMenuOpen, menuRef };
}

function SiteTile({
  site,
  sponsor,
  destinationFolderId,
  onMove,
}: {
  site: Site | Sponsor;
  sponsor?: boolean;
  destinationFolderId: string | null;
  onMove: (direction: -1 | 1) => void;
}) {
  const { data, setData, groupId, setDialog, showToast } = usePrototype();
  const { menuOpen, setMenuOpen, menuRef } = useTileMenu();
  const dismiss = () => {
    if (sponsor) {
      setData((previous) => ({
        ...previous,
        dismissedSponsors: [...previous.dismissedSponsors, site.id],
      }));
      showToast("スポンサーサイトを非表示にしました");
    } else {
      setData((previous) => ({
        ...previous,
        sites: previous.sites.filter((item) => item.id !== site.id),
      }));
      showToast("サイトを削除しました");
    }
    setMenuOpen(false);
  };
  const saveSponsor = () => {
    if (
      data.sites.some(
        (item) =>
          item.url === site.url &&
          item.groupId === groupId &&
          (item.folderId ?? null) === destinationFolderId,
      )
    ) {
      showToast("このフォルダーに追加済みです");
      setMenuOpen(false);
      return;
    }
    const saved: Site = {
      ...site,
      id: crypto.randomUUID(),
      groupId,
      folderId: destinationFolderId,
      order: nextDialOrder(data, groupId, destinationFolderId),
      sponsorId: site.id,
    };
    setData((previous) => ({ ...previous, sites: [...previous.sites, saved] }));
    showToast("スピードダイアルに追加しました");
    setMenuOpen(false);
  };
  return (
    <article
      className={`site-tile ${sponsor ? "sponsor-tile" : ""}`}
      data-site-id={site.id}
      data-sponsored={sponsor || undefined}
      data-menu-open={menuOpen || undefined}
    >
      <a
        href={site.url}
        target="_blank"
        rel="noopener noreferrer"
        className="site-link"
        aria-label={`${site.title}を開く`}
      >
        <span className="site-icon">
          <BrandIcon name={site.icon} size={48} />
        </span>
        <span className="site-caption">
          {sponsor && <span className="sponsor-label">スポンサー</span>}
          <strong>{site.title}</strong>
          <span className="site-domain">
            {new URL(site.url).hostname.replace(/^www\./, "")}
          </span>
        </span>
      </a>
      <div className="site-menu-anchor" ref={menuRef}>
        <IconButton
          className="tile-menu-button"
          label={`${site.title}のメニュー`}
          onClick={() => setMenuOpen(!menuOpen)}
          aria-expanded={menuOpen}
        >
          <EllipsisVertical size={18} />
        </IconButton>
        {menuOpen && (
          <div className="site-menu" aria-label={`${site.title}の操作`}>
            {sponsor ? (
              <button onClick={saveSponsor}>
                <BookmarkPlus size={17} />
                スピードダイアルに追加
              </button>
            ) : (
              <>
                <button
                  onClick={() => {
                    menuRef.current?.querySelector("button")?.focus();
                    setDialog({ kind: "site", site: site as Site });
                    setMenuOpen(false);
                  }}
                >
                  <Pencil size={16} />
                  編集
                </button>
                <button
                  onClick={() => {
                    onMove(-1);
                    setMenuOpen(false);
                  }}
                >
                  <ArrowLeft size={16} />
                  前へ移動
                </button>
                <button
                  onClick={() => {
                    onMove(1);
                    setMenuOpen(false);
                  }}
                >
                  <ArrowRight size={16} />
                  後ろへ移動
                </button>
              </>
            )}
            <button
              className={sponsor ? "" : "is-destructive"}
              onClick={dismiss}
            >
              {sponsor ? <X size={17} /> : <Trash2 size={16} />}
              {sponsor ? "表示しない" : "削除"}
            </button>
          </div>
        )}
      </div>
    </article>
  );
}

function FolderTile({
  folder,
  onMove,
}: {
  folder: DialFolder;
  onMove: (direction: -1 | 1) => void;
}) {
  const { data, openFolder, setDialog } = usePrototype();
  const { menuOpen, setMenuOpen, menuRef } = useTileMenu();
  const count = getDialEntries(data, folder.groupId, folder.id).length;
  return (
    <article
      className="site-tile folder-tile"
      data-folder-id={folder.id}
      data-menu-open={menuOpen || undefined}
    >
      <button
        type="button"
        className="site-link folder-open"
        onClick={() => openFolder(folder.id)}
        aria-label={`${folder.title}フォルダーを開く`}
      >
        <span className="site-icon folder-icon">
          <Folder size={48} strokeWidth={1.5} aria-hidden="true" />
        </span>
        <span className="site-caption">
          <strong>{folder.title}</strong>
          <span className="folder-item-count">{count} 件の項目</span>
        </span>
      </button>
      <div className="site-menu-anchor" ref={menuRef}>
        <IconButton
          className="tile-menu-button"
          label={`${folder.title}フォルダーのメニュー`}
          onClick={() => setMenuOpen(!menuOpen)}
          aria-expanded={menuOpen}
        >
          <EllipsisVertical size={18} />
        </IconButton>
        {menuOpen && (
          <div
            className="site-menu"
            aria-label={`${folder.title}フォルダーの操作`}
          >
            <button
              onClick={() => {
                menuRef.current?.querySelector("button")?.focus();
                setDialog({ kind: "edit-folder", folder });
                setMenuOpen(false);
              }}
            >
              <Pencil size={16} />
              名前・移動先を変更
            </button>
            <button
              onClick={() => {
                onMove(-1);
                setMenuOpen(false);
              }}
            >
              <ArrowLeft size={16} />
              前へ移動
            </button>
            <button
              onClick={() => {
                onMove(1);
                setMenuOpen(false);
              }}
            >
              <ArrowRight size={16} />
              後ろへ移動
            </button>
            <button
              className="is-destructive"
              onClick={() => {
                menuRef.current?.querySelector("button")?.focus();
                setDialog({ kind: "delete-folder", folder });
                setMenuOpen(false);
              }}
            >
              <Trash2 size={16} />
              削除
            </button>
          </div>
        )}
      </div>
    </article>
  );
}

export function SiteGrid({ compact = false }: { compact?: boolean }) {
  const { data, setData, groupId, folderId, setDialog, showToast } =
    usePrototype();
  const [dropTarget, setDropTarget] = useState<string | null>(null);
  const currentFolderId = compact ? null : folderId;
  const entries = getDialEntries(data, groupId, currentFolderId);
  const visibleEntries = compact ? entries.slice(0, 6) : entries;
  const visibleSponsors = data.settings.showSponsors
    ? sponsors.filter((site) => !data.dismissedSponsors.includes(site.id))
    : [];
  const move = (id: string, direction: -1 | 1) => {
    const current = entries.findIndex((entry) => entry.id === id);
    const target = entries[current + direction];
    if (!target) return;
    setData((previous) =>
      reorderDialEntry(previous, groupId, currentFolderId, id, target.id),
    );
    showToast("並び順を変更しました");
  };
  const drop = (event: React.DragEvent, target: DialEntry) => {
    event.preventDefault();
    event.stopPropagation();
    setDropTarget(null);
    if (compact) return;
    const id = event.dataTransfer.getData("text/plain");
    if (!id || id === target.id) return;
    const sourceType = data.folders.some((folder) => folder.id === id)
      ? "folder"
      : data.sites.some((site) => site.id === id)
        ? "site"
        : null;
    if (!sourceType) return;
    try {
      if (target.type === "folder") {
        setData(moveDialEntry(data, sourceType, id, groupId, target.folder.id));
        showToast(`「${target.folder.title}」に移動しました`);
      } else {
        setData((previous) =>
          reorderDialEntry(previous, groupId, currentFolderId, id, target.id),
        );
        showToast("並び順を変更しました");
      }
    } catch (cause) {
      showToast(
        cause instanceof Error ? cause.message : "移動できませんでした",
      );
    }
  };
  return (
    <>
      {entries.length === 0 && (
        <EmptyState
          title="サイトやフォルダーを追加しましょう"
          description="この場所によく使うサイトをまとめられます。"
        >
          <div className="dial-empty-actions">
            <Button
              onClick={() =>
                setDialog({ kind: "site", initialFolderId: currentFolderId })
              }
            >
              <Plus size={17} />
              サイトを追加
            </Button>
            <Button
              variant="secondary"
              onClick={() =>
                setDialog({
                  kind: "edit-folder",
                  initialFolderId: currentFolderId,
                })
              }
            >
              <FolderPlus size={17} />
              フォルダーを作成
            </Button>
          </div>
        </EmptyState>
      )}
      <div
        className={`site-grid ${compact ? "site-grid-home" : "site-grid-full"}`}
        style={
          { "--dial-columns": data.settings.columns } as React.CSSProperties
        }
        data-size={data.settings.tileSize}
      >
        {visibleEntries.map((entry) => (
          <div
            key={entry.id}
            className="draggable-tile"
            data-drop-target={dropTarget === entry.id || undefined}
            draggable={!compact}
            onDragStart={(event) => {
              event.dataTransfer.setData("text/plain", entry.id);
              event.dataTransfer.effectAllowed = "move";
            }}
            onDragEnd={() => setDropTarget(null)}
            onDragOver={(event) => {
              if (!compact) {
                event.preventDefault();
                event.dataTransfer.dropEffect = "move";
                setDropTarget(entry.id);
              }
            }}
            onDragLeave={(event) => {
              if (
                !event.currentTarget.contains(
                  event.relatedTarget as Node | null,
                )
              )
                setDropTarget(null);
            }}
            onDrop={(event) => drop(event, entry)}
          >
            {entry.type === "folder" ? (
              <FolderTile
                folder={entry.folder}
                onMove={(direction) => move(entry.id, direction)}
              />
            ) : (
              <SiteTile
                site={entry.site}
                destinationFolderId={currentFolderId}
                onMove={(direction) => move(entry.id, direction)}
              />
            )}
          </div>
        ))}
        {visibleSponsors.map((site) => (
          <div key={site.id} className="draggable-tile">
            <SiteTile
              site={site}
              sponsor
              destinationFolderId={currentFolderId}
              onMove={() => {}}
            />
          </div>
        ))}
      </div>
    </>
  );
}

function FolderBreadcrumbs() {
  const { data, groupId, folderId, openFolder } = usePrototype();
  const path = getFolderPath(data.folders, folderId);
  const root = groups.find((group) => group.id === groupId);
  const parentId = path.length > 1 ? path[path.length - 2].id : null;
  return (
    <div className="dial-folder-navigation">
      <IconButton
        label="一つ上のフォルダーへ"
        onClick={() => openFolder(parentId)}
        disabled={!folderId}
      >
        <ArrowUp size={19} />
      </IconButton>
      <nav
        id="dial-location"
        tabIndex={-1}
        className="dial-breadcrumbs"
        aria-label="ブックマークフォルダーの階層"
      >
        <ol>
          <li>
            {folderId ? (
              <button onClick={() => openFolder(null)}>{root?.title}</button>
            ) : (
              <span aria-current="page">{root?.title}</span>
            )}
          </li>
          {path.map((folder, index) => (
            <li key={folder.id}>
              <ChevronRight size={15} aria-hidden="true" />
              {index === path.length - 1 ? (
                <span aria-current="page">{folder.title}</span>
              ) : (
                <button onClick={() => openFolder(folder.id)}>
                  {folder.title}
                </button>
              )}
            </li>
          ))}
        </ol>
      </nav>
    </div>
  );
}

export function Dial() {
  const {
    data,
    navigate,
    groupId,
    setGroupId,
    workspaceId,
    folderId,
    setDialog,
  } = usePrototype();
  return (
    <main id="main-content" className="dial-page page-container">
      <div className="dial-heading">
        <h1>スピードダイアル</h1>
        <p>ブックマークから、いつもの場所へ。</p>
      </div>
      <div className="dial-toolbar">
        <div
          className="group-tabs"
          role="tablist"
          aria-label="ブックマークグループ"
          onKeyDown={(event) => {
            const current = groups.findIndex((group) => group.id === groupId);
            const next =
              event.key === "ArrowRight"
                ? (current + 1) % groups.length
                : event.key === "ArrowLeft"
                  ? (current + groups.length - 1) % groups.length
                  : event.key === "Home"
                    ? 0
                    : event.key === "End"
                      ? groups.length - 1
                      : -1;
            if (next >= 0) {
              event.preventDefault();
              setGroupId(groups[next].id);
              document.getElementById(`group-tab-${groups[next].id}`)?.focus();
            }
          }}
        >
          {groups.map((group) => (
            <button
              key={group.id}
              id={`group-tab-${group.id}`}
              role="tab"
              aria-controls="dial-site-panel"
              aria-selected={groupId === group.id}
              tabIndex={groupId === group.id ? 0 : -1}
              onClick={() => setGroupId(group.id)}
            >
              <Folder size={19} />
              {group.title}
            </button>
          ))}
        </div>
        <div className="dial-actions">
          <Button
            onClick={() =>
              setDialog({ kind: "site", initialFolderId: folderId })
            }
          >
            <Plus size={19} />
            サイトを追加
          </Button>
          <Button
            variant="secondary"
            onClick={() =>
              setDialog({ kind: "edit-folder", initialFolderId: folderId })
            }
          >
            <FolderPlus size={19} />
            フォルダーを作成
          </Button>
        </div>
      </div>
      <div className="group-assignment-note">
        {groupId === data.assignments[workspaceId] ? (
          <>
            <Check size={15} />
            このワークスペースに割り当てたグループ
          </>
        ) : (
          <>
            <Folder size={15} />
            グループを一時的に表示しています
          </>
        )}
        <button onClick={() => navigate("hub", "dial")}>Hub で変更</button>
      </div>
      <FolderBreadcrumbs />
      <div
        id="dial-site-panel"
        role="tabpanel"
        aria-labelledby={`group-tab-${groupId}`}
      >
        <SiteGrid />
      </div>
    </main>
  );
}
