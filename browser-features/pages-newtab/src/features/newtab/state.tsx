// SPDX-License-Identifier: MPL-2.0
import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useRef,
  useState,
} from "react";
import type { PropsWithChildren } from "react";
import { useTranslation } from "react-i18next";
import { newTabClient } from "../../../../../libs/newtab/client.ts";
import type {
  FolderView,
  NewTabSnapshot,
} from "../../../../../libs/newtab/types.ts";
import type { DialogState, NewTabContextValue, Page } from "./types.ts";
import { Button } from "../../../../../libs/ui/button.tsx";
import { recoverFolder } from "./folderRecovery.ts";

const Context = createContext<NewTabContextValue | null>(null);
export const messageOf = (cause: unknown): string =>
  cause instanceof Error ? cause.message : String(cause);
export function pageFromHash(): Page {
  const route = location.hash.slice(1).split("/")[0];
  return route === "dial" || route === "feed" ? route : "home";
}
export function folderFromHash(): string | null {
  if (pageFromHash() !== "dial") return null;
  try {
    return (
      decodeURIComponent(location.hash.slice(1).split("/")[1] ?? "") || null
    );
  } catch {
    return null;
  }
}
export function NewTabProvider({ children }: PropsWithChildren) {
  const { t } = useTranslation();
  const [snapshot, setSnapshot] = useState<NewTabSnapshot | null>(null);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(true);
  const [page, setPage] = useState<Page>(pageFromHash);
  const [groupGuid, setGroupGuid] = useState("");
  const [folderGuid, setFolderGuid] = useState(folderFromHash() ?? "");
  const [folder, setFolder] = useState<FolderView | null>(null);
  const [folderLoading, setFolderLoading] = useState(false);
  const [folderError, setFolderError] = useState("");
  const [dialog, setDialog] = useState<DialogState>(null);
  const [toast, setToast] = useState("");
  const [epoch, setEpoch] = useState(0);
  const live = useRef(true);
  const snapshotVersion = useRef(0);
  const folderVersion = useRef(0);
  const previousFolder = useRef<FolderView | null>(null);
  const assignment = useRef<string | null>(null);
  const timer = useRef<ReturnType<typeof setTimeout> | undefined>(undefined);
  const snapshotRef = useRef<NewTabSnapshot | null>(null);
  const notify = useCallback((message: string) => {
    setToast(message);
    clearTimeout(timer.current);
    timer.current = setTimeout(() => setToast(""), 4000);
  }, []);
  const refresh = useCallback(async () => {
    const version = ++snapshotVersion.current;
    try {
      const next = await newTabClient.call("snapshot", {});
      if (!live.current || version !== snapshotVersion.current) return;
      const selected = next.workspace?.id
        ? next.settings.speedDial.workspaceGroups[next.workspace.id]
        : null;
      const root = next.groups.some((group) => group.guid === selected)
        ? selected!
        : (next.settings.speedDial.commonGuid ?? next.groups[0]?.guid ?? "");
      const identity = `${next.workspace?.id ?? ""}:${root}`;
      snapshotRef.current = next;
      setSnapshot(next);
      setError("");
      if (assignment.current !== null && assignment.current !== identity) {
        ++folderVersion.current;
        previousFolder.current = null;
        setGroupGuid(root);
        setFolderGuid(root);
        setFolder(null);
        if (pageFromHash() === "dial")
          location.hash = `dial/${encodeURIComponent(root)}`;
      } else
        setGroupGuid((current) =>
          next.groups.some((group) => group.guid === current) ? current : root,
        );
      setFolderGuid((current) => current || root);
      assignment.current = identity;
      setEpoch((current) => current + 1);
    } catch (cause) {
      if (live.current && version === snapshotVersion.current)
        setError(messageOf(cause));
    } finally {
      if (live.current && version === snapshotVersion.current)
        setLoading(false);
    }
  }, []);
  const reloadFolder = useCallback(async () => {
    const guid = page === "home" ? groupGuid : folderGuid || groupGuid;
    if (!guid || snapshotRef.current?.private || page === "feed") return;
    const version = ++folderVersion.current;
    setFolderLoading(true);
    setFolder((current) => (current?.guid === guid ? current : null));
    try {
      const result = await newTabClient.call("folder", { guid });
      if (!live.current || version !== folderVersion.current) return;
      previousFolder.current = result;
      setFolder(result);
      setFolderError("");
      if (page === "dial") {
        setGroupGuid(result.groupGuid);
        setFolderGuid(result.guid);
      }
    } catch (cause) {
      if (!live.current || version !== folderVersion.current) return;
      const recovered = await recoverFolder(
        previousFolder.current,
        guid,
        groupGuid,
        (ancestorGuid) => newTabClient.call("folder", { guid: ancestorGuid }),
        () => live.current && version === folderVersion.current,
      );
      if (!live.current || version !== folderVersion.current) return;
      if (recovered) {
        previousFolder.current = recovered;
        setFolder(recovered);
        setFolderError("");
        setFolderGuid(recovered.guid);
        if (pageFromHash() === "dial")
          location.hash = `dial/${encodeURIComponent(recovered.guid)}`;
        return;
      }
      setFolderError(messageOf(cause));
      setFolder(null);
    } finally {
      if (live.current && version === folderVersion.current)
        setFolderLoading(false);
    }
  }, [page, groupGuid, folderGuid]);
  useEffect(() => {
    live.current = true;
    void refresh();
    let debounce: ReturnType<typeof setTimeout>;
    const unsubscribe = newTabClient.subscribe(() => {
      clearTimeout(debounce);
      debounce = setTimeout(() => void refresh(), 40);
    });
    return () => {
      live.current = false;
      clearTimeout(debounce);
      clearTimeout(timer.current);
      unsubscribe();
    };
  }, [refresh]);
  useEffect(() => {
    void reloadFolder();
  }, [reloadFolder, epoch]);
  useEffect(() => {
    const sync = () => {
      const route = pageFromHash();
      const next = folderFromHash() ?? groupGuid;
      if (route !== page || (route === "dial" && next !== folderGuid))
        ++folderVersion.current;
      setPage(route);
      if (route === "dial") {
        if (next !== folderGuid) setFolder(null);
        setFolderGuid(next);
      }
    };
    addEventListener("hashchange", sync);
    return () => removeEventListener("hashchange", sync);
  }, [page, groupGuid, folderGuid]);
  const navigate = (next: Page) => {
    if (next !== page || (next === "dial" && folderGuid !== groupGuid))
      ++folderVersion.current;
    setPage(next);
    location.hash =
      next === "dial" ? `dial/${encodeURIComponent(groupGuid)}` : next;
    if (next === "dial") {
      if (folder?.guid !== groupGuid) setFolder(null);
      setFolderGuid(groupGuid);
    }
    scrollTo({ top: 0 });
  };
  const openFolder = (guid: string) => {
    if (page === "dial" && folderGuid === guid) {
      void reloadFolder();
      return;
    }
    ++folderVersion.current;
    setFolder(null);
    setFolderGuid(guid);
    setPage("dial");
    location.hash = `dial/${encodeURIComponent(guid)}`;
    scrollTo({ top: 0 });
    requestAnimationFrame(() =>
      document.getElementById("dial-location")?.focus(),
    );
  };
  const selectGroup = (guid: string) => {
    if (groupGuid === guid && (page === "home" || folderGuid === guid)) {
      void reloadFolder();
      return;
    }
    ++folderVersion.current;
    if (groupGuid !== guid) previousFolder.current = null;
    setFolder(null);
    setGroupGuid(guid);
    setFolderGuid(guid);
    if (page === "dial") location.hash = `dial/${encodeURIComponent(guid)}`;
  };
  if (!snapshot)
    return (
      <main className="newtab-start" aria-busy={loading}>
        <h1>{t("newtab.title", "New tab")}</h1>
        {loading ? (
          <p role="status">{t("newtab.loading", "Loading…")}</p>
        ) : (
          <>
            <p role="alert">{error}</p>
            <Button
              onClick={() => {
                setLoading(true);
                void refresh();
              }}
            >
              {t("newtab.retry", "Try again")}
            </Button>
          </>
        )}
      </main>
    );
  if (snapshot.private)
    return (
      <main className="newtab-start">
        <h1>
          {t(
            "newtab.unavailable",
            "This page is unavailable in private browsing",
          )}
        </h1>
        <a href="about:privatebrowsing">
          {t("newtab.privatePage", "Open the private browsing page")}
        </a>
      </main>
    );
  return (
    <Context.Provider
      value={{
        snapshot,
        page,
        groupGuid,
        folderGuid,
        folder,
        folderLoading,
        folderError,
        epoch,
        navigate,
        openFolder,
        selectGroup,
        refresh,
        reloadFolder,
        dialog,
        setDialog,
        notify,
      }}
    >
      {error && (
        <div className="newtab-global-error" role="alert">
          {error}
          <button type="button" onClick={() => void refresh()}>
            {t("newtab.retry", "Try again")}
          </button>
        </div>
      )}
      {children}
      <div
        className={`newtab-toast${toast ? " is-visible" : ""}`}
        role="status"
        aria-live="polite"
        aria-atomic="true"
      >
        {toast}
      </div>
    </Context.Provider>
  );
}
export function useNewTab(): NewTabContextValue {
  const value = useContext(Context);
  if (!value) throw new Error("NewTabProvider is missing");
  return value;
}
