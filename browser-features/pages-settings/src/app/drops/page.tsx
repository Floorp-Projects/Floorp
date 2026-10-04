/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Drops: コード一つで降ってくる機能を、店のように並べて入れる一枚。
//
//   棚          registry が配れるもの(library は連れてこられる側なので並べない)
//   入っている  この profile に入っているもの。戻せる
//   一枚        押すとまず「見る」(落として sha256 を確かめ、xpi を zip として読む。
//               JS は動かさない)。それから「入れる」── 権限を確認してから
//
// uuid で直に見るのは上級(棚に無いもの)。DropLinks が xpi のリンクを拾って
// about:hub#/features/drops/<uuid> を開いたときも、ここに来る。

import { useCallback, useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { useParams } from "react-router-dom";
import { Button } from "@/components/common/button.tsx";
import { Input } from "@/components/common/input.tsx";
import { Store } from "@/app/drops/Store.tsx";
import { Sheet } from "@/app/drops/Sheet.tsx";
import { Shelf, DropIcon } from "@/app/drops/Shelf.tsx";
import {
  type CatalogItem,
  type DropInspection,
  dropsRpc,
  type InstalledDrop,
} from "@/lib/rpc/drops.ts";

type Panel = "shelf" | "installed";

/** 押した瞬間に詳細へ移るための、棚から来た minimal な一枚(読み込みはこのあと) */
type Opening = {
  uuid: string;
  name?: string;
  note?: string;
  icon?: string | null;
  version?: string | null;
  registry?: string;
};

function messageOf(error: unknown): string {
  return String((error as Error)?.message ?? error);
}

export default function Page() {
  const { t } = useTranslation();
  const params = useParams<{ uuid?: string }>();

  const [panel, setPanel] = useState<Panel>("shelf");
  const [shelf, setShelf] = useState<CatalogItem[] | null>(null);
  const [failed, setFailed] = useState<{ registry: string; reason: string }[]>([]);
  const [q, setQ] = useState("");
  const [ref, setRef] = useState(params.uuid ?? "");
  const [seen, setSeen] = useState<DropInspection | null>(null);
  const [pending, setPending] = useState<Opening | null>(null);
  const [reading, setReading] = useState(false);
  const [installed, setInstalled] = useState<Record<string, InstalledDrop>>({});
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const refresh = useCallback(() => {
    try {
      setInstalled(dropsRpc.listDrops());
    } catch (e) {
      console.error("[drops] listDrops failed:", e);
    }
  }, []);

  // 押した瞬間に詳細の画面へ移る。棚のまま待たせると、無反応に見える ──
  // 先に棚の一枚(preview)だけ持って行って、読み込みはその画面で見せる
  const inspect = useCallback(async (
    uuid: string,
    registry?: string,
    preview?: Opening,
  ) => {
    setBusy(true);
    setError(null);
    setSeen(null);
    setReading(false);
    setPending(preview ?? { uuid });
    try {
      setSeen(await dropsRpc.inspectDrop(uuid, registry));
    } catch (e) {
      setError(`${t("drops.lookFailed")}: ${messageOf(e)}`);
    } finally {
      setBusy(false);
      setPending(null);
    }
  }, [t]);

  // 棚を引く。まず手元の一枚を出し(開いた瞬間に並ぶ)、裏で差分を取りに行く
  useEffect(() => {
    let alive = true;
    dropsRpc.listCatalogCached().then((cached) => {
      if (alive && cached.length > 0) {
        setShelf(cached);
      }
    }).catch(() => {});
    dropsRpc.listCatalog().then(
      (result) => {
        if (alive) {
          setShelf(result.items);
          setFailed(result.failed);
        }
      },
      (e) => {
        if (alive) {
          // 手元の一枚があれば、それを消さない(通信が転んでも棚は残す)
          setShelf((prev) => prev ?? []);
          setFailed([{ registry: "?", reason: messageOf(e) }]);
        }
      },
    );
    return () => {
      alive = false;
    };
  }, []);

  // 棚で見かけた一枚の manifest だけ、押す前に温める(hover の意思表示で一度だけ)
  const prefetched = useRef(new Set<string>());
  const prefetch = useCallback((item: CatalogItem) => {
    if (prefetched.current.has(item.uuid)) return;
    prefetched.current.add(item.uuid);
    void dropsRpc.prefetchDrop(item.uuid, item.registry).catch(() => {});
  }, []);

  // xpi のリンクから来たとき(DropLinks): about:hub#/features/drops/<uuid>
  useEffect(() => {
    const uuid = params.uuid?.trim().toLowerCase();
    if (uuid) {
      void inspect(uuid);
    }
    refresh();
  }, []);

  const install = async (drop: DropInspection) => {
    setBusy(true);
    setError(null);
    try {
      await dropsRpc.installDrop(drop);
      refresh();
    } catch (e) {
      setError(`${t("drops.installFailed")}: ${messageOf(e)}`);
    } finally {
      setBusy(false);
    }
  };

  const remove = async (uuid: string) => {
    setBusy(true);
    setError(null);
    try {
      await dropsRpc.removeDrop(uuid);
      refresh();
      // 戻すと、落としてあった xpi も一緒に消える。同じ一枚を開いたままなら **見直す** ──
      // そのまま「入れる」を押すと、指す先がもう無いので「見てから入れて」で転ぶ
      if (seen && seen.uuid === uuid) {
        setSeen(await dropsRpc.inspectDrop(uuid, seen.registry.name));
      }
    } catch (e) {
      setError(`${t("drops.installFailed")}: ${messageOf(e)}`);
    } finally {
      setBusy(false);
    }
  };

  const shown = (shelf ?? []).filter((item) => !item.lib);
  const libs = (shelf?.length ?? 0) - shown.length;
  const needle = q.trim().toLowerCase();
  const hit = shown.filter((item) =>
    !needle ||
    item.name.toLowerCase().includes(needle) ||
    item.note.toLowerCase().includes(needle) ||
    item.uuid.startsWith(needle)
  );
  const installedList = Object.entries(installed);
  const shelfOf = (uuid: string) => shown.find((item) => item.uuid === uuid);

  if (seen) {
    const version = installed[seen.uuid]?.versions?.[0];
    return (
      <div className="space-y-4 p-6">
        <button
          type="button"
          className="text-sm text-primary underline"
          onClick={() => {
            setSeen(null);
            setReading(false);
          }}
        >
          {t("drops.backToShelf")}
        </button>
        {error && <p className="text-sm text-red-600">{error}</p>}
        {reading
          ? <Sheet seen={seen} />
          : (
            <Store
              seen={seen}
              installedVersion={version}
              busy={busy}
              onInstall={() => install(seen)}
              onRead={() => setReading(true)}
              onRemove={version !== undefined ? () => remove(seen.uuid) : undefined}
              onLookAgain={() =>
                inspect(seen.uuid, seen.registry.name, {
                  uuid: seen.uuid,
                  name: seen.name,
                  note: seen.manifest.note,
                  icon: seen.icon,
                  version: seen.entries[0]?.version,
                  registry: seen.registry.name,
                })}
            />
          )}
      </div>
    );
  }

  // 押した直後の画面。棚から来た一枚だけを持って、読み込みを見せる(無反応にしない)
  if (pending) {
    return (
      <div className="space-y-4 p-6">
        <button
          type="button"
          className="text-sm text-primary underline"
          onClick={() => setPending(null)}
        >
          {t("drops.backToShelf")}
        </button>
        {error && <p className="text-sm text-red-600">{error}</p>}
        <div className="flex items-start gap-4">
          <DropIcon
            item={{ name: pending.name ?? pending.uuid, icon: pending.icon }}
            size="lg"
          />
          <div className="min-w-0">
            <h2 className="text-2xl font-bold">
              {pending.name ?? pending.uuid}
            </h2>
            {pending.note && (
              <p className="mt-1 text-sm text-base-content/70">{pending.note}</p>
            )}
            <p className="mt-2 flex flex-wrap items-center gap-2 text-xs text-base-content/50">
              {pending.version && (
                <code className="rounded bg-base-content/10 px-1.5 py-0.5">
                  {pending.version}
                </code>
              )}
              {pending.registry && <span>· {pending.registry}</span>}
            </p>
          </div>
        </div>
        <div className="flex items-center gap-3 text-sm text-base-content/70">
          <span className="inline-block h-4 w-4 animate-spin rounded-full border-2 border-primary border-t-transparent" />
          {t("drops.opening")}
        </div>
      </div>
    );
  }

  return (
    <div className="space-y-4 p-6">
      <div className="flex flex-col items-start pl-6">
        <h1 className="mb-2 text-3xl font-bold">{t("drops.title")}</h1>
        <p className="mb-6 text-sm">{t("drops.subtitle")}</p>
      </div>

      <div className="flex gap-2 pl-6">
        <Button
          variant={panel === "shelf" ? "primary" : "light"}
          size="sm"
          onClick={() => setPanel("shelf")}
        >
          {t("drops.tab.shelf")}
        </Button>
        <Button
          variant={panel === "installed" ? "primary" : "light"}
          size="sm"
          onClick={() => setPanel("installed")}
        >
          {t("drops.tab.installed")} ({installedList.length})
        </Button>
      </div>

      <div className="pl-6">
        {error && <p className="mb-3 text-sm text-red-600">{error}</p>}

        {panel === "shelf" && (
          <div className="space-y-4">
            <Input
              value={q}
              placeholder={t("drops.searchPlaceholder")}
              onChange={(e) => setQ(e.target.value)}
            />
            {shelf === null && (
              <p className="py-6 text-center text-base-content/70">
                {t("drops.shelfReading")}
              </p>
            )}
            {failed.map((f) => (
              <p key={f.registry} className="text-sm text-base-content/60">
                {t("drops.registryFailed", { registry: f.registry, reason: f.reason })}
              </p>
            ))}
            {shelf !== null && hit.length === 0 && (
              <p className="py-6 text-center text-base-content/70">
                {q ? t("drops.shelfEmptySearch") : t("drops.shelfEmpty")}
              </p>
            )}
            {hit.length > 0 && (
              <Shelf
                items={hit}
                installed={installed}
                onOpen={(item) => inspect(item.uuid, item.registry, item)}
                onHover={prefetch}
              />
            )}
            {libs > 0 && (
              <p className="text-sm text-base-content/50">
                {t("drops.libNote", { n: libs })}
              </p>
            )}

            <details className="rounded-lg border border-base-content/15 p-3">
              <summary className="cursor-pointer text-sm text-base-content/70">
                {t("drops.advanced")}
              </summary>
              <div className="mt-3 space-y-2">
                <label className="text-sm" htmlFor="drops-uuid">
                  {t("drops.uuidLabel")}
                </label>
                <div className="flex gap-2">
                  <Input
                    id="drops-uuid"
                    value={ref}
                    placeholder={t("drops.uuidPlaceholder")}
                    onChange={(e) => setRef(e.target.value)}
                    onKeyDown={(e) => {
                      if (e.key === "Enter" && ref.trim() && !busy) {
                        void inspect(ref.trim());
                      }
                    }}
                  />
                  <Button
                    variant="primary"
                    size="sm"
                    disabled={busy || ref.trim() === ""}
                    className="shrink-0"
                    onClick={() => inspect(ref.trim())}
                  >
                    {t("drops.look")}
                  </Button>
                </div>
              </div>
            </details>
          </div>
        )}

        {panel === "installed" && (
          <div className="space-y-3">
            {installedList.length === 0
              ? (
                <p className="py-6 text-center text-base-content/70">
                  {t("drops.installedEmpty")}
                </p>
              )
              : (
                installedList.map(([uuid, drop]) => {
                  const item = shelfOf(uuid);
                  return (
                    <div
                      key={uuid}
                      className="flex flex-col gap-3 rounded-lg border border-base-content/20 p-4 md:flex-row md:items-center md:justify-between"
                    >
                      <div className="min-w-0">
                        <div className="flex flex-wrap items-center gap-2">
                          <span className="font-medium">{drop.name || uuid}</span>
                          <code className="rounded bg-base-content/10 px-1.5 py-0.5 text-xs">
                            {drop.versions?.join(", ")}
                          </code>
                          {drop.registry && (
                            <span className="text-xs text-base-content/50">
                              {drop.registry}
                            </span>
                          )}
                        </div>
                        <span className="text-xs break-all text-base-content/50">
                          {uuid}
                        </span>
                      </div>
                      <div className="flex shrink-0 gap-2">
                        {item && (
                          <Button
                            variant="light"
                            size="sm"
                            onClick={() => inspect(item.uuid, item.registry, item)}
                          >
                            {t("drops.read")}
                          </Button>
                        )}
                        <Button
                          variant="secondary"
                          size="sm"
                          disabled={busy}
                          onClick={() => remove(uuid)}
                        >
                          {t("drops.uninstall")}
                        </Button>
                      </div>
                    </div>
                  );
                })
              )}
          </div>
        )}
      </div>
    </div>
  );
}
