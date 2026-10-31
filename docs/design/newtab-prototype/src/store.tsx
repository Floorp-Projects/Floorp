import React, {
  createContext,
  useContext,
  useEffect,
  useRef,
  useState,
} from "react";
import type { PropsWithChildren } from "react";
import { initialState } from "./data.ts";
import type {
  FeedFilter,
  GroupId,
  HubTab,
  PrototypeState,
  Route,
  WorkspaceId,
} from "./types.ts";
import type { DialogState, PrototypeContextValue } from "./context-types.ts";

const STORAGE_KEY = "floorp-newtab-design-v1";
const Context = createContext<PrototypeContextValue | null>(null);
const routeFromHash = (): Route => {
  const value = window.location.hash.slice(1).split("/")[0];
  return value === "dial" || value === "feed" || value === "hub"
    ? value
    : "home";
};
const folderFromHash = (): string | null => {
  if (routeFromHash() !== "dial") return null;
  try {
    return (
      decodeURIComponent(location.hash.slice(1).split("/")[1] ?? "") || null
    );
  } catch {
    return null;
  }
};

function loadState(): PrototypeState {
  try {
    const value: unknown = JSON.parse(
      localStorage.getItem(STORAGE_KEY) ?? "null",
    );
    if (
      value &&
      typeof value === "object" &&
      "settings" in value &&
      "assignments" in value &&
      "sites" in value &&
      Array.isArray(value.sites) &&
      "readIds" in value &&
      Array.isArray(value.readIds) &&
      "feeds" in value &&
      Array.isArray(value.feeds) &&
      "starredIds" in value &&
      Array.isArray(value.starredIds) &&
      "dismissedSponsors" in value &&
      Array.isArray(value.dismissedSponsors)
    ) {
      const saved = value as PrototypeState;
      const background = saved.settings?.background;
      const folders = Array.isArray(saved.folders) ? saved.folders : [];
      return {
        ...structuredClone(initialState),
        ...saved,
        // Flat previews remain flat; do not inject sample folders into a save.
        folders,
        sites: saved.sites.map((site, index) => ({
          ...site,
          folderId: folders.some(
            (folder) =>
              folder.id === site.folderId && folder.groupId === site.groupId,
          )
            ? site.folderId
            : null,
          order: site.order ?? index,
        })),
        settings: {
          ...initialState.settings,
          ...saved.settings,
          background: {
            ...initialState.settings.background,
            // Older design previews stored only a white/lavender swatch.
            ...(background && typeof background === "object" ? background : {}),
          },
        },
      };
    }
  } catch {
    /* An invalid demo save starts with sample data. */
  }
  return structuredClone(initialState);
}

export function PrototypeProvider({ children }: PropsWithChildren) {
  const [data, setData] = useState(loadState);
  const [route, setRoute] = useState<Route>(routeFromHash);
  const [workspaceId, changeWorkspace] = useState<WorkspaceId>("development");
  const [groupId, changeGroup] = useState<GroupId>(
    () =>
      data.folders.find((folder) => folder.id === folderFromHash())?.groupId ??
      data.assignments.development,
  );
  const [folderId, setFolderId] = useState<string | null>(
    () =>
      data.folders.find((folder) => folder.id === folderFromHash())?.id ?? null,
  );
  const previousAssignment = useRef({
    workspaceId,
    groupId: data.assignments[workspaceId],
  });
  const [hubTab, setHubTab] = useState<HubTab>(() => {
    const tab = location.hash.split("/")[1];
    return tab === "dial" || tab === "feed" || tab === "sponsors"
      ? tab
      : "general";
  });
  const [selectedArticleId, setSelectedArticleId] = useState<string | null>(
    null,
  );
  const [feedFilter, setFeedFilter] = useState<FeedFilter>("all");
  const [selectedFeedId, setSelectedFeedId] = useState<string | null>(null);
  const [feedQuery, setFeedQuery] = useState("");
  const [dialog, setDialog] = useState<DialogState>(null);
  const [toast, setToast] = useState("");
  const toastTimer = useRef<ReturnType<typeof setTimeout> | undefined>(
    undefined,
  );

  useEffect(() => {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
    } catch {
      /* Storage may be unavailable in file previews. */
    }
  }, [data]);
  useEffect(() => {
    const assignedGroup = data.assignments[workspaceId];
    if (
      previousAssignment.current.workspaceId === workspaceId &&
      previousAssignment.current.groupId === assignedGroup
    )
      return;
    previousAssignment.current = { workspaceId, groupId: assignedGroup };
    changeGroup(assignedGroup);
    setFolderId(null);
    if (routeFromHash() === "dial") window.location.hash = "dial";
  }, [workspaceId, data.assignments]);
  useEffect(() => {
    const syncRoute = () => {
      setRoute(routeFromHash());
      if (routeFromHash() === "dial") {
        const folder = data.folders.find(
          (item) => item.id === folderFromHash(),
        );
        setFolderId(folder?.id ?? null);
        if (folder) changeGroup(folder.groupId);
      }
      const tab = location.hash.split("/")[1];
      if (
        tab === "general" ||
        tab === "dial" ||
        tab === "feed" ||
        tab === "sponsors"
      )
        setHubTab(tab);
    };
    window.addEventListener("hashchange", syncRoute);
    return () => window.removeEventListener("hashchange", syncRoute);
  }, [data.folders]);
  useEffect(() => {
    if (!folderId) return;
    const folder = data.folders.find((item) => item.id === folderId);
    if (!folder) {
      setFolderId(null);
      if (routeFromHash() === "dial") window.location.hash = "dial";
    } else if (folder.groupId !== groupId) changeGroup(folder.groupId);
  }, [folderId, data.folders, groupId]);
  useEffect(() => {
    document.title =
      route === "hub" ? "新しいタブ — Floorp Hub" : "新しいタブ — Floorp";
  }, [route]);
  useEffect(() => () => clearTimeout(toastTimer.current), []);

  const navigate = (next: Route, tab?: HubTab) => {
    setRoute(next);
    if (next === "dial") setFolderId(null);
    if (tab) setHubTab(tab);
    window.location.hash = next === "hub" ? `hub/${tab ?? hubTab}` : next;
    window.scrollTo({ top: 0 });
    document.title =
      next === "hub" ? "新しいタブ — Floorp Hub" : "新しいタブ — Floorp";
  };
  const setGroupId = (id: GroupId) => {
    changeGroup(id);
    setFolderId(null);
    if (routeFromHash() === "dial") window.location.hash = "dial";
  };
  const openFolder = (id: string | null) => {
    const folder = id ? data.folders.find((item) => item.id === id) : undefined;
    if (id && !folder) return;
    if (folder) changeGroup(folder.groupId);
    setFolderId(id);
    setRoute("dial");
    window.location.hash = id ? `dial/${encodeURIComponent(id)}` : "dial";
    window.scrollTo({ top: 0 });
    requestAnimationFrame(() =>
      document.getElementById("dial-location")?.focus(),
    );
  };
  const showToast = (message: string) => {
    clearTimeout(toastTimer.current);
    setToast(message);
    toastTimer.current = setTimeout(() => setToast(""), 3000);
  };
  const openArticle = (id: string) => {
    setSelectedArticleId(id);
    setData((previous) =>
      previous.readIds.includes(id)
        ? previous
        : { ...previous, readIds: [...previous.readIds, id] },
    );
    navigate("feed");
  };

  return (
    <Context.Provider
      value={{
        data,
        setData,
        route,
        navigate,
        workspaceId,
        setWorkspaceId: changeWorkspace,
        groupId,
        setGroupId,
        folderId,
        openFolder,
        hubTab,
        setHubTab,
        selectedArticleId,
        openArticle,
        feedFilter,
        setFeedFilter,
        selectedFeedId,
        setSelectedFeedId,
        feedQuery,
        setFeedQuery,
        dialog,
        setDialog,
        toast,
        showToast,
      }}
    >
      {children}
    </Context.Provider>
  );
}

export function usePrototype(): PrototypeContextValue {
  const value = useContext(Context);
  if (!value) throw new Error("PrototypeProvider is missing");
  return value;
}
