import { useTranslation } from "react-i18next";
import { Search, X } from "lucide-react";

interface NoteSearchProps {
  value: string;
  onChange: (value: string) => void;
}

export function NoteSearch({ value, onChange }: NoteSearchProps) {
  const { t } = useTranslation();

  return (
    <div className="relative px-2 pt-2" onMouseDown={(e) => e.preventDefault()}>
      <Search className="absolute left-4 top-1/2 -translate-y-1/2 h-4 w-4 text-base-content/50 mt-1" />
      <input
        type="search"
        data-testid="notes-search"
        className="input input-sm input-bordered w-full pl-8 pr-8"
        placeholder={t("notes.search")}
        aria-label={t("notes.search")}
        value={value}
        onChange={(e) => onChange(e.target.value)}
        onMouseDown={(e) => e.stopPropagation()}
      />
      {value && (
        <button
          type="button"
          className="absolute right-4 top-1/2 -translate-y-1/2 mt-1"
          onClick={() => onChange("")}
          onMouseDown={(e) => e.stopPropagation()}
          aria-label={t("notes.clearSearch")}
        >
          <X className="h-3 w-3 text-base-content/50" />
        </button>
      )}
    </div>
  );
}
