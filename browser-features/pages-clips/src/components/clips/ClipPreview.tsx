import { File as FileIcon, Type } from "lucide-react";
import { useTranslation } from "react-i18next";
import type { Clip } from "@/types/clip.ts";
import { formatClipTime } from "@/lib/format.ts";

/**
 * One line's worth of a clip: enough to recognise it, not enough to read it.
 * Shown where a clip is about to be deleted, so the question "this one?"
 * has an answer in it.
 */
export function ClipPreview({ clip }: { clip: Clip }) {
  const { t, i18n } = useTranslation();
  const label = clip.kind === "text"
    ? (clip.text ?? "").split("\n")[0]
    : clip.fileName ?? (clip.kind === "image" ? t("clips.image") : t("clips.file"));

  return (
    <div
      className="flex items-center gap-2 rounded-lg bg-base-200 px-2.5 py-1.5 text-sm"
      data-testid="clips-preview"
    >
      {clip.kind === "image" && clip.preview
        ? (
          <img
            src={clip.preview}
            alt=""
            className="h-10 w-10 shrink-0 rounded object-cover"
          />
        )
        : clip.kind === "file"
        ? <FileIcon className="h-4 w-4 shrink-0 text-base-content/60" />
        : <Type className="h-4 w-4 shrink-0 text-base-content/60" />}
      <div className="min-w-0 flex-1">
        <p className="truncate" title={label}>{label}</p>
        <p className="text-xs text-base-content/60">
          {formatClipTime(clip.createdAt, i18n.language)}
        </p>
      </div>
    </div>
  );
}
