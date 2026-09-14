/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SidebarProvider } from "@/components/common/sidebar.tsx";
import { useTranslation } from "react-i18next";
import { lazy, Suspense, useEffect } from "react";
import { Navigate, Route, Routes, useLocation } from "react-router-dom";
import { AppSidebar } from "@/components/app-sidebar.tsx";
import { Header } from "@/header/header.tsx";
import useHashSync from "@/hooks/useHashSync.ts";
import { useSettingFocus } from "@/hooks/useSettingFocus.ts";

const Dashboard = lazy(() => import("@/app/dashboard/page.tsx"));
const Design = lazy(() => import("@/app/design/page.tsx"));
const PanelSidebar = lazy(() => import("@/app/sidebar/page.tsx"));
const Workspaces = lazy(() => import("@/app/workspaces/page.tsx"));
const ProgressiveWebApp = lazy(() => import("@/app/pwa/page.tsx"));
const FloorpOS = lazy(() => import("@/app/floorp-os/page.tsx"));
const About = lazy(() => import("./app/about/noraneko.tsx"));
const ProfileAndAccount = lazy(() => import("@/app/accounts/page.tsx"));
const MouseGesture = lazy(() => import("@/app/gesture/page.tsx"));
const KeyboardShortcut = lazy(() => import("@/app/keyboard-shortcut/page.tsx"));
const Updates = lazy(() => import("@/app/updates/page.tsx"));
const Performance = lazy(() => import("@/app/performance/page.tsx"));
const LeptonSettings = lazy(() =>
  import("@/app/design/components/LeptonSettings.tsx").then((module) => ({
    default: module.LeptonSettings,
  }))
);
const SearchPage = lazy(() => import("@/app/search/page.tsx"));

export default function App() {
  const location = useLocation();
  const { t, i18n } = useTranslation();
  useEffect(() => {
    document.documentElement.lang = i18n.language;
    document.documentElement.dir = i18n.dir(i18n.language);
  }, [i18n, i18n.language]);
  useHashSync(location.pathname + location.search);
  useSettingFocus(location.pathname, location.search);

  return (
    <SidebarProvider>
      <div className="floorp-settings">
        <a className="floorp-skip" href="#settings-content">
          {t("ui.skipToContent", { defaultValue: "Skip to content" })}
        </a>
        <AppSidebar />
        <div className="floorp-settings-body">
          <Header />
          <main
            id="settings-content"
            tabIndex={-1}
            className="floorp-settings-main"
          >
            <Suspense
              fallback={
                <p role="status">
                  {t("ui.loading", { defaultValue: "Loading…" })}
                </p>
              }
            >
              <Routes>
                <Route
                  path="/"
                  element={<Navigate to="/overview/home" replace />}
                />
                <Route path="/search" element={<SearchPage />} />
                <Route path="/overview/home" element={<Dashboard />} />
                <Route path="/features/design" element={<Design />} />
                <Route
                  path="/features/design/lepton"
                  element={<LeptonSettings />}
                />
                <Route path="/features/sidebar" element={<PanelSidebar />} />
                <Route path="/features/workspaces" element={<Workspaces />} />
                <Route
                  path="/features/webapps"
                  element={<ProgressiveWebApp />}
                />
                <Route path="/features/floorp-os" element={<FloorpOS />} />
                <Route
                  path="/features/accounts"
                  element={<ProfileAndAccount />}
                />
                <Route path="features/gesture" element={<MouseGesture />} />
                <Route
                  path="/features/shortcuts"
                  element={<KeyboardShortcut />}
                />
                <Route path="/features/performance" element={<Performance />} />
                <Route path="/about/browser" element={<About />} />
                <Route path="/about/updates" element={<Updates />} />
              </Routes>
            </Suspense>
          </main>
        </div>
      </div>
    </SidebarProvider>
  );
}
