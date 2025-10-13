import { type MouseEvent } from "react";
import { PinIcon } from "lucide-react";
import type { TopSite, PinnedSite } from "./index.tsx";

const TRUNCATE_TITLE_MAX = 18;
function truncateTopSiteTitle(
  title: string | undefined,
  max: number = TRUNCATE_TITLE_MAX,
): string {
  if (!title) return "";
  return title.length <= max ? title : title.slice(0, max - 1) + "…";
}

interface TopSiteTileProps {
  site: TopSite | PinnedSite;
  isPinned: boolean;
  isFirefoxMode: boolean;
  onContextMenu: (e: MouseEvent<HTMLAnchorElement>, site: TopSite | PinnedSite) => void;
}

export function TopSiteTile({ site, isPinned, isFirefoxMode, onContextMenu }: TopSiteTileProps) {
  const isPinnedSite = isPinned;
  const faviconUrl = isPinnedSite
    ? `https://www.google.com/s2/favicons?domain=${new URL(site.url).hostname}&sz=64`
    : (site as TopSite).favicon || (site as TopSite).smallFavicon;

  return (
    <a
      key={site.url}
      href={site.url}
      className={`group flex flex-col items-center p-2 rounded-lg transition-all duration-200 hover:backdrop-blur-sm hover:bg-gray-700/50 ${
        isFirefoxMode ? "w-20 md:w-24" : "w-16"
      }`}
      onContextMenu={(e) => onContextMenu(e, site)}
    >
      <div
        className={`relative ${isFirefoxMode ? "w-10 h-10 md:w-12 md:h-12" : "w-8 h-8"}`}
      >
        <div
          className={`rounded-lg overflow-hidden bg-gray-700 flex items-center justify-center transform transition-transform group-hover:scale-110 mb-1 ${
            isFirefoxMode ? "w-10 h-10 md:w-12 md:h-12" : "w-8 h-8"
          }`}
        >
          {faviconUrl ? (
            <img
              src={faviconUrl}
              alt={site.title}
              className={`${
                isFirefoxMode ? "w-6 h-6 md:w-8 md:h-8" : "w-6 h-6"
              } object-contain`}
            />
          ) : (
            <span className="text-lg font-semibold text-gray-300">
              {(site.title || "?")[0]}
            </span>
          )}
        </div>
        {isPinnedSite && (
          <div className="absolute top-0 right-0 transform translate-x-1/2 -translate-y-1/2">
            <div className="w-4 h-4 rounded-full bg-gray-700 flex items-center justify-center shadow-sm">
              <PinIcon className="w-3 h-3 text-white" />
            </div>
          </div>
        )}
      </div>
      <span
        className="text-center text-gray-200 leading-tight max-w-full inline-block overflow-hidden text-ellipsis text-shadow-lg/20 group-hover:text-white transition-colors"
        title={site.title}
        style={{ fontSize: isFirefoxMode ? "12px" : "11px" }}
      >
        {truncateTopSiteTitle(site.title)}
      </span>
    </a>
  );
}
