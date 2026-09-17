import darkLogo from "./assets/Floorp_Logo_B_Dark.svg";
import lightLogo from "./assets/Floorp_Logo_B_Light.svg";
import type { BrandProps } from "./types.ts";

// about: pages cannot resolve Vite's root-relative asset paths against their URL.
const darkLogoUrl = new URL(darkLogo, import.meta.url).href;
const lightLogoUrl = new URL(lightLogo, import.meta.url).href;

export function FloorpBrand({ onDark = false }: BrandProps) {
  return (
    <img
      src={onDark ? darkLogoUrl : lightLogoUrl}
      alt="Floorp"
      width="160"
      height="40"
      style={{ objectFit: "contain", objectPosition: "left center" }}
    />
  );
}
