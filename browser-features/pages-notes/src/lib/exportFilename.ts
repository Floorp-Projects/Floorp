// SPDX-License-Identifier: MPL-2.0

/** Maximum length of the filename stem, excluding the extension. */
const MAX_STEM_LENGTH = 100;

/** Common POSIX filesystems limit one filename component to 255 UTF-8 bytes. */
const MAX_FILENAME_BYTES = 255;

/** Characters that are illegal in filenames on at least one supported platform. */
const FORBIDDEN_RE = /[\\/:*?"<>|]/g;

/**
 * C0/C1 control characters plus the bidirectional overrides, which can be used
 * to disguise a file's extension in a file manager.
 */
// deno-lint-ignore no-control-regex
const CONTROL_RE = /[\u0000-\u001F\u007F-\u009F\u202A-\u202E\u2066-\u2069]/g;

/** Windows reserved device names, which cannot be used even with an extension. */
const RESERVED_RE = /^(con|prn|aux|nul|com[1-9]|lpt[1-9])$/i;

function sanitizeStem(value: string, maxStemBytes: number): string {
  let stem = value
    .replace(FORBIDDEN_RE, "-")
    // Control characters become spaces rather than being deleted, so a
    // newline between two words does not glue them together.
    .replace(CONTROL_RE, " ")
    .replace(/\s+/g, " ")
    .trim();

  // Windows rejects names ending in a dot or a space.
  stem = stem.replace(/[. ]+$/, "");

  // Windows resolves device names up to the first dot, so "CON.txt" is
  // rejected just as "CON" is.
  if (RESERVED_RE.test(stem.split(".")[0])) {
    stem = stem.replace(/^([^.]+)/, "$1_");
  }

  // Enforce both a readable length and the filesystem's byte limit. Counting
  // code points preserves surrogate pairs, but Japanese text and emoji take
  // multiple UTF-8 bytes each, so a character cap alone is insufficient.
  const encoder = new TextEncoder();
  let stemBytes = 0;
  let stemLength = 0;
  let truncated = "";
  for (const point of stem) {
    const pointBytes = encoder.encode(point).length;
    if (
      stemLength >= MAX_STEM_LENGTH || stemBytes + pointBytes > maxStemBytes
    ) {
      break;
    }
    truncated += point;
    stemBytes += pointBytes;
    stemLength++;
  }
  stem = truncated.replace(/[. ]+$/, "");
  return /^-+$/.test(stem) ? "" : stem;
}

/**
 * Converts a note title into a filename that is safe on Windows, macOS and
 * Linux. Note titles are free text and may be blank or, for sync conflict
 * copies, very long.
 */
export function toExportFilename(
  title: string,
  fallback: string,
  extension = "md",
): string {
  const maxStemBytes = MAX_FILENAME_BYTES -
    new TextEncoder().encode(`.${extension}`).length;
  const stem = sanitizeStem(title, maxStemBytes) ||
    sanitizeStem(fallback, maxStemBytes) ||
    sanitizeStem("Untitled", maxStemBytes);

  return `${stem}.${extension}`;
}
