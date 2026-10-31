import React, { useState } from "react";
import {
  Check,
  ChevronDown,
  Code2,
  Grid2X2,
  Home as HomeIcon,
  Rss,
  Settings,
} from "lucide-react";
import { workspaces } from "./data.ts";
import { usePrototype } from "./store.tsx";
import { Logo, IconButton } from "./ui.tsx";
import { Home } from "./Home.tsx";
import { Dial } from "./SiteGrid.tsx";
import { Reader } from "./Reader.tsx";
import { Hub } from "./Hub.tsx";
import { Dialogs } from "./Dialogs.tsx";
import { NewTabBackground } from "./Background.tsx";
import type { Route, WorkspaceId } from "./types.ts";

const navigation = [
  { id: "home", title: "ホーム", Icon: HomeIcon },
  { id: "dial", title: "スピードダイアル", Icon: Grid2X2 },
  { id: "feed", title: "フィード", Icon: Rss },
] as const;

function Header() {
  const { route, navigate, workspaceId, setWorkspaceId } = usePrototype();
  const selectRoute = (next: Route) => {
    if (next === route) {
      document
        .querySelector(".reader-list-scroll")
        ?.scrollTo({ top: 0, behavior: "smooth" });
      window.scrollTo({ top: 0, behavior: "smooth" });
    } else navigate(next);
  };
  return (
    <header className="app-header">
      <button
        className="brand-home"
        onClick={() => selectRoute("home")}
        aria-label="Floorp ホーム"
      >
        <Logo onDark />
      </button>
      <nav className="primary-nav" aria-label="新しいタブ">
        {navigation.map(({ id, title, Icon }) => (
          <button
            key={id}
            onClick={() => selectRoute(id)}
            aria-current={route === id ? "page" : undefined}
          >
            <Icon size={20} strokeWidth={1.8} />
            <span>{title}</span>
          </button>
        ))}
      </nav>
      <div className="header-actions">
        <label className="workspace-select">
          <Code2 size={17} aria-hidden="true" />
          <span className="sr-only">ワークスペース</span>
          <select
            aria-label="ワークスペース"
            value={workspaceId}
            onChange={(event) =>
              setWorkspaceId(event.target.value as WorkspaceId)
            }
          >
            {workspaces.map((workspace) => (
              <option key={workspace.id} value={workspace.id}>
                {workspace.title}
              </option>
            ))}
          </select>
          <ChevronDown size={15} aria-hidden="true" />
        </label>
        <IconButton
          label="Floorp Hub で新しいタブの設定を開く"
          onClick={() => navigate("hub", "general")}
        >
          <Settings size={22} strokeWidth={1.8} />
        </IconButton>
      </div>
    </header>
  );
}

export function App() {
  const { route, data, toast } = usePrototype();
  const [hasWallpaper, setHasWallpaper] = useState(false);
  const translucent =
    hasWallpaper &&
    data.settings.translucentPanels &&
    (route === "home" || route === "dial");
  return (
    <div
      className={`prototype-app route-${route}${translucent ? " wallpaper-glass" : ""}`}
      data-background={data.settings.background.type}
      data-wallpaper={hasWallpaper && (route === "home" || route === "dial")}
      data-translucency={Boolean(data.settings.translucentPanels)}
    >
      <NewTabBackground onImageChange={setHasWallpaper} />
      <a
        className="floorp-skip"
        href="#main-content"
        onClick={(event) => {
          event.preventDefault();
          const main =
            document.getElementById("main-content") ??
            document.querySelector("main");
          if (main instanceof HTMLElement) {
            main.tabIndex = -1;
            main.focus();
          }
        }}
      >
        本文へ移動
      </a>
      {route !== "hub" && <Header />}
      {route === "home" && <Home />}
      {route === "dial" && <Dial />}
      {route === "feed" && (
        <main id="main-content">
          <Reader />
        </main>
      )}
      {route === "hub" && <Hub />}
      <Dialogs />
      <div
        role="status"
        aria-live="polite"
        aria-atomic="true"
        className={`toast ${toast ? "is-visible" : ""}`}
      >
        {toast && (
          <>
            <Check size={18} />
            {toast}
          </>
        )}
      </div>
    </div>
  );
}
