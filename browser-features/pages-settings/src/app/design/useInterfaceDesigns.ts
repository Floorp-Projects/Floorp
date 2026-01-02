import { useMemo } from "react";
import { useTheme } from "@nora/ui-components";
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

export const useInterfaceDesigns = (): {
  value: string;
  title: string;
  image: string;
}[] => {
  const { resolvedTheme } = useTheme();
  const { t } = useTranslation();

  return useMemo(() => {
    const isDark = resolvedTheme === "dark";
    return [
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
  }, [resolvedTheme, t]);
};
