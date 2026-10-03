// SPDX-License-Identifier: MPL-2.0

import {
  createContext,
  type PropsWithChildren,
  useContext,
  useEffect,
  useRef,
  useState,
} from "react";
import { useTranslation } from "react-i18next";
import { Navigate, useLocation } from "react-router-dom";
import {
  EXPERIMENTS_CHANGED_EVENT,
  EXPERIMENTS_POLICY_PREF,
  experimentsRpc,
} from "../rpc/experiments.ts";
import { rpc } from "../rpc/rpc.ts";
import type {
  ContextMenuAvailability,
  ContextMenuAvailabilityState,
} from "./types.ts";

export const CONTEXT_MENU_SETTINGS_ROUTE = "/features/context-menu";
const CONTEXT_MENU_EXPERIMENT_ID = "context_menu_customization";

const AvailabilityContext = createContext<ContextMenuAvailability>("loading");

export function useContextMenuAvailability(): ContextMenuAvailability {
  return useContext(AvailabilityContext);
}

export function ContextMenuAvailabilityProvider(
  { children }: PropsWithChildren,
) {
  const { pathname } = useLocation();
  const generation = useRef(0);
  const [state, setState] = useState<ContextMenuAvailabilityState>({
    pathname,
    availability: "loading",
  });

  useEffect(() => {
    let mounted = true;
    const refresh = async (invalidate = true) => {
      const request = ++generation.current;
      if (invalidate) setState({ pathname, availability: "loading" });
      let availability: ContextMenuAvailability = "unavailable";
      try {
        const [experiments, policy] = await Promise.all([
          experimentsRpc.getAllExperiments(),
          rpc.getStringPref(EXPERIMENTS_POLICY_PREF),
        ]);
        // The policy is checked separately: a failed reinitialization can leave
        // assignments from before the user opted out in the experiments cache.
        if (
          policy !== "never" &&
          experiments.some((experiment) =>
            experiment.id === CONTEXT_MENU_EXPERIMENT_ID &&
            experiment.isActive === true &&
            experiment.currentVariantId === "enabled" &&
            (experiment.enrollmentStatus === "enrolled" ||
              experiment.enrollmentStatus === "force_enrolled")
          )
        ) availability = "available";
      } catch (error) {
        console.error(
          "[ContextMenuAvailability] Failed to read Flasco state",
          error,
        );
      }
      if (mounted && request === generation.current) {
        setState({ pathname, availability });
      }
    };
    const onChange = () => void refresh();
    // Rechecking after native menu interaction must preserve editor selections.
    // First entry and explicit Flasco changes still hide the UI while checking.
    const onFocus = () => void refresh(false);
    globalThis.addEventListener("focus", onFocus);
    globalThis.addEventListener(EXPERIMENTS_CHANGED_EVENT, onChange);
    void refresh();
    return () => {
      mounted = false;
      generation.current++;
      globalThis.removeEventListener("focus", onFocus);
      globalThis.removeEventListener(EXPERIMENTS_CHANGED_EVENT, onChange);
    };
  }, [pathname]);

  // Do not render a newly entered lazy route with the previous route's result
  // while its refresh effect is still waiting to run.
  const availability = state.pathname === pathname
    ? state.availability
    : "loading";
  return (
    <AvailabilityContext.Provider value={availability}>
      {children}
    </AvailabilityContext.Provider>
  );
}

export function ContextMenuAvailabilityGate({ children }: PropsWithChildren) {
  const availability = useContextMenuAvailability();
  const { t } = useTranslation();
  if (availability === "loading") {
    return <p role="status">{t("ui.loading", { defaultValue: "Loading…" })}</p>;
  }
  if (availability !== "available") {
    return <Navigate to="/overview/home" replace />;
  }
  return children;
}
