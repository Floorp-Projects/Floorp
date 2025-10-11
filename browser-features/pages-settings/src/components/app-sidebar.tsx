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

export function AppSidebar({ ...props }: React.ComponentProps<typeof Sidebar>) {
  const { t } = useTranslation();

  const overview = [
    { title: t("pages.home"), url: "/overview/home", icon: House },
  ];

  const features = [
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
    {
      title: "Floorp OS",
      url: "/features/floorp-os",
      icon: Cpu,
    },
    {
      title: t("pages.profileAndAccount"),
      url: "/features/accounts",
      icon: UserRoundPen,
    },
  ];

  const about = [
    { title: t("pages.aboutBrowser"), url: "/about/browser", icon: BadgeInfo },
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
