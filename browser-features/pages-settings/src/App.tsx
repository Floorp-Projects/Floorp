/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import Dashboard from "@/app/dashboard/page.tsx";
import Design from "@/app/design/page.tsx";
import PanelSidebar from "@/app/sidebar/page.tsx";
import Workspaces from "@/app/workspaces/page.tsx";
import ProgressiveWebApp from "@/app/pwa/page.tsx";
import FloorpOS from "@/app/floorp-os/page.tsx";
import About from "./app/about/noraneko.tsx";
import ProfileAndAccount from "@/app/accounts/page.tsx";
import MouseGesture from "@/app/gesture/page.tsx";
import KeyboardShortcut from "@/app/keyboard-shortcut/page.tsx";
import Debug from "./app/debug/page.tsx";
import { LeptonSettings } from "@/app/design/components/LeptonSettings.tsx";
import { AppBackground } from "@/components/app-background.tsx";
import { Navigate, Route, Routes, useLocation } from "react-router-dom";
import { AppSidebar } from "@/components/app-sidebar.tsx";
import { Header } from "@/header/header.tsx";
import useHashSync from "@/hooks/useHashSync.ts";

export default function App() {
  const location = useLocation();
  useHashSync(location.pathname);

  return (
    <div className="flex flex-col w-screen">
      <AppBackground />
      <div className="flex flex-1 z-1">
        <AppSidebar />
        <div className="flex-1 max-w-3xl">
          <Header />
          <div className="p-4">
            <Routes>
              <Route
                path="/"
                element={<Navigate to="/overview/home" replace />}
              />
              <Route path="/overview/home" element={<Dashboard />} />
              <Route path="/features/design" element={<Design />} />
              <Route
                path="/features/design/lepton"
                element={<LeptonSettings />}
              />
              <Route path="/features/sidebar" element={<PanelSidebar />} />
              <Route path="/features/workspaces" element={<Workspaces />} />
              <Route path="/features/webapps" element={<ProgressiveWebApp />} />
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
              <Route path="/about/browser" element={<About />} />
              <Route path="/debug" element={<Debug />} />
            </Routes>
          </div>
        </div>
      </div>
    </div>
  );
}
