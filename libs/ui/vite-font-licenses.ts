import { readFileSync } from "node:fs";
import type { Plugin } from "vite";

// Run before genJarmnPlugin so the notices are included in the package manifest.
export function fontLicensesPlugin(): Plugin {
  return {
    name: "floorp-font-licenses",
    generateBundle() {
      for (const name of ["Inter", "NotoSansJP"]) {
        this.emitFile({
          type: "asset",
          fileName: `licenses/${name}-OFL.txt`,
          source: readFileSync(new URL(`./fonts/${name}-OFL.txt`, import.meta.url), "utf8"),
        });
      }
    },
  };
}
