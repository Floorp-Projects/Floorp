import type { KeyboardEvent } from "react";
import { useTranslation } from "react-i18next";
import { Search } from "lucide-react";

declare global {
  var NRFocusUrlBar: (() => void) | undefined;
}

export function SearchBar() {
  const { t } = useTranslation();

  const activate = () => {
    if (globalThis.NRFocusUrlBar) {
      globalThis.NRFocusUrlBar();
    }
  };

  const handleKeyDown = (e: KeyboardEvent<HTMLDivElement>) => {
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();
      activate();
    }
  };

  return (
    <div className="relative w-full">
      <div
        className="group bg-gray-800/50 backdrop-blur-sm rounded-lg shadow-sm flex items-center p-2 cursor-pointer hover:bg-gray-700/50 transition-colors"
        onClick={activate}
        role="button"
        tabIndex={0}
        onKeyDown={handleKeyDown}
        aria-label={t("searchBar.searchOrEnterUrl")}
      >
        <div className="relative flex-1">
          <input
            type="text"
            placeholder={t("searchBar.searchOrEnterUrl")}
            className="w-full bg-transparent border-none outline-none px-2 py-1 text-gray-100 cursor-pointer group-hover:text-white transition-colors"
            readOnly
            tabIndex={-1}
          />
        </div>

        <div className="p-2 text-gray-400 group-hover:text-white transition-colors">
          <Search size={18} />
        </div>
      </div>
    </div>
  );
}
