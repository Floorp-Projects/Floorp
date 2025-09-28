import type { LucideIcon } from "lucide-react";
import {
  SidebarGroup,
  SidebarGroupLabel,
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
} from "@/components/common/sidebar.tsx";
import { Link, useLocation } from "react-router-dom";

export function NavFeatures({
  title,
  features,
}: {
  title: string;
  features: {
    title: string;
    url: string;
    icon: LucideIcon;
  }[];
}) {
  const location = useLocation();
  const currentRoute = location.pathname;

  return (
    <SidebarGroup>
      <SidebarGroupLabel>{title}</SidebarGroupLabel>
      <SidebarMenu>
        {features.map((feature) => {
          const featurePath = feature.url.startsWith("#")
            ? feature.url.slice(1)
            : feature.url;
          const isActive = featurePath === "/"
            ? currentRoute === "/"
            : currentRoute.startsWith(featurePath);

          return (
            <SidebarMenuItem key={feature.title}>
              <Link to={feature.url} className="block w-full">
                <SidebarMenuButton
                  className={`${
                    isActive ? "bg-base-300" : ""
                  } w-full flex items-center gap-2`}
                >
                  <feature.icon className="size-4" />
                  <span>{feature.title}</span>
                </SidebarMenuButton>
              </Link>
            </SidebarMenuItem>
          );
        })}
      </SidebarMenu>
    </SidebarGroup>
  );
}
