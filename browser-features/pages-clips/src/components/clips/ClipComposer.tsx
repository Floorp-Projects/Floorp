import { useEffect, useRef, useState } from "react";
import { Paperclip, Plus } from "lucide-react";
import { useTranslation } from "react-i18next";
import { getActiveTabInfo } from "@/lib/rpc/rpc.ts";
import type { ActiveTabInfo } from "../../../../modules/common/defines.ts";

interface ClipComposerProps {
  onAddText: (text: string) => void;
  onAddFiles: (files: File[], paths: (string | undefined)[]) => void;
}

/** How often the suggestion button re-reads the active tab. */
const TAB_POLL_MS = 2000;

/** A file picked through the attach button carries its path as mozFullPath. */
function pathOf(file: File): string | undefined {
  const path = (file as unknown as { mozFullPath?: string }).mozFullPath;
  return typeof path === "string" && path.length > 0 ? path : undefined;
}

export function ClipComposer({ onAddText, onAddFiles }: ClipComposerProps) {
  const { t } = useTranslation();
  const [text, setText] = useState("");
  const [tab, setTab] = useState<ActiveTabInfo | null>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);

  // Only while the page is open — nothing watches in the background.
  useEffect(() => {
    let alive = true;
    const read = async () => {
      const info = await getActiveTabInfo();
      if (alive) setTab(info);
    };
    void read();
    const timer = setInterval(() => void read(), TAB_POLL_MS);
    return () => {
      alive = false;
      clearInterval(timer);
    };
  }, []);

  const submit = () => {
    const value = text.trim();
    if (!value) return;
    onAddText(value);
    setText("");
  };

  return (
    <div className="border-t border-base-300 bg-base-100 p-2">
      {tab?.url && (
        <button
          type="button"
          data-testid="clips-suggest"
          className="btn btn-sm btn-block btn-ghost mb-1.5 justify-start gap-1 font-normal"
          title={`${tab.title}\n${tab.url}`}
          onClick={() => onAddText(`${tab.title}\n${tab.url}`.trim())}
        >
          <Plus className="h-4 w-4 shrink-0" />
          <span className="shrink-0">{t("clips.addCurrentTab")}</span>
          <span className="truncate text-base-content/60">{tab.url}</span>
        </button>
      )}

      <div className="flex items-end gap-1">
        <textarea
          data-testid="clips-input"
          className="textarea textarea-bordered min-h-9 flex-1 resize-none text-sm"
          rows={1}
          value={text}
          placeholder={t("clips.inputPlaceholder")}
          onChange={(e) => setText(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === "Enter" && !e.shiftKey) {
              e.preventDefault();
              submit();
            }
          }}
          onPaste={(e) => {
            // Pasted images and files become clips; text falls through to
            // the textarea as plain text.
            const files = Array.from(e.clipboardData?.files ?? []);
            if (files.length === 0) return;
            e.preventDefault();
            onAddFiles(files, files.map(pathOf));
          }}
        />
        <button
          type="button"
          className="btn btn-sm btn-ghost btn-square"
          aria-label={t("clips.attach")}
          title={t("clips.attach")}
          onClick={() => fileInputRef.current?.click()}
        >
          <Paperclip className="h-4 w-4" />
        </button>
        <button
          type="button"
          className="btn btn-sm btn-primary"
          disabled={text.trim().length === 0}
          onClick={submit}
        >
          {t("clips.add")}
        </button>
      </div>

      <input
        ref={fileInputRef}
        type="file"
        multiple
        className="hidden"
        onChange={(e) => {
          const files = Array.from(e.target.files ?? []);
          if (files.length > 0) onAddFiles(files, files.map(pathOf));
          e.target.value = "";
        }}
      />
    </div>
  );
}
