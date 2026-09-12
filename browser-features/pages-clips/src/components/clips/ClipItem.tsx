import { memo, useMemo, useState } from "react";
import {
  Copy,
  File as FileIcon,
  Pin,
  PinOff,
  X,
} from "lucide-react";
import { useTranslation } from "react-i18next";
import type { Clip } from "@/types/clip.ts";
import { formatSize, URL_PATTERN } from "@/lib/intake.ts";
import { formatClipTime } from "@/lib/format.ts";
import { clips as clipsRpc, localFile, openLinkInTab } from "@/lib/rpc/rpc.ts";
import type { FileAction } from "@/lib/settings.ts";

interface ClipItemProps {
  clip: Clip;
  fileAction: FileAction;
  onTogglePin: (clip: Clip) => void;
  onDelete: (id: string) => void;
  onZoom: (clip: Clip) => void;
  onError: (message: string | null) => void;
}

/**
 * The text with its URLs turned into links. A click opens the URL in a normal
 * tab rather than inside the panel; the href is still there so the link
 * reads, hovers, and drags like one.
 */
function withLinks(text: string): React.ReactNode[] {
  const parts: React.ReactNode[] = [];
  let last = 0;
  for (const m of text.matchAll(new RegExp(URL_PATTERN.source, "g"))) {
    const url = m[0];
    if (m.index > last) parts.push(text.slice(last, m.index));
    parts.push(
      <a
        key={m.index}
        href={url}
        className="link link-primary break-all"
        onClick={(e) => {
          e.preventDefault();
          void openLinkInTab(url);
        }}
      >
        {url}
      </a>,
    );
    last = m.index + url.length;
  }
  if (last < text.length) parts.push(text.slice(last));
  return parts;
}

/** What gets copied when the copy button is pressed. */
function copyText(clip: Clip): string {
  return clip.text ?? clip.filePath ?? clip.fileName ?? "";
}

/**
 * The compressed preview as a real File, so a clip whose original file is
 * gone can still be dropped onto a web page. Built without awaiting, because
 * a dragstart handler has no time to wait.
 */
function previewAsFile(clip: Clip): File | null {
  if (!clip.preview) return null;
  try {
    const [header, base64] = clip.preview.split(",");
    const type = header.match(/data:([^;]+)/)?.[1] ?? "image/jpeg";
    const binary = atob(base64);
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
    return new File([bytes], clip.fileName ?? "clip.jpg", { type });
  } catch (e) {
    console.error("[Floorp Clips] Could not rebuild the preview file:", e);
    return null;
  }
}

export const ClipItem = memo(function ClipItem({
  clip,
  fileAction,
  onTogglePin,
  onDelete,
  onZoom,
  onError,
}: ClipItemProps) {
  const { t, i18n } = useTranslation();
  const [copied, setCopied] = useState(false);

  const time = useMemo(
    () => formatClipTime(clip.createdAt, i18n.language),
    [clip.createdAt, i18n.language],
  );

  const handleCopy = async () => {
    const text = copyText(clip);
    if (!text) return;
    try {
      await navigator.clipboard.writeText(text);
      setCopied(true);
      setTimeout(() => setCopied(false), 1200);
    } catch (e) {
      console.error("[Floorp Clips] Failed to copy:", e);
    }
  };

  /**
   * Open a file clip the way the settings say to. A path that no longer
   * leads anywhere is an error up front, not a file manager pointed at
   * nothing.
   */
  const handleOpenFile = async () => {
    if (!clip.filePath || !(await clipsRpc.fileExists(clip.filePath))) {
      onError(t("clips.fileGone"));
      return;
    }
    const ok = fileAction === "launch"
      ? await clipsRpc.launchFile(clip.filePath)
      : await clipsRpc.revealFile(clip.filePath);
    onError(ok ? null : t("clips.fileGone"));
  };

  /**
   * Dragging a clip out. The original file goes first — it is the real thing,
   * uncompressed. If its path no longer leads anywhere, an image falls back to
   * its preview and a plain file has nothing left to give.
   */
  const handleDragStart = (event: React.DragEvent) => {
    const dt = event.dataTransfer;
    if (!dt) return;

    if (clip.kind === "text") {
      dt.setData("text/plain", clip.text ?? "");
      return;
    }

    const file = clip.filePath ? localFile(clip.filePath) : null;
    const moz = dt as unknown as {
      mozSetDataAt?: (type: string, data: unknown, index: number) => void;
    };
    try {
      if (file?.exists() && moz.mozSetDataAt) {
        moz.mozSetDataAt("application/x-moz-file", file, 0);
        return;
      }
    } catch (e) {
      console.error("[Floorp Clips] Could not hand over the file:", e);
    }

    const preview = previewAsFile(clip);
    if (preview) {
      dt.items.add(preview);
      return;
    }

    onError(t("clips.fileGone"));
    event.preventDefault();
  };

  return (
    <li className="group flex flex-col items-end gap-0.5" data-testid="clips-row">
      <div
        className="max-w-[85%] rounded-lg bg-base-200 px-2.5 py-1.5 text-sm"
        draggable
        onDragStart={handleDragStart}
      >
        {clip.kind === "text" && (
          <p className="whitespace-pre-wrap break-words select-text">
            {withLinks(clip.text ?? "")}
          </p>
        )}

        {clip.kind === "image" && clip.preview && (
          <button
            type="button"
            className="block cursor-zoom-in"
            onClick={() => onZoom(clip)}
            aria-label={clip.fileName ?? ""}
          >
            <img
              src={clip.preview}
              alt={clip.fileName ?? ""}
              className="max-h-48 rounded-lg"
            />
          </button>
        )}

        {clip.kind === "file" && (
          <button
            type="button"
            data-testid="clips-open-file"
            className="flex w-full items-center gap-2 text-left"
            onClick={() => void handleOpenFile()}
            title={fileAction === "launch"
              ? t("clips.launchFile")
              : t("clips.revealFile")}
          >
            <FileIcon className="h-4 w-4 shrink-0 text-base-content/60" />
            <div className="min-w-0">
              <p className="truncate font-medium" title={clip.fileName}>
                {clip.fileName ?? t("clips.file")}
              </p>
              <p className="text-xs text-base-content/60">
                {formatSize(clip.size)}
              </p>
            </div>
          </button>
        )}

        {(clip.kind === "image" || clip.kind === "file") && clip.filePath && (
          <p
            className="mt-1 truncate text-xs text-base-content/50"
            title={clip.filePath}
          >
            {clip.filePath}
          </p>
        )}
      </div>

      {/*
        The buttons live on the time line, not inside the bubble: the bubble
        holds only the clip, and the line below it already has a fixed height
        to hide them in, so nothing moves when they appear.
      */}
      <div className="flex h-6 items-center gap-1 pr-1 text-xs text-base-content/50">
        <span className="flex items-center gap-0.5 opacity-0 transition-opacity group-hover:opacity-100 group-focus-within:opacity-100">
          <button
            type="button"
            className="btn btn-xs btn-ghost btn-circle"
            aria-label={t("clips.copy")}
            title={copied ? t("clips.copied") : t("clips.copy")}
            onClick={() => void handleCopy()}
          >
            <Copy className="h-3 w-3" />
          </button>
          <button
            type="button"
            data-testid="clips-pin"
            className="btn btn-xs btn-ghost btn-circle"
            aria-pressed={clip.pinned}
            aria-label={clip.pinned ? t("clips.unpin") : t("clips.pin")}
            title={clip.pinned ? t("clips.unpin") : t("clips.pin")}
            onClick={() => onTogglePin(clip)}
          >
            {clip.pinned
              ? <PinOff className="h-3 w-3" />
              : <Pin className="h-3 w-3" />}
          </button>
          <button
            type="button"
            data-testid="clips-delete"
            className="btn btn-xs btn-ghost btn-circle"
            aria-label={t("clips.delete")}
            title={t("clips.delete")}
            onClick={() => onDelete(clip.id)}
          >
            <X className="h-3 w-3" />
          </button>
        </span>
        {clip.pinned && <Pin className="h-3 w-3" />}
        {time}
      </div>
    </li>
  );
});
