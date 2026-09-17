import { useEffect } from "react";

/** Wait for a lazy route, then reveal the search destination without changing it. */
export function useSettingFocus(pathname: string, search: string) {
  useEffect(() => {
    const id = new URLSearchParams(search).get("setting");
    const root = document.getElementById("settings-content");
    if (!id || !root) return;
    const reveal = () => {
      const field = document.getElementById(id);
      if (!field || !root.contains(field)) return false;
      if (field.closest('[aria-busy="true"]')) return false;
      field.scrollIntoView({ block: "center" });
      field.focus({ preventScroll: true });
      if (document.activeElement !== field) {
        const label = root.querySelector<HTMLElement>(
          `label[for="${CSS.escape(id)}"]`,
        );
        if (label) {
          label.tabIndex = -1;
          label.focus({ preventScroll: true });
        }
      }
      return true;
    };
    if (reveal()) return;
    const observer = new MutationObserver(() => {
      if (reveal()) observer.disconnect();
    });
    observer.observe(root, { childList: true, subtree: true, attributes: true, attributeFilter: ["aria-busy", "disabled"] });
    const timeout = globalThis.setTimeout(() => observer.disconnect(), 10000);
    return () => {
      observer.disconnect();
      globalThis.clearTimeout(timeout);
    };
  }, [pathname, search]);
}
