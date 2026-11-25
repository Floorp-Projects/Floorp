import { useMemo } from "react";
import type { DesignFormData } from "@/types/pref.ts";
import { useTheme } from "@/components/theme-provider";
import { useTranslation } from "react-i18next";

import ProtonLight from "@/app/design/images/designs/Floorp_UI_Proton_Light.svg";
import LeptonLight from "@/app/design/images/designs/Floorp_UI_Lepton_Light.svg";
import PhotonLight from "@/app/design/images/designs/Floorp_UI_Photon_Light.svg";
import ProtonFixLight from "@/app/design/images/designs/Floorp_UI_ProtonFix_Light.svg";
import FluerialLight from "@/app/design/images/designs/Floorp_UI_Fluerial_Light.svg";

import ProtonDark from "@/app/design/images/designs/Floorp_UI_Proton_Dark.svg";
import LeptonDark from "@/app/design/images/designs/Floorp_UI_Lepton_Dark.svg";
import PhotonDark from "@/app/design/images/designs/Floorp_UI_Photon_Dark.svg";
import ProtonFixDark from "@/app/design/images/designs/Floorp_UI_ProtonFix_Dark.svg";
import FluerialDark from "@/app/design/images/designs/Floorp_UI_Fluerial_Dark.svg";

export type InterfaceDesign = DesignFormData["design"];

/** The recommended design, also the default for new profiles. */
export const RECOMMENDED_INTERFACE_DESIGN: InterfaceDesign = "lepton";

/**
 * Designs built for Firefox's Proton UI, which Firefox replaced with Nova.
 * They stay selectable but are no longer recommended.
 */
export const DEPRECATED_INTERFACE_DESIGNS: ReadonlySet<InterfaceDesign> =
  new Set(["proton", "protonfix"]);

export const isDeprecatedInterfaceDesign = (design: InterfaceDesign) =>
  DEPRECATED_INTERFACE_DESIGNS.has(design);

export const useInterfaceDesigns = (): {
  value: InterfaceDesign;
  title: string;
  image: string;
  deprecated: boolean;
  recommended: boolean;
}[] => {
  const { resolvedTheme } = useTheme();
  const { t } = useTranslation();

  return useMemo(() => {
    const isDark = resolvedTheme === "dark";
    const designs: {
      value: InterfaceDesign;
      title: string;
      image: string;
    }[] = [
      {
        value: "proton",
        title: t("design.proton"),
        image: isDark ? ProtonDark : ProtonLight,
      },
      {
        value: "lepton",
        title: t("design.lepton"),
        image: isDark ? LeptonDark : LeptonLight,
      },
      {
        value: "photon",
        title: t("design.photon"),
        image: isDark ? PhotonDark : PhotonLight,
      },
      {
        value: "protonfix",
        title: t("design.protonfix"),
        image: isDark ? ProtonFixDark : ProtonFixLight,
      },
      {
        value: "fluerial",
        title: t("design.fluerial"),
        image: isDark ? FluerialDark : FluerialLight,
      },
    ];
    return designs.map((design) => ({
      ...design,
      deprecated: isDeprecatedInterfaceDesign(design.value),
      recommended: design.value === RECOMMENDED_INTERFACE_DESIGN,
    }));
  }, [resolvedTheme, t]);
};
