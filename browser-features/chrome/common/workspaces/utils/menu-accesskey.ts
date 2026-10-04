// SPDX-License-Identifier: MPL-2.0

/** Keep the localized workspace key clear of the visible native tab commands. */
export function getWorkspaceMenuAccessKey(
  menu: Element,
  preferredAccessKey: string,
): string {
  const usedKeys = new Set<string>();
  for (const sibling of menu.parentElement?.children ?? []) {
    if (
      sibling === menu ||
      (sibling.localName !== "menu" && sibling.localName !== "menuitem")
    ) {
      continue;
    }
    const style = menu.ownerDocument.defaultView?.getComputedStyle(sibling);
    if (
      sibling.getAttribute("hidden") === "true" ||
      sibling.getAttribute("collapsed") === "true" ||
      style?.display === "none" ||
      style?.visibility === "hidden" ||
      style?.visibility === "collapse"
    ) {
      continue;
    }
    const key = sibling.getAttribute("accesskey");
    if (key) {
      usedKeys.add(key.toLowerCase());
    }
  }

  // Source-language fallbacks also cover locales awaiting a Crowdin update.
  return [preferredAccessKey.trim(), "K", "W", "F"].find((key) =>
    Array.from(key).length === 1 && !usedKeys.has(key.toLowerCase())
  ) ?? "";
}
