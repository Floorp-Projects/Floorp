// SPDX-License-Identifier: MPL-2.0

/** The hooks shared by Vite/Rollup and tsdown/Rolldown. */
export interface JarManifestPlugin {
  name: string;
  configResolved(config: { root: string }): void;
  generateBundle(
    this: {
      emitFile(asset: { type: "asset"; fileName: string; source: string }): string;
    },
    options: unknown,
    bundle: Record<string, { fileName: string }>,
    isWrite: boolean,
  ): Promise<void>;
}
