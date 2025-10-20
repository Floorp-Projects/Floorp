import {
  FormEvent,
  type KeyboardEvent,
  useEffect,
  useMemo,
  useState,
} from "react";
import { useLocation, useNavigate } from "react-router-dom";
import { useTranslation } from "react-i18next";
import { Search } from "lucide-react";
import { Input } from "@/components/common/input.tsx";
import { cn } from "@/lib/utils.ts";

function useSearchQuery() {
  const location = useLocation();
  return useMemo(() => {
    const params = new URLSearchParams(location.search);
    return params.get("q")?.trim() ?? "";
  }, [location.search]);
}

export function SettingsSearchInput() {
  const { t } = useTranslation();
  const navigate = useNavigate();
  const location = useLocation();
  const queryFromLocation = useSearchQuery();
  const [value, setValue] = useState(queryFromLocation);

  useEffect(() => {
    setValue(queryFromLocation);
  }, [queryFromLocation]);

  const handleSubmit = (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const trimmed = value.trim();
    if (!trimmed) {
      navigate({ pathname: "/search" });
      return;
    }

    navigate({
      pathname: "/search",
      search: `?q=${encodeURIComponent(trimmed)}`,
    });
  };

  const handleKeyDown = (event: KeyboardEvent<HTMLInputElement>) => {
    if (event.key === "Escape" && value) {
      event.preventDefault();
      setValue("");
      navigate({ pathname: "/search" });
    }
  };

  const placeholder = t("header.searchPlaceholder");

  return (
    <form
      role="search"
      aria-label={t("pages.search.title")}
      onSubmit={handleSubmit}
      className="relative"
    >
      <Search className="pointer-events-none absolute left-3 top-1/2 size-4 -translate-y-1/2 text-base-content/40" />
      <Input
        id="settings-search-input"
        type="search"
        value={value}
        onChange={(event) => {
          const v = event.target.value;
          setValue(v);
          const trimmed = v.trim();
          if (!trimmed) {
            // Clear query param without creating a new history entry
            navigate({ pathname: "/search" }, { replace: true });
            return;
          }

          const hasQ = new URLSearchParams(location.search).has("q");
          // If there is no existing `q` in the URL, push a new history entry so
          // that clicking a result and then pressing Back will restore the search.
          // For subsequent changes, replace the current entry to avoid bloating history.
          navigate(
            {
              pathname: "/search",
              search: `?q=${encodeURIComponent(trimmed)}`,
            },
            { replace: hasQ },
          );
        }}
        onKeyDown={handleKeyDown}
        placeholder={placeholder}
        aria-label={placeholder}
        autoComplete="off"
        className={cn("pl-9 pr-3", "bg-base-100/90 dark:bg-base-800/70")}
      />
    </form>
  );
}
