import { compressImage } from "@/lib/imageCompressor.ts";
import type { Clip } from "@/types/clip.ts";

function newClip(kind: Clip["kind"]): Clip {
  const now = Date.now();
  return {
    id: crypto.randomUUID(),
    kind,
    createdAt: now,
    updatedAt: now,
    pinned: false,
  };
}

export function clipFromText(text: string): Clip {
  return { ...newClip("text"), text };
}

/**
 * The local paths behind dropped files, when we can see them.
 *
 * A plain `File` only carries a name — the path is a chrome-only extra that
 * Gecko exposes as `application/x-moz-file`. In the dev server (a content
 * page) it is not there, and the clips simply have no path.
 *
 * The moz item list and `dt.files` are not the same list: a drop can carry
 * items that are not files at all, so the two indexes drift apart. Match on
 * the file name instead, and give up on the path when a name is ambiguous
 * rather than pin the wrong path to a clip.
 */
export function filePathsFromDataTransfer(
  dt: DataTransfer | null,
): (string | undefined)[] {
  if (!dt) return [];
  const moz = dt as unknown as {
    mozItemCount?: number;
    mozGetDataAt?: (type: string, index: number) => unknown;
  };

  const byName = new Map<string, string | undefined>();
  for (let i = 0; i < (moz.mozItemCount ?? 0); i++) {
    try {
      const file = moz.mozGetDataAt?.("application/x-moz-file", i) as
        | { path?: string; leafName?: string }
        | undefined;
      if (typeof file?.path !== "string" || typeof file.leafName !== "string") {
        continue;
      }
      // Same name twice: we cannot tell which file is which. Drop both paths.
      byName.set(file.leafName, byName.has(file.leafName) ? undefined : file.path);
    } catch {
      // Not a chrome page, or the item is not a file. No path, that is all.
    }
  }

  return Array.from(dt.files).map((file) => byName.get(file.name));
}

/**
 * Turn dropped/attached files into clips.
 *
 * Images get the same compression Notes uses, and the compressed copy is what
 * the panel shows; the original name and path are kept alongside it. Anything
 * else is recorded as a file — name, path, type, size — without a preview.
 */
export async function clipsFromFiles(
  files: File[],
  paths: (string | undefined)[],
): Promise<{ clips: Clip[]; failed: number }> {
  const clips: Clip[] = [];
  let failed = 0;

  for (const [index, file] of files.entries()) {
    const base = {
      fileName: file.name,
      filePath: paths[index],
      mimeType: file.type || undefined,
      size: file.size,
    };

    if (file.type.startsWith("image/")) {
      try {
        clips.push({
          ...newClip("image"),
          ...base,
          preview: await compressImage(file),
        });
        continue;
      } catch (e) {
        console.error("[Floorp Clips] Failed to compress an image:", e);
        failed++;
        continue;
      }
    }

    clips.push({ ...newClip("file"), ...base });
  }

  return { clips, failed };
}

/** The first web URL inside a text clip, if there is one. */
/** What counts as a URL inside a clip. Japanese punctuation ends one too. */
export const URL_PATTERN = /https?:\/\/[^\s<>"'。、]+/;

/**
 * An image dragged off a web page arrives without a File: Firefox hands over
 * the page's HTML for the drag, with the image's URL in it. Fetch that and
 * make the File ourselves. The URL itself is not kept — the picture is the
 * clip, not its address.
 */
export async function imageFromHtml(html: string): Promise<File | null> {
  const src = html.match(/<img[^>]+src=["']([^"']+)["']/i)?.[1];
  if (!src) return null;
  try {
    const blob = await (await fetch(src)).blob();
    if (!blob.type.startsWith("image/")) return null;
    const last = new URL(src, location.href).pathname.split("/").pop() ?? "";
    const name = decodeURIComponent(last) || "image";
    return new File([blob], name, { type: blob.type });
  } catch (e) {
    console.error("[Floorp Clips] Could not fetch the dragged image:", e);
    return null;
  }
}

/** Human-readable byte size, for file clips. */
export function formatSize(bytes: number | undefined): string {
  if (bytes === undefined) return "";
  const units = ["B", "KB", "MB", "GB"];
  let value = bytes;
  let unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit++;
  }
  return `${unit === 0 ? value : value.toFixed(1)} ${units[unit]}`;
}
