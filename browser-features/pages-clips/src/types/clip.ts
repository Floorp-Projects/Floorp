/** What kind of thing a clip holds. */
export type ClipKind = "text" | "image" | "file";

/**
 * One clip. A clip is a small thing set aside for later — a bit of text, a
 * URL, an image, or a pointer to a local file.
 *
 * Only the fields that make sense for the kind are filled in.
 */
export interface Clip {
  id: string;
  kind: ClipKind;
  /** ms since epoch; also the sort key (oldest first, like a chat log). */
  createdAt: number;
  /**
   * When the clip last changed. A clip's body never changes, so in practice
   * this moves only when it is pinned or unpinned — which is exactly what
   * sync needs in order to tell two versions apart.
   */
  updatedAt: number;
  /** Pinned clips stay: they are excluded from the count limit and cleanup. */
  pinned: boolean;

  /** text: the plain text (rich text is flattened before it gets here). */
  text?: string;
  /** image: the compressed data URL shown inside the panel. */
  preview?: string;
  /** image, file: the original file name. */
  fileName?: string;
  /** image, file: the original local path, when the drop gave us one. */
  filePath?: string;
  /** file: MIME type as reported when it was added. */
  mimeType?: string;
  /** file: byte size when it was added. */
  size?: number;
}
