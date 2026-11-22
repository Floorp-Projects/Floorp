import { ExternalLink } from "lucide-react";
import {
  SidebarGroup,
  SidebarGroupLabel,
  SidebarMenu,
  useSidebar,
} from "@/components/common/sidebar.tsx";
import { Link, useLocation } from "react-router-dom";
import {
  createContext,
  useCallback,
  useContext,
  useLayoutEffect,
  useMemo,
  useRef,
} from "react";
import styles from "./common/sidebar.module.css";
import type {
  ExternalItemProps,
  InternalItemFocusState,
  InternalItemProps,
  NavFeaturesProps,
  NavigationFocusRemoval,
  NavigationFocusScopeContextValue,
  NavigationFocusScopeProps,
} from "./nav-features.types.ts";

export type { Feature } from "./nav-features.types.ts";

const NavigationFocusScopeContext = createContext<
  NavigationFocusScopeContextValue | null
>(null);

export function NavigationFocusScope({
  children,
}: NavigationFocusScopeProps) {
  const scopeRef = useRef<HTMLDivElement | null>(null);
  const pendingRemoval = useRef<NavigationFocusRemoval | null>(null);
  const recordFocusedRemoval = useCallback(
    (focusKey: string, node: HTMLElement) => {
      if (node.ownerDocument.activeElement === node) {
        pendingRemoval.current = { focusKey, node };
      }
    },
    [],
  );
  const context = useMemo(() => ({ recordFocusedRemoval }), [
    recordFocusedRemoval,
  ]);

  useLayoutEffect(() => {
    const removal = pendingRemoval.current;
    pendingRemoval.current = null;
    const scope = scopeRef.current;
    if (!removal || !scope) return;

    const document = removal.node.ownerDocument;
    const activeElement = document.activeElement;
    if (
      activeElement !== null && activeElement !== removal.node &&
      activeElement !== document.body &&
      activeElement !== document.documentElement
    ) return;

    for (
      const candidate of scope.querySelectorAll<HTMLElement>(
        "[data-sidebar-focus-key]",
      )
    ) {
      if (
        candidate.getAttribute("data-sidebar-focus-key") === removal.focusKey
      ) {
        candidate.focus({ preventScroll: true });
        return;
      }
    }

    scope.querySelector<HTMLElement>("[data-sidebar-home-fallback]")?.focus({
      preventScroll: true,
    });
  });

  return (
    <NavigationFocusScopeContext.Provider value={context}>
      <div ref={scopeRef}>{children}</div>
    </NavigationFocusScopeContext.Provider>
  );
}

// Internal item renderer without SidebarMenuItem wrapper
function InternalItem({ feature, isActive }: InternalItemProps) {
  const { setOpen } = useSidebar();
  const focusScope = useContext(NavigationFocusScopeContext);
  const focusState = useRef<InternalItemFocusState>({
    node: null,
    hadFocus: false,
  });
  const setItemRef = useCallback((node: HTMLElement | null) => {
    const state = focusState.current;
    if (!node) {
      const outgoing = state.node;
      state.hadFocus = outgoing?.ownerDocument.activeElement === outgoing;
      if (state.hadFocus && outgoing && feature.focusKey) {
        focusScope?.recordFocusedRemoval(feature.focusKey, outgoing);
      }
      state.node = null;
      return;
    }
    state.node = node;
    if (state.hadFocus) {
      state.hadFocus = false;
      node.focus({ preventScroll: true });
    }
  }, [feature.focusKey, focusScope]);

  return (
    <div>
      {feature.disabled
        ? (
          <div
            ref={setItemRef}
            data-sidebar-focus-key={feature.focusKey}
            className={`${styles.link} ${styles.disabled}`}
            aria-current={isActive ? "page" : undefined}
            aria-disabled="true"
            tabIndex={-1}
          >
            <feature.icon className="size-4" />
            <span>{feature.title}</span>
          </div>
        )
        : (
          <Link
            ref={setItemRef}
            data-sidebar-focus-key={feature.focusKey}
            data-sidebar-home-fallback={feature.focusKey === undefined &&
                feature.url === "/overview/home"
              ? ""
              : undefined}
            to={feature.url}
            onClick={() => setOpen(false)}
            className={styles.link}
            aria-current={isActive ? "page" : undefined}
          >
            <feature.icon className="size-4" />
            <span>{feature.title}</span>
          </Link>
        )}
    </div>
  );
}

// External item renderer without SidebarMenuItem wrapper
function ExternalItem({ feature }: ExternalItemProps) {
  const { setOpen } = useSidebar();
  return (
    <div>
      <button
        type="button"
        onClick={(event) => {
          feature.onClick(event);
          setOpen(false);
        }}
        className={styles.link}
      >
        <feature.icon className="size-4" />
        <span>{feature.title}</span>
        <ExternalLink className="size-3 ml-auto opacity-60" />
      </button>
    </div>
  );
}

export function NavFeatures({
  title,
  features,
}: NavFeaturesProps) {
  const location = useLocation();
  const currentRoute = location.pathname;

  return (
    <SidebarGroup>
      <SidebarGroupLabel>{title}</SidebarGroupLabel>
      <SidebarMenu>
        {features.map((feature) => {
          if (feature.isExternal === true) {
            return (
              <ExternalItem
                key={feature.title}
                feature={feature}
              />
            );
          }

          const featurePath = feature.url.startsWith("#")
            ? feature.url.slice(1)
            : feature.url;
          const isActive = feature.exactMatch
            ? currentRoute === featurePath
            : featurePath === "/"
            ? currentRoute === "/"
            : currentRoute.startsWith(featurePath);

          return (
            <InternalItem
              key={feature.reactKey ?? feature.title}
              feature={feature}
              isActive={isActive}
            />
          );
        })}
      </SidebarMenu>
    </SidebarGroup>
  );
}
