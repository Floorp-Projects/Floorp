// SPDX-License-Identifier: MPL-2.0

// Importing the Plugin type may require import-map or environment configuration.
// To avoid import resolution issues in different runtimes, we use an untyped export
// that is compatible with rollup/vite plugin shape.
export function includeIndexHtmlPlugin(opts?: {
  path?: string;
  isIndexOwner?: boolean;
}) {
  return {
    name: "include-index-html-plugin",
    async generateBundle(
      _options,
      bundle: Record<string, { fileName?: string }>,
    ) {
      // Determine whether bundle already contains index.html (any path)
      const hasIndexInBundle = Object.values(bundle).some((v) => {
        const fn = v.fileName ?? "";
        return (
          fn === "index.html" ||
          fn.endsWith("/index.html") ||
          fn.endsWith("\\index.html")
        );
      });

      // If an index is already present in bundle, don't emit anything.
      if (hasIndexInBundle) {
        return;
      }

      // If this plugin is not the designated index owner and the bundle is
      // missing index.html, do nothing. The 'owner' feature should set
      // isIndexOwner: true so that exactly one feature emits index.html.
      const isOwner = !!opts?.isIndexOwner;
      if (!isOwner) {
        return;
      }

      // Try to read index.html from the project root if available (Deno runtime)
      let content =
        "<!-- index.html emitted by include-index-plugin (stub) -->";
      const path = opts?.path ?? "index.html";
      try {
        // Use Deno API when available
        // eslint-disable-next-line @typescript-eslint/ban-ts-comment
        // @ts-ignore: Deno may be available in the environment
        if (
          typeof Deno !== "undefined" &&
          typeof Deno.readTextFile === "function"
        ) {
          // Deno.readTextFile is async
          content = await Deno.readTextFile(path);
        }
      } catch {
        // Ignore failure and fall back to stub
      }

      this.emitFile({
        type: "asset",
        fileName: "index.html",
        source: content,
      });
    },
  };
}
