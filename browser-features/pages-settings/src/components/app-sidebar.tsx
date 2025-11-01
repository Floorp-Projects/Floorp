import type * as React from "react";
import { useTranslation } from "react-i18next";
import {
  BadgeInfo,
  Briefcase,
  Cpu,
  Grip,
  House,
  MousePointer,
  Option,
  PanelLeft,
  PencilRuler,
  RefreshCw,
  UserRoundPen,
} from "lucide-react";
import { NavHeader } from "@/components/nav-header.tsx";
import {
  Sidebar,
  SidebarContent,
  SidebarHeader,
  SidebarRail,
} from "@/components/common/sidebar.tsx";
import { NavFeatures } from "@/components/nav-features.tsx";
import { useEffect, useMemo, useState } from "react";
import { rpc } from "../lib/rpc/rpc.ts";

export function AppSidebar({ ...props }: React.ComponentProps<typeof Sidebar>) {
  const { t } = useTranslation();

  const overview = [
    { title: t("pages.home"), url: "/overview/home", icon: House },
  ];

  const [isFloorpOSEnabled, setIsFloorpOSEnabled] = useState<boolean | null>(
    null,
  );

  useEffect(() => {
    let mounted = true;
    (async () => {
      try {
        const val = await rpc.getBoolPref(
          "floorp.experiment.floorpos.enabled",
          false,
        );
        if (mounted) setIsFloorpOSEnabled(Boolean(val));
      } catch (e) {
        console.error(
          "failed to get pref floorp.experiment.floorpos.enabled",
          e,
        );
        if (mounted) setIsFloorpOSEnabled(false);
      }
    })();
    return () => {
      mounted = false;
    };
  }, []);

  const features = useMemo(() => [
    {
      title: t("pages.tabAndAppearance"),
      url: "/features/design",
      icon: PencilRuler,
    },
    {
      title: t("pages.browserSidebar"),
      url: "/features/sidebar",
      icon: PanelLeft,
    },
    {
      title: t("pages.mouseGesture"),
      url: "/features/gesture",
      icon: MousePointer,
    },
    {
      title: t("pages.workspaces"),
      url: "/features/workspaces",
      icon: Briefcase,
    },
    {
      title: t("pages.keyboardShortcuts"),
      url: "/features/shortcuts",
      icon: Option,
    },
    { title: t("pages.webApps"), url: "/features/webapps", icon: Grip },
    // Floorp OS entry is conditional based on pref floorp.experiment.floorpos.enabled
    ...(isFloorpOSEnabled
      ? [
        {
          title: "Floorp OS",
          url: "/features/floorp-os",
          icon: Cpu,
        },
      ]
      : []),
    {
      title: t("pages.profileAndAccount"),
      url: "/features/accounts",
      icon: UserRoundPen,
    },
  ], [isFloorpOSEnabled, t]);

  const about = [
    { title: t("pages.aboutBrowser"), url: "/about/browser", icon: BadgeInfo },
    { title: t("pages.updates"), url: "/about/updates", icon: RefreshCw },
  ];

  return (
    <Sidebar {...props}>
      <SidebarHeader>
        <NavHeader />
      </SidebarHeader>
      <SidebarContent>
        <NavFeatures
          title={t("sidebar.overview")}
          features={overview}
        />
        <NavFeatures
          title={t("sidebar.features")}
          features={features}
        />
        <NavFeatures
          title={t("sidebar.about")}
          features={about}
        />
      </SidebarContent>
      {
        /* <SidebarFooter>
        <NavUser user={user} />
      </SidebarFooter> */
      }
      <SidebarRail />
    </Sidebar>
  );
}
