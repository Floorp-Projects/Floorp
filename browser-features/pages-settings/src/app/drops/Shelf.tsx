/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// 棚: drop を面に並べる。一件が一枚の札で、押すとその drop のパネルへ。
// 並ぶ字(名前・一言)は registry から来たもの。必ずテキストとして描く。

import { useTranslation } from "react-i18next";
import type { CatalogItem, InstalledDrop } from "@/lib/rpc/drops.ts";
import { newer } from "@/lib/rpc/drops.ts";

export function DropIcon(
  { item, size = "md" }: {
    item: { icon?: string | null; name: string };
    size?: "md" | "lg";
  },
) {
  const box = size === "lg" ? "h-14 w-14 text-2xl" : "h-10 w-10 text-base";
  if (item.icon) {
    return (
      <img
        src={item.icon}
        alt=""
        loading="lazy"
        decoding="async"
        className={`${box} shrink-0 rounded-lg object-cover`}
      />
    );
  }
  return (
    <span
      className={`${box} flex shrink-0 items-center justify-center rounded-lg bg-base-content/10 font-medium text-base-content/70`}
    >
      {[...item.name][0] ?? "?"}
    </span>
  );
}

export function Shelf(
  { items, installed, onOpen, onHover }: {
    items: CatalogItem[];
    installed: Record<string, InstalledDrop>;
    onOpen: (item: CatalogItem) => void;
    onHover?: (item: CatalogItem) => void;
  },
) {
  const { t } = useTranslation();
  return (
    <div className="grid grid-cols-2 gap-3 md:grid-cols-3">
      {items.map((item) => {
        const have = installed[item.uuid];
        const update = have && newer(item.version, have?.versions?.[0]);
        return (
          <button
            key={`${item.registry}:${item.uuid}`}
            type="button"
            title={item.uuid}
            onClick={() => onOpen(item)}
            onMouseEnter={() => onHover?.(item)}
            onFocus={() => onHover?.(item)}
            className="flex min-h-36 flex-col items-start gap-2 rounded-lg border border-base-content/20 p-4 text-left transition-colors hover:border-base-content/40 hover:bg-base-content/5"
          >
            <DropIcon item={item} />
            <span className="font-medium">{item.name}</span>
            <span className="line-clamp-2 text-sm text-base-content/60">
              {item.note}
            </span>
            <span className="mt-auto flex flex-wrap items-center gap-2 pt-1 text-xs">
              <code className="rounded bg-base-content/10 px-1.5 py-0.5">
                {item.version ?? "?"}
              </code>
              <span className={item.rekor === null
                ? "text-base-content/50"
                : "text-green-600"}>
                {item.rekor === null
                  ? t("drops.signatureBad")
                  : t("drops.signatureOk")}
              </span>
              {have && (
                <span className="rounded-full bg-primary/15 px-2 py-0.5 text-primary">
                  {update ? t("drops.updateBadge") : t("drops.installedBadge")}
                </span>
              )}
            </span>
          </button>
        );
      })}
    </div>
  );
}
