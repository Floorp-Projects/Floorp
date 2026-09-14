import darkLogo from "./assets/Floorp_Logo_B_Dark.svg";
import lightLogo from "./assets/Floorp_Logo_B_Light.svg";
import type { BrandProps } from "./types.ts";

export function FloorpBrand({ onDark = false }: BrandProps) {
  return (
    <img
      src={onDark ? darkLogo : lightLogo}
      alt="Floorp"
      width="160"
      height="40"
      style={{ objectFit: "contain", objectPosition: "left center" }}
    />
  );
}
