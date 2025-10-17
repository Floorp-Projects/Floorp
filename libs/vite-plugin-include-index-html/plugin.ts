// SPDX-License-Identifier: MPL-2.0

// Importing the Plugin type may require import-map or environment configuration.
// To avoid import resolution issues in different runtimes, we use an untyped export
// that is compatible with rollup/vite plugin shape.
export function includeIndexHtmlPlugin(opts?: { path?: string }) {
  return {
    name: "include-index-html-plugin",
    async generateBundle(
      _options,
      bundle: Record<string, { fileName?: string }>,
    ) {
      // If index.html already present anywhere in the bundle (root or nested), do nothing
      if (
        Object.values(bundle).some((v) => {
          const fn = v.fileName ?? "";
          return (
            fn === "index.html" ||
            fn.endsWith("/index.html") ||
            fn.endsWith("\\index.html")
          );
        })
      ) {
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
