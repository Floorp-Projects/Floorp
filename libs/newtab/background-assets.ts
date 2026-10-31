// SPDX-License-Identifier: MPL-2.0
const images = import.meta.glob(
  "../../browser-features/pages-newtab/src/assets/floorp/*.png",
  { eager: true, import: "default", query: "?url" },
);
export const floorpBackgrounds = Object.entries(images).flatMap(
  ([path, value]) =>
    typeof value === "string"
      ? [{ name: path.split("/").pop()!, url: value }]
      : [],
);
export function backgroundTimeLabel(name: string): string {
  const match = /^(\d{2})(\d{2})-(\d{2})(\d{2})\.png$/.exec(name);
  return match ? `${match[1]}:${match[2]} – ${match[3]}:${match[4]}` : name;
}
