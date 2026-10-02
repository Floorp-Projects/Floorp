// SPDX-License-Identifier: MPL-2.0

import { isDevMode } from "./env.ts";

// In production this page is loaded from `chrome://noraneko-notes/content/`
// inside a non-remote <xul:browser>, so it runs with the system principal and
// these globals exist. Under the Vite dev server they do not.
// deno-lint-ignore no-explicit-any
declare const Cc: any;
// deno-lint-ignore no-explicit-any
declare const Ci: any;
// deno-lint-ignore no-explicit-any
declare const IOUtils: any;
/** Chrome-only Window property; the file picker anchors its dialog to it. */
// deno-lint-ignore no-explicit-any
declare const browsingContext: any;

export interface SaveTextFileOptions {
    /** File contents. */
    text: string;
    /** Filename offered in the dialog, extension included. */
    suggestedName: string;
    /** Extension appended if the user deletes it, e.g. "md". */
    defaultExtension: string;
    /** Human-readable filter label, e.g. "Markdown". */
    filterLabel: string;
    /** Filter glob, e.g. "*.md". */
    filterPattern: string;
    /** Dialog window title. */
    dialogTitle: string;
}

/**
 * Shows a "Save As" dialog and writes `text` to the chosen path.
 *
 * Resolves `true` when a file was written and `false` when the user cancelled.
 */
async function saveViaFilePicker(
    options: SaveTextFileOptions,
): Promise<boolean> {
    const picker = Cc["@mozilla.org/filepicker;1"].createInstance(
        Ci.nsIFilePicker,
    );
    picker.init(browsingContext, options.dialogTitle, Ci.nsIFilePicker.modeSave);
    picker.appendFilter(options.filterLabel, options.filterPattern);
    picker.appendFilters(Ci.nsIFilePicker.filterAll);
    picker.defaultString = options.suggestedName;
    picker.defaultExtension = options.defaultExtension;

    const result: number = await new Promise((resolve) => picker.open(resolve));
    if (result === Ci.nsIFilePicker.returnCancel) {
        return false;
    }

    await IOUtils.writeUTF8(picker.file.path, options.text);
    return true;
}

/**
 * Dev-server fallback: the page has a content principal here, so `nsIFilePicker`
 * is unavailable. Hand the file to the download manager instead.
 *
 * This always reports success — a download has no cancel signal the page can
 * observe — so the `false` return of `saveTextFile` means "user cancelled" only
 * in production.
 */
function saveViaDownloadLink(options: SaveTextFileOptions): boolean {
    const blob = new Blob([options.text], {
        type: "text/plain;charset=utf-8",
    });
    const url = URL.createObjectURL(blob);
    try {
        const anchor = document.createElement("a");
        anchor.href = url;
        anchor.download = options.suggestedName;
        document.body.appendChild(anchor);
        anchor.click();
        anchor.remove();
        return true;
    } finally {
        // Revoke on the next turn so the download has started.
        setTimeout(() => URL.revokeObjectURL(url), 0);
    }
}

/**
 * Writes a text file to disk.
 *
 * Production uses a real "Save As" dialog; the Vite dev server falls back to a
 * download, mirroring the environment split already used by `rpc.ts`.
 */
export function saveTextFile(
    options: SaveTextFileOptions,
): Promise<boolean> {
    if (isDevMode) {
        return Promise.resolve(saveViaDownloadLink(options));
    }
    return saveViaFilePicker(options);
}
