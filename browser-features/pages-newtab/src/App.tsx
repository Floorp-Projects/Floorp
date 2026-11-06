// SPDX-License-Identifier: MPL-2.0
import { useEffect, useState } from "react";
import { Grid2X2, Home as HomeIcon, Rss, Settings } from "lucide-react";
import { useTranslation } from "react-i18next";
import { StandardUIProvider } from "../../../libs/ui/standard-provider.tsx";
import { newTabClient } from "../../../libs/newtab/client.ts";
import logo from "../../../libs/ui/assets/Floorp_Logo_B_Dark.svg";
import {
  NewTabProvider,
  messageOf,
  useNewTab,
} from "./features/newtab/state.tsx";
import { Home } from "./features/newtab/Home.tsx";
import { Dial } from "./features/newtab/Bookmarks.tsx";
import { Reader } from "./features/newtab/Reader.tsx";
import { Dialogs } from "./features/newtab/Dialogs.tsx";
import { IconButton } from "./features/newtab/ui.tsx";
import { Wallpaper } from "./features/newtab/Wallpaper.tsx";
import "../../../libs/ui/page.css";
import "./globals.css";

function NewTabPage() {
  const { t, i18n } = useTranslation();
  const { snapshot, page, navigate, notify } = useNewTab();
  const [hasWallpaper, setHasWallpaper] = useState(false);
  const wallpaper = hasWallpaper && page !== "feed";
  useEffect(() => {
    document.title = `${t("newtab.title", "New tab")} — Floorp`;
    document.documentElement.lang = i18n.language;
    document.documentElement.dir = i18n.dir(i18n.language);
  }, [t, i18n, i18n.language]);
  const settings = async () => {
    try {
      await newTabClient.call("openHub", {});
    } catch (cause) {
      notify(messageOf(cause));
    }
  };
  return (
    <div
      className={`newtab-app nt-page-${page}${wallpaper ? " has-wallpaper" : ""}${wallpaper && snapshot.settings.home.translucentPanels ? " has-glass" : ""}`}
    >
      <a
        className="floorp-skip"
        href="#main-content"
        onClick={(event) => {
          event.preventDefault();
          const main = document.getElementById("main-content");
          main?.focus();
        }}
      >
        {t("newtab.skip", "Skip to content")}
      </a>
      <Wallpaper
        settings={snapshot.settings.background}
        visible={page !== "feed"}
        onLoadedChange={setHasWallpaper}
      />
      <header className="nt-header">
        <button
          type="button"
          className="nt-brand"
          onClick={() => navigate("home")}
          aria-label={t("newtab.floorpHome", "Floorp home")}
        >
          <img src={logo} alt="Floorp" />
        </button>
        <nav
          className="nt-primary-nav"
          aria-label={t("newtab.navigation", "New tab navigation")}
        >
          {(
            [
              { id: "home", Icon: HomeIcon, key: "home", label: "Home" },
              { id: "dial", Icon: Grid2X2, key: "dial", label: "Speed Dial" },
              { id: "feed", Icon: Rss, key: "feeds", label: "Feeds" },
            ] as const
          ).map(({ id, Icon, key, label }) => (
            <button
              type="button"
              key={id}
              aria-current={page === id ? "page" : undefined}
              onClick={() => {
                if (page === id) {
                  document
                    .querySelector(".nt-reader-list-scroll")
                    ?.scrollTo({ top: 0, behavior: "smooth" });
                  scrollTo({ top: 0, behavior: "smooth" });
                } else navigate(id);
              }}
            >
              <Icon size={20} strokeWidth={1.8} />
              <span>{t(`newtab.${key}`, label)}</span>
            </button>
          ))}
        </nav>
        <div className="nt-header-actions">
          {snapshot.workspace && (
            <span
              className="nt-current-workspace"
              title={snapshot.workspace.title}
            >
              {snapshot.workspace.title}
            </span>
          )}
          <IconButton
            label={t(
              "newtab.settingsInHub",
              "Open new tab settings in Floorp Hub",
            )}
            onClick={() => void settings()}
          >
            <Settings size={21} />
          </IconButton>
        </div>
      </header>
      {snapshot.readOnly && (
        <p className="nt-readonly" role="status">
          {t(
            "newtab.readOnly",
            "Settings are read-only. Your saved data has not been changed.",
          )}
        </p>
      )}
      {page === "home" ? <Home /> : page === "dial" ? <Dial /> : <Reader />}
      <Dialogs />
    </div>
  );
}
export default function App() {
  return (
    <StandardUIProvider>
      <NewTabProvider>
        <NewTabPage />
      </NewTabProvider>
    </StandardUIProvider>
  );
}
