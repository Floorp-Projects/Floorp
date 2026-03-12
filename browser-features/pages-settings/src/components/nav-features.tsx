import type { LucideIcon } from "lucide-react";
import { ExternalLink } from "lucide-react";
import {
  SidebarGroup,
  SidebarGroupLabel,
  SidebarMenu,
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
    onClick?: (e: React.MouseEvent) => void;
    isExternal?: boolean;
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

          // External link with onClick handler
          if (feature.isExternal && feature.onClick) {
            return (
              <SidebarMenuItem key={feature.title}>
                <button
                  type="button"
                  onClick={feature.onClick}
                  className="hover:bg-primary/30 w-full flex items-center rounded-lg p-3 text-left gap-3"
                >
                  <feature.icon className="size-4" />
                  <span>{feature.title}</span>
                  <ExternalLink className="size-3 ml-auto opacity-60" />
                </button>
              </SidebarMenuItem>
            );
          }

          return (
            <SidebarMenuItem key={feature.title}>
              <Link to={feature.url} className="block w-full">
                <button
                  type="button"
                  className={`${
                    isActive
                      ? "bg-primary text-primary-content"
                      : "hover:bg-primary/30"
                  } w-full flex items-center rounded-lg p-3 text-left gap-3`}
                >
                  <feature.icon className="size-4" />
                  <span>{feature.title}</span>
                </button>
              </Link>
            </SidebarMenuItem>
          );
        })}
      </SidebarMenu>
    </SidebarGroup>
  );
}
