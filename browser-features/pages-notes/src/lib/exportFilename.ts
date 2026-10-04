// SPDX-License-Identifier: MPL-2.0

/** Maximum length of the filename stem, excluding the extension. */
const MAX_STEM_LENGTH = 100;

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
    let stem = title
        .replace(FORBIDDEN_RE, "-")
        // Control characters become spaces rather than being deleted, so a
        // newline between two words does not glue them together.
        .replace(CONTROL_RE, " ")
        .replace(/\s+/g, " ")
        .trim();

    // Windows rejects names ending in a dot or a space.
    stem = stem.replace(/[. ]+$/, "");

    // Truncate by code point: slicing UTF-16 units can split a surrogate pair
    // and leave a lone surrogate in the filename.
    const points = [...stem];
    if (points.length > MAX_STEM_LENGTH) {
        stem = points.slice(0, MAX_STEM_LENGTH).join("").replace(/[. ]+$/, "");
    }

    if (stem.length === 0 || /^-+$/.test(stem)) {
        stem = fallback;
    }

    // Windows resolves device names up to the first dot, so "CON.txt" is
    // rejected just as "CON" is.
    if (RESERVED_RE.test(stem.split(".")[0])) {
        stem = `${stem}_`;
    }

    return `${stem}.${extension}`;
}
