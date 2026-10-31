// SPDX-License-Identifier: MPL-2.0
import type { BookmarkGridProps, TileProps } from "./types.ts";
import { useEffect, useRef, useState } from "react";
import {
  ArrowLeft,
  ArrowRight,
  ArrowUp,
  BookmarkPlus,
  ChevronRight,
  EllipsisVertical,
  Folder,
  FolderPlus,
  Pencil,
  Plus,
  Trash2,
  X,
} from "lucide-react";
import { useTranslation } from "react-i18next";
import { newTabClient } from "../../../../../libs/newtab/client.ts";
import type {
  BookmarkNode,
  PinnedSite,
} from "../../../../../libs/newtab/types.ts";
import { selectHistorySuggestions } from "./suggestions.ts";
import { messageOf, useNewTab } from "./state.tsx";
import {
  Button,
  domain,
  FloatingMenu,
  IconButton,
  safeWebUrl,
  SiteIcon,
} from "./ui.tsx";

function Tile({ node, index, total, sponsor, suggestion, compact }: TileProps) {
  const { t } = useTranslation();
  const {
    snapshot,
    folder,
    groupGuid,
    openFolder,
    setDialog,
    refresh,
    notify,
  } = useNewTab();
  const [menu, setMenu] = useState(false);
  const trigger = useRef<HTMLButtonElement>(null);
  const act = async (action: () => Promise<unknown>) => {
    setMenu(false);
    trigger.current?.focus();
    try {
      await action();
      await refresh();
    } catch (cause) {
      notify(messageOf(cause));
    }
  };
  const move = (offset: number) =>
    act(async () => {
      if (!folder) return;
      const guids = folder.children.map((item) => item.guid);
      [guids[index], guids[index + offset]] = [
        guids[index + offset],
        guids[index],
      ];
      await newTabClient.call("bookmark", {
        action: "reorder",
        parentGuid: folder.guid,
        guids,
      });
    });
  const edit = () => {
    trigger.current?.focus();
    setMenu(false);
    setDialog({
      type: "bookmark",
      node,
      kind: node.type,
      parentGuid: node.parentGuid,
    });
  };
  const remove = () =>
    act(async () => {
      const summary = await newTabClient.call("deleteSummary", {
        guid: node.guid,
      });
      setDialog({ type: "delete", summary, title: node.title });
    });
  const name = node.title || domain(node.url) ||
    t("newtab.untitled", "Untitled");
  return (
    <article
      className={`nt-tile${node.type === "folder" ? " nt-folder-tile" : ""}`}
      data-bookmark-guid={sponsor || suggestion ? undefined : node.guid}
      data-history-suggestion={suggestion || undefined}
      data-menu-open={menu || undefined}
    >
      {node.type === "folder"
        ? (
          <button
            type="button"
            className="nt-tile-link"
            title={name}
            onClick={() => openFolder(node.guid)}
            aria-label={t("newtab.openFolder", {
              title: name,
              defaultValue: "Open {{title}} folder",
            })}
          >
            <span className="nt-site-icon">
              <SiteIcon folder />
            </span>
            <span className="nt-site-caption">
              <strong>{name}</strong>
              <small>
                {t("newtab.itemCount", {
                  count: node.childCount ?? 0,
                  defaultValue: "{{count}} items",
                })}
              </small>
            </span>
          </button>
        )
        : (
          <a
            className="nt-tile-link"
            href={safeWebUrl(node.url) ?? undefined}
            aria-disabled={!safeWebUrl(node.url) || undefined}
            title={`${name}\n${node.url ?? ""}`}
          >
            <span className="nt-site-icon">
              <SiteIcon url={node.url} />
            </span>
            <span className="nt-site-caption">
              {sponsor && (
                <small className="nt-sponsored">
                  {t("newtab.sponsored", "Sponsored")}
                </small>
              )}
              <strong>{name}</strong>
              {suggestion && (
                <small className="nt-suggestion-label">
                  {t("newtab.historySuggestion", "Suggested site")}
                </small>
              )}
              {!safeWebUrl(node.url) && (
                <small>
                  {t(
                    "newtab.unsupportedUrl",
                    "Use the bookmark manager to open this address.",
                  )}
                </small>
              )}
              {!compact && <small>{domain(node.url)}</small>}
            </span>
          </a>
        )}
      <div className="nt-tile-menu-anchor">
        <IconButton
          ref={trigger}
          label={t("newtab.itemMenu", {
            title: name,
            defaultValue: "{{title}} menu",
          })}
          aria-expanded={menu}
          onClick={() => setMenu(!menu)}
        >
          <EllipsisVertical size={18} />
        </IconButton>
        {menu && (
          <FloatingMenu
            anchor={trigger}
            close={() => setMenu(false)}
            label={name}
          >
            {sponsor || suggestion
              ? (
                <>
                  <button
                    type="button"
                    disabled={snapshot.readOnly}
                    onClick={() =>
                      void act(() =>
                        sponsor
                          ? newTabClient.call("saveSponsor", {
                            id: sponsor.id,
                            expectedTitle: sponsor.title,
                            expectedUrl: sponsor.url,
                            parentGuid: compact
                              ? groupGuid
                              : (folder?.guid ?? groupGuid),
                          })
                          : newTabClient.call("bookmark", {
                            action: "create",
                            type: "bookmark",
                            parentGuid: groupGuid,
                            title: name,
                            url: node.url,
                          })
                      )}
                  >
                    <BookmarkPlus size={17} />
                    {t("newtab.saveToDial", "Save to Speed Dial")}
                  </button>
                  <button
                    type="button"
                    disabled={snapshot.readOnly}
                    onClick={() => {
                      if (sponsor) {
                        void act(() =>
                          newTabClient.call("dismissSponsor", {
                            id: sponsor.id,
                          })
                        );
                      } else if (node.url) {
                        setMenu(false);
                        trigger.current?.focus();
                        setDialog({
                          type: "block",
                          url: node.url,
                          title: name,
                        });
                      }
                    }}
                  >
                    <X size={17} />
                    {t("newtab.hide", "Hide")}
                  </button>
                </>
              )
              : (
                <>
                  <button
                    type="button"
                    disabled={snapshot.readOnly}
                    onClick={edit}
                  >
                    <Pencil size={17} />
                    {t("newtab.editMove", "Edit or move")}
                  </button>
                  <button
                    type="button"
                    disabled={snapshot.readOnly || index === 0}
                    onClick={() => void move(-1)}
                  >
                    <ArrowLeft size={17} />
                    {t("newtab.movePrevious", "Move earlier")}
                  </button>
                  <button
                    type="button"
                    disabled={snapshot.readOnly || index >= total - 1}
                    onClick={() => void move(1)}
                  >
                    <ArrowRight size={17} />
                    {t("newtab.moveNext", "Move later")}
                  </button>
                  <button
                    type="button"
                    className="nt-danger"
                    disabled={snapshot.readOnly}
                    onClick={() => void remove()}
                  >
                    <Trash2 size={17} />
                    {t("newtab.delete", "Delete")}
                  </button>
                </>
              )}
          </FloatingMenu>
        )}
      </div>
    </article>
  );
}
export function GroupSelect() {
  const { snapshot, groupGuid, selectGroup } = useNewTab();
  const { t } = useTranslation();
  return (
    <label className="nt-group-select">
      <Folder size={17} aria-hidden="true" />
      <span className="nt-sr-only">{t("newtab.group", "Bookmark group")}</span>
      <select
        aria-label={t("newtab.group", "Bookmark group")}
        value={groupGuid}
        onChange={(event) => selectGroup(event.currentTarget.value)}
      >
        {snapshot.groups.map((group) => (
          <option key={group.guid} value={group.guid}>
            {group.title}
          </option>
        ))}
      </select>
    </label>
  );
}
export function BookmarkGrid({ compact = false }: BookmarkGridProps) {
  const { t } = useTranslation();
  const {
    snapshot,
    folder,
    folderLoading,
    folderError,
    groupGuid,
    epoch,
    setDialog,
    refresh,
    notify,
  } = useNewTab();
  const [dropTarget, setDropTarget] = useState<string | null>(null);
  const [history, setHistory] = useState<PinnedSite[]>([]);
  const showSuggestions = snapshot.settings.home.showHistory &&
    !snapshot.private && folder?.guid === groupGuid;
  useEffect(() => {
    let active = true;
    setHistory([]);
    if (showSuggestions) {
      void newTabClient.call("history", {}).then((items) => {
        if (active) setHistory(items);
      }).catch(
        () => {/* Bookmarks remain usable if history is unavailable. */},
      );
    }
    return () => {
      active = false;
    };
  }, [showSuggestions, groupGuid, epoch]);
  const suggestions = showSuggestions
    ? selectHistorySuggestions(
      history,
      folder?.children ?? [],
      snapshot.sponsors,
      snapshot.settings.topSites.blocked,
    ).slice(0, compact ? Math.max(0, 6 - (folder?.children.length ?? 0)) : 8)
    : [];

  const nodes = compact
    ? (folder?.children.slice(0, 6) ?? [])
    : (folder?.children ?? []);
  const drop = async (event: React.DragEvent, target: BookmarkNode) => {
    event.preventDefault();
    setDropTarget(null);
    if (compact || snapshot.readOnly || !folder) return;
    const guid = event.dataTransfer.getData("text/x-floorp-bookmark");
    if (!guid || guid === target.guid) return;
    try {
      if (target.type === "folder") {
        await newTabClient.call("bookmark", {
          action: "move",
          guid,
          parentGuid: target.guid,
        });
      } else {
        const guids = folder.children.map((node) => node.guid);
        const from = guids.indexOf(guid);
        const to = guids.indexOf(target.guid);
        if (from < 0 || to < 0) return;
        guids.splice(from, 1);
        guids.splice(to, 0, guid);
        await newTabClient.call("bookmark", {
          action: "reorder",
          parentGuid: folder.guid,
          guids,
        });
      }
      await refresh();
    } catch (cause) {
      notify(messageOf(cause));
    }
  };
  if (folderError || !groupGuid) {
    return (
      <div className="nt-empty">
        <p role="alert">
          {folderError ||
            t(
              "newtab.bookmarksUnavailable",
              "The Speed Dial folder is unavailable.",
            )}
        </p>
        <Button variant="secondary" onClick={() => void refresh()}>
          {t("newtab.retry", "Try again")}
        </Button>
        <Button
          variant="secondary"
          disabled={snapshot.readOnly}
          onClick={() =>
            void newTabClient
              .call("repairBookmarks", {})
              .then(refresh)
              .catch((cause) => notify(messageOf(cause)))}
        >
          {t("newtab.recreateFolders", "Create empty Speed Dial folders")}
        </Button>
      </div>
    );
  }
  return (
    <div aria-busy={folderLoading}>
      {folderLoading && !folder
        ? <p role="status">{t("newtab.loading", "Loading…")}</p>
        : (
          nodes.length === 0 && suggestions.length === 0 && (
            <div className="nt-empty">
              <Folder size={30} />
              <h3>
                {t("newtab.emptyFolder", "Add your first site or folder")}
              </h3>
              <p>
                {t(
                  "newtab.emptyFolderHint",
                  "Keep the places you use together here.",
                )}
              </p>
              {groupGuid && (
                <Button
                  disabled={snapshot.readOnly}
                  onClick={() =>
                    setDialog({
                      type: "bookmark",
                      kind: "bookmark",
                      parentGuid: compact
                        ? groupGuid
                        : (folder?.guid ?? groupGuid),
                    })}
                >
                  <Plus size={17} />
                  {t("newtab.addSite", "Add site")}
                </Button>
              )}
            </div>
          )
        )}
      <div
        className={`nt-site-grid${compact ? " is-compact" : ""}`}
        style={{
          "--nt-columns": snapshot.settings.speedDial.columns,
        } as React.CSSProperties}
        data-tile-size={snapshot.settings.speedDial.tileSize}
      >
        {nodes.map((node, index) => (
          <div
            className="nt-draggable"
            key={node.guid}
            draggable={!compact && !snapshot.readOnly}
            data-drop-target={dropTarget === node.guid || undefined}
            onDragStart={(event) => {
              event.dataTransfer.setData("text/x-floorp-bookmark", node.guid);
              event.dataTransfer.effectAllowed = "move";
            }}
            onDragEnd={() => setDropTarget(null)}
            onDragOver={(event) => {
              if (!compact && !snapshot.readOnly) {
                event.preventDefault();
                setDropTarget(node.guid);
              }
            }}
            onDragLeave={(event) => {
              if (
                !event.currentTarget.contains(
                  event.relatedTarget as Node | null,
                )
              ) {
                setDropTarget(null);
              }
            }}
            onDrop={(event) => void drop(event, node)}
          >
            <Tile
              node={node}
              index={index}
              total={folder?.children.length ?? 0}
              compact={compact}
            />
          </div>
        ))}
        {suggestions.map((site, index) => (
          <div className="nt-draggable" key={`history-${site.url}`}>
            <Tile
              node={{
                guid: `history-${site.url}`,
                parentGuid: groupGuid,
                index,
                type: "bookmark",
                title: site.title,
                url: site.url,
              }}
              index={index}
              total={suggestions.length}
              compact={compact}
              suggestion
            />
          </div>
        ))}
        {snapshot.sponsors.slice(0, 2).map((sponsor) => (
          <div className="nt-draggable" key={`sponsor-${sponsor.id}`}>
            <Tile
              node={{
                guid: sponsor.id,
                parentGuid: groupGuid,
                index: 0,
                type: "bookmark",
                title: sponsor.title,
                url: sponsor.url,
              }}
              index={0}
              total={0}
              sponsor={sponsor}
              compact={compact}
            />
          </div>
        ))}
      </div>
    </div>
  );
}
export function Dial() {
  const { t } = useTranslation();
  const {
    snapshot,
    groupGuid,
    selectGroup,
    folder,
    folderGuid,
    openFolder,
    setDialog,
  } = useNewTab();
  const currentGroup = snapshot.groups.find(
    (group) => group.guid === groupGuid,
  );
  const ancestors = folder?.ancestors.filter(
    (node) =>
      node.guid !== snapshot.settings.speedDial.rootGuid &&
      node.guid !== folder.guid,
  ) ?? [];
  const path = ancestors.some((node) => node.guid === groupGuid) ||
      folder?.guid === groupGuid
    ? ancestors
    : currentGroup
    ? [currentGroup, ...ancestors]
    : ancestors;
  const parent = folder?.guid === groupGuid
    ? groupGuid
    : (ancestors.at(-1)?.guid ?? groupGuid);
  return (
    <main
      id="main-content"
      className="nt-dial nt-container"
      tabIndex={-1}
    >
      <div className="nt-dial-controls nt-surface">
        <header className="nt-page-heading">
          <h1>{t("newtab.dial", "Speed Dial")}</h1>
          <p>{t("newtab.dialHint", "Your bookmarks, in familiar places.")}</p>
        </header>
        <div className="nt-dial-toolbar">
          <div
            className="nt-group-tabs"
            role="tablist"
            aria-label={t("newtab.groups", "Bookmark groups")}
            onKeyDown={(event) => {
              const index = snapshot.groups.findIndex(
                (group) => group.guid === groupGuid,
              );
              const next = event.key === "ArrowRight"
                ? (index + 1) % snapshot.groups.length
                : event.key === "ArrowLeft"
                ? (index - 1 + snapshot.groups.length) %
                  snapshot.groups.length
                : event.key === "Home"
                ? 0
                : event.key === "End"
                ? snapshot.groups.length - 1
                : -1;
              if (next >= 0) {
                event.preventDefault();
                const group = snapshot.groups[next];
                selectGroup(group.guid);
                document.getElementById(`group-${group.guid}`)?.focus();
              }
            }}
          >
            {snapshot.groups.map((group) => (
              <button
                type="button"
                id={`group-${group.guid}`}
                role="tab"
                key={group.guid}
                aria-controls="nt-folder-panel"
                aria-selected={group.guid === groupGuid}
                title={group.title}
                tabIndex={group.guid === groupGuid ? 0 : -1}
                onClick={() => selectGroup(group.guid)}
              >
                <Folder size={18} />
                <span>{group.title}</span>
              </button>
            ))}
          </div>
          <div className="nt-dial-actions">
            <Button
              disabled={snapshot.readOnly || !folderGuid}
              onClick={() =>
                setDialog({
                  type: "bookmark",
                  kind: "bookmark",
                  parentGuid: folderGuid || groupGuid,
                })}
            >
              <Plus size={18} />
              {t("newtab.addSite", "Add site")}
            </Button>
            <Button
              variant="secondary"
              disabled={snapshot.readOnly || !folderGuid}
              onClick={() =>
                setDialog({
                  type: "bookmark",
                  kind: "folder",
                  parentGuid: folderGuid || groupGuid,
                })}
            >
              <FolderPlus size={18} />
              {t("newtab.createFolder", "Create folder")}
            </Button>
          </div>
        </div>
        <div className="nt-folder-navigation">
          <IconButton
            label={t("newtab.goUp", "Go up one folder")}
            disabled={!folder || folder.guid === groupGuid}
            onClick={() => openFolder(parent)}
          >
            <ArrowUp size={19} />
          </IconButton>
          <nav
            id="dial-location"
            tabIndex={-1}
            aria-label={t("newtab.folderPath", "Bookmark folder path")}
          >
            <ol>
              {path.map((node) => (
                <li key={node.guid}>
                  <button type="button" onClick={() => openFolder(node.guid)}>
                    {node.title}
                  </button>
                  <ChevronRight size={15} aria-hidden="true" />
                </li>
              ))}
              <li>
                <span aria-current="page">
                  {folder?.title ?? currentGroup?.title}
                </span>
              </li>
            </ol>
          </nav>
          <div className="nt-group-note">
            {t(
              "newtab.assignmentNote",
              "Group selection here does not change the workspace assignment.",
            )}
            <button
              type="button"
              onClick={() => void newTabClient.call("openHub", {})}
            >
              {t("newtab.changeInHub", "Change in Hub")}
            </button>
          </div>
        </div>
      </div>
      <section
        id="nt-folder-panel"
        role="tabpanel"
        aria-labelledby={`group-${groupGuid}`}
      >
        <BookmarkGrid />
      </section>
    </main>
  );
}
