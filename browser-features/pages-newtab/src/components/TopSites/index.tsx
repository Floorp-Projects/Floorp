import { type MouseEvent, useEffect, useRef, useState } from "react";
import {
  getNewTabSettings,
  type NewTabSettings,
  type PinnedSite,
  saveNewTabSettings,
} from "@/utils/dataManager.ts";
import { useTranslation } from "react-i18next";
import { PinIcon } from "lucide-react";
import { callNRWithRetry } from "@/utils/nrRetry.ts";

interface TopSite {
  url: string;
  title: string;
  label?: string;
  favicon?: string;
  smallFavicon?: string | null;
}

const TRUNCATE_TITLE_MAX = 18;
function truncateTopSiteTitle(
  title: string | undefined,
  max: number = TRUNCATE_TITLE_MAX,
): string {
  if (!title) return "";
  return title.length <= max ? title : title.slice(0, max - 1) + "…";
}

declare global {
  interface Window {
    NRGetCurrentTopSites: (callback: (data: string) => void) => void;
  }
}

async function getTopSites(): Promise<TopSite[]> {
  try {
    const parsed = await callNRWithRetry<{ topsites?: TopSite[] | undefined }>(
      (cb) => (globalThis as unknown as Window).NRGetCurrentTopSites(cb),
      (sites) => JSON.parse(sites),
      {
        retries: 3,
        timeoutMs: 1200,
        delayMs: 300,
        shouldRetry: (res) => {
          const shouldRetry = !res || !Array.isArray(res.topsites);
          return shouldRetry;
        },
      },
    );

    const list = (parsed.topsites ?? []).map((site: TopSite) => ({
      ...site,
      title: site.title || site.label || site.url,
      smallFavicon: site.smallFavicon || "",
    }));

    return list;
  } catch {
    return [];
  }
}

function BlockModal({
  site,
  onConfirm,
  onCancel,
}: {
  site: TopSite | PinnedSite;
  onConfirm: () => void;
  onCancel: () => void;
}) {
  const { t } = useTranslation();
  return (
    <dialog className="modal modal-open">
      <div className="modal-box">
        <h3 className="font-bold text-lg">
          {t("topSites.blockTitle", {
            defaultValue: `Block "${site.title}"?`,
          })}
        </h3>
        <p className="py-4">
          {t(
            "topSites.blockConfirm",
            "This site will be removed from your Top Sites and won't be suggested again. You can manage blocked sites in settings.",
          )}
        </p>
        <div className="modal-action">
          <button type="button" onClick={onConfirm} className="btn btn-primary">
            {t("topSites.block")}
          </button>
          <button type="button" onClick={onCancel} className="btn">
            {t("topSites.cancel")}
          </button>
        </div>
      </div>
    </dialog>
  );
}

function AddSiteModal({
  onAdd,
  onCancel,
}: {
  onAdd: (url: string, label: string) => void;
  onCancel: () => void;
}) {
  const { t } = useTranslation();
  const [url, setUrl] = useState("");
  const [label, setLabel] = useState("");
  const [error, setError] = useState<string | null>(null);
  const urlInputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    urlInputRef.current?.focus();
  }, []);

  const normalizeUrl = (raw: string): string | null => {
    let value = raw.trim();
    if (!value) return null;
    if (!/^https?:\/\//i.test(value)) {
      value = `https://${value}`;
    }
    try {
      const u = new URL(value);
      // 不要な末尾スラッシュを除去（ルートのみ維持）
      if (u.pathname !== "/") {
        u.pathname = u.pathname.replace(/\/+$/, "");
      }
      return u.href;
    } catch {
      return null;
    }
  };

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    const normalized = normalizeUrl(url);
    if (!normalized) {
      setError(t("topSites.invalidUrl", { defaultValue: "Invalid URL" }));
      return;
    }
    onAdd(normalized, label.trim() || normalized);
  };

  return (
    <dialog className="modal modal-open">
      <div className="modal-box">
        <h3 className="font-bold text-lg mb-2">{t("topSites.addNewSite")}</h3>
        <form onSubmit={handleSubmit} className="space-y-4">
          <div className="form-control">
            <label className="label">
              <span className="label-text">{t("topSites.url")}</span>
            </label>
            <input
              ref={urlInputRef}
              type="text"
              className="input input-bordered w-full"
              placeholder="https://example.com"
              value={url}
              onChange={(e) => setUrl(e.target.value)}
              required
            />
          </div>
          <div className="form-control">
            <label className="label">
              <span className="label-text">{t("topSites.label")}</span>
            </label>
            <input
              type="text"
              className="input input-bordered w-full"
              placeholder={t("topSites.labelPlaceholder", {
                defaultValue: "Website name",
              })}
              value={label}
              onChange={(e) => setLabel(e.target.value)}
            />
          </div>
          {error && <p className="text-error text-sm">{error}</p>}
          <div className="modal-action">
            <button type="button" className="btn" onClick={onCancel}>
              {t("topSites.cancel")}
            </button>
            <button type="submit" className="btn btn-primary">
              {t("topSites.add")}
            </button>
          </div>
        </form>
      </div>
    </dialog>
  );
}

export function TopSites() {
  const { t } = useTranslation();
  const [sites, setSites] = useState<TopSite[]>([]);
  const [pinnedSites, setPinnedSites] = useState<PinnedSite[]>([]);
  const [blockedSites, setBlockedSites] = useState<string[]>([]);
  const [contextMenu, setContextMenu] = useState<
    {
      x: number;
      y: number;
      site: TopSite | PinnedSite;
    } | null
  >(null);
  const [siteToBlock, setSiteToBlock] = useState<TopSite | PinnedSite | null>(
    null,
  );
  const [showAddModal, setShowAddModal] = useState(false);

  const contextMenuRef = useRef<HTMLDivElement>(null);

  const loadSettings = async () => {
    const settings = await getNewTabSettings();
    setPinnedSites(settings.topSites.pinned);
    setBlockedSites(settings.topSites.blocked);

    const topSites = await getTopSites();

    const filteredSites = topSites.filter(
      (site) => {
        const isBlocked = settings.topSites.blocked.includes(site.url);
        const isPinned = settings.topSites.pinned.some((p) =>
          p.url === site.url
        );
        return !isBlocked && !isPinned;
      },
    );

    setSites(filteredSites);
  };

  useEffect(() => {
    loadSettings();
  }, []);

  useEffect(() => {
    const handleClickOutside = (event: globalThis.MouseEvent) => {
      if (
        contextMenuRef.current &&
        !contextMenuRef.current.contains(event.target as Node)
      ) {
        setContextMenu(null);
      }
    };
    document.addEventListener("mousedown", handleClickOutside);
    return () => {
      document.removeEventListener("mousedown", handleClickOutside);
    };
  }, [contextMenuRef]);

  const handleContextMenu = (
    e: MouseEvent<HTMLAnchorElement>,
    site: TopSite | PinnedSite,
  ) => {
    e.preventDefault();
    setContextMenu({ x: e.pageX, y: e.pageY, site });
  };

  const handleSaveSettings = async (settings: Partial<NewTabSettings>) => {
    await saveNewTabSettings(settings);
    await loadSettings();
    setContextMenu(null);
  };

  const pinSite = async (site: TopSite | PinnedSite) => {
    const title = "title" in site && site.title ? site.title : site.url;
    const newPinnedSites = [...pinnedSites, { url: site.url, title }];
    await handleSaveSettings({
      topSites: { pinned: newPinnedSites, blocked: blockedSites },
    });
  };

  const addCustomSite = async (url: string, title: string) => {
    // 重複確認
    if (
      pinnedSites.some((p) => p.url === url) ||
      sites.some((s) => s.url === url) ||
      blockedSites.includes(url)
    ) {
      // 既に存在する場合はピン済み扱いとして閉じる
      setShowAddModal(false);
      return;
    }
    const newPinnedSites = [...pinnedSites, { url, title }];
    await handleSaveSettings({
      topSites: { pinned: newPinnedSites, blocked: blockedSites },
    });
    setShowAddModal(false);
  };

  const unpinSite = async (site: PinnedSite) => {
    const newPinnedSites = pinnedSites.filter((p) => p.url !== site.url);
    await handleSaveSettings({
      topSites: { pinned: newPinnedSites, blocked: blockedSites },
    });
  };

  const handleBlockRequest = (site: TopSite | PinnedSite) => {
    setSiteToBlock(site);
    setContextMenu(null);
  };

  const confirmBlockSite = async () => {
    if (!siteToBlock) return;
    const newBlockedSites = [...blockedSites, siteToBlock.url];
    let newPinnedSites = pinnedSites;
    if (isPinned(siteToBlock)) {
      newPinnedSites = pinnedSites.filter((p) => p.url !== siteToBlock.url);
    }
    await handleSaveSettings({
      topSites: { pinned: newPinnedSites, blocked: newBlockedSites },
    });
    setSiteToBlock(null);
  };

  const cancelBlockSite = () => {
    setSiteToBlock(null);
  };

  const isPinned = (site: TopSite | PinnedSite) => {
    return pinnedSites.some((p) => p.url === site.url);
  };

  return (
    <>
      {siteToBlock && (
        <BlockModal
          site={siteToBlock}
          onConfirm={confirmBlockSite}
          onCancel={cancelBlockSite}
        />
      )}
      {showAddModal && (
        <AddSiteModal
          onAdd={addCustomSite}
          onCancel={() => setShowAddModal(false)}
        />
      )}
      <div className="bg-gray-800/50 p-3 rounded-lg inline-block backdrop-blur-sm">
        <div className="flex flex-wrap gap-x-0.5">
          {pinnedSites.map((site) => (
            <a
              key={site.url}
              href={site.url}
              className="group flex flex-col items-center w-16 p-2 rounded-lg transition-all duration-200 hover:backdrop-blur-sm hover:bg-gray-700/50"
              onContextMenu={(e) => handleContextMenu(e, site)}
            >
              <div className="relative w-8 h-8">
                <div className="w-8 h-8 rounded-lg overflow-hidden bg-gray-700 flex items-center justify-center transform transition-transform group-hover:scale-110 mb-1">
                  <img
                    src={`https://www.google.com/s2/favicons?domain=${
                      new URL(site.url).hostname
                    }&sz=32`}
                    alt={site.title}
                    className="w-6 h-6 object-contain"
                  />
                </div>
                <div className="absolute top-0 right-0 transform translate-x-1/2 -translate-y-1/2">
                  <div className="w-4 h-4 rounded-full bg-gray-700 flex items-center justify-center shadow-sm">
                    <PinIcon className="w-3 h-3 text-white" />
                  </div>
                </div>
              </div>
              <span
                className="text-[11px] text-center text-gray-200 leading-tight max-w-full inline-block overflow-hidden text-ellipsis text-shadow-lg/20 group-hover:text-white transition-colors"
                title={site.title}
              >
                {truncateTopSiteTitle(site.title)}
              </span>
            </a>
          ))}
          {sites.map((site) => (
            <a
              key={site.url}
              href={site.url}
              className="group flex flex-col items-center w-16 p-2 rounded-lg transition-all duration-200 hover:backdrop-blur-sm hover:bg-gray-700/50"
              onContextMenu={(e) => handleContextMenu(e, site)}
            >
              <div className="w-8 h-8 rounded-lg overflow-hidden bg-gray-700 flex items-center justify-center transform transition-transform group-hover:scale-110 mb-1">
                {site.favicon || site.smallFavicon
                  ? (
                    <img
                      src={site.favicon || site.smallFavicon!}
                      alt={site.title}
                      className="w-6 h-6 object-contain"
                    />
                  )
                  : (
                    <span className="text-lg font-semibold text-gray-300">
                      {(site.title || "?")[0]}
                    </span>
                  )}
              </div>
              <span
                className="text-[11px] text-center text-gray-200 leading-tight max-w-full inline-block overflow-hidden text-ellipsis text-shadow-lg/20 group-hover:text-white transition-colors"
                title={site.title}
              >
                {truncateTopSiteTitle(site.title)}
              </span>
            </a>
          ))}
          {/* Add new site tile */}
          <button
            type="button"
            onClick={() => setShowAddModal(true)}
            className="group flex flex-col items-center w-16 p-2 rounded-lg border border-dashed border-gray-400/50 text-gray-300 hover:text-white hover:backdrop-blur-sm hover:bg-gray-700/50 transition-all"
            title={t("topSites.addSite")}
          >
            <div className="w-8 h-8 rounded-lg flex items-center justify-center bg-gray-700 group-hover:scale-110 transform transition-transform mb-1">
              <span className="text-2xl leading-none">＋</span>
            </div>
            <span className="text-[10px] text-center leading-tight line-clamp-2 text-shadow-lg/20 group-hover:text-white transition-colors">
              {t("topSites.addSite")}
            </span>
          </button>
        </div>
      </div>
      {contextMenu && (
        <div
          ref={contextMenuRef}
          className="absolute z-10 bg-base-200 rounded-md shadow-lg"
          style={{
            top: `${contextMenu.y}px`,
            left: `${contextMenu.x}px`,
          }}
        >
          <ul className="menu p-2">
            {isPinned(contextMenu.site)
              ? (
                <li onClick={() => unpinSite(contextMenu.site as PinnedSite)}>
                  <a>{t("topSites.unpin")}</a>
                </li>
              )
              : (
                <li onClick={() => pinSite(contextMenu.site)}>
                  <a>{t("topSites.pin")}</a>
                </li>
              )}
            <li onClick={() => handleBlockRequest(contextMenu.site)}>
              <a>{t("topSites.block")}</a>
            </li>
          </ul>
        </div>
      )}
    </>
  );
}
