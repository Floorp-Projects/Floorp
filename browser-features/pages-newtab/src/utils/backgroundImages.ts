const backgroundImages = import.meta.glob("../assets/background/*.avif", {
  eager: true,
});

const floorpImages = import.meta.glob("../assets/floorp/*.png", {
  eager: true,
});

// Helper to extract a URL string from different module shapes that Vite/rollup
// might produce when importing assets. Modules could be simple strings (url),
// objects with a `default` property, or objects with a `url` field depending on
// the bundler and plugin configuration.
function extractUrlFromModule(mod: unknown): string | null {
  if (!mod) return null;
  if (typeof mod === "string") return mod;
  if (typeof mod === "object") {
    const m = mod as Record<string, unknown>;
    if (typeof m.default === "string") return m.default;
    if (typeof m.url === "string") return m.url;
  }
  return null;
}

const allUrls = Object.values(backgroundImages)
  .map(extractUrlFromModule)
  .filter((u): u is string => !!u);

// Filter results to image file URLs only. Some build outputs can place JS files or
// other artifacts in the same folder; ensure we only pick typical image extensions.
const imagePattern = /\.(avif|png|jpe?g|webp|gif)(\?.*)?$/i;
const imageUrls = allUrls.filter((u) => imagePattern.test(u));

// No runtime debug logs - production build should be quiet.

export function getRandomBackgroundImage(): string | null {
  if (!imageUrls || imageUrls.length === 0) {
    // eslint-disable-next-line no-console
    console.debug("[backgroundImages] no background images found");
    return null;
  }

  const randomIndex = Math.floor(Math.random() * imageUrls.length);
  const selected = imageUrls[randomIndex] as string;
  return selected;
}

export function getFloorpImages(): { name: string; url: string }[] {
  return Object.entries(floorpImages)
    .map(([path, mod]) => {
      const fileName = path.split("/").pop() || "";
      const url = extractUrlFromModule(mod);
      return url ? { name: fileName, url } : null;
    })
    .filter((x): x is { name: string; url: string } => x !== null);
}

export function getSelectedFloorpImage(
  imageName: string | null,
): string | null {
  if (!imageName) return null;

  const foundImage = Object.entries(floorpImages)
    .map(([path, mod]) => ({ path, url: extractUrlFromModule(mod) }))
    .find((entry) => entry.path.includes(imageName) && entry.url);

  return foundImage ? foundImage.url || null : null;
}
