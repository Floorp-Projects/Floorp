// SPDX-License-Identifier: MPL-2.0

import {
  createContext,
  type PropsWithChildren,
  useContext,
  useEffect,
  useState,
} from "react";
import { useTranslation } from "react-i18next";
import { Navigate } from "react-router-dom";
import { rpc } from "../rpc/rpc.ts";

export const CLIPS_SETTINGS_ROUTE = "/features/clips";
const AvailabilityContext = createContext<boolean | null>(null);

export function useClipsAvailability(): boolean | null {
  return useContext(AvailabilityContext);
}

/** The startup gate's answer is shared by routing, navigation and search. */
export function ClipsAvailabilityProvider({ children }: PropsWithChildren) {
  const [enabled, setEnabled] = useState<boolean | null>(null);
  useEffect(() => {
    let mounted = true;
    void rpc.getBoolPref("floorp.browser.clips.enabled").then(
      (value) => {
        if (mounted) setEnabled(value === true);
      },
      (error) => {
        console.error("[ClipsAvailability] Failed to read startup gate", error);
        if (mounted) setEnabled(false);
      },
    );
    return () => {
      mounted = false;
    };
  }, []);
  return (
    <AvailabilityContext.Provider value={enabled}>
      {children}
    </AvailabilityContext.Provider>
  );
}

export function ClipsAvailabilityGate({ children }: PropsWithChildren) {
  const enabled = useClipsAvailability();
  const { t } = useTranslation();
  if (enabled === null) {
    return <p role="status">{t("ui.loading", { defaultValue: "Loading…" })}</p>;
  }
  return enabled ? children : <Navigate to="/overview/home" replace />;
}
