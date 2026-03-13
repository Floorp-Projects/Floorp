import type { LucideIcon } from "lucide-react";
import { ExternalLink } from "lucide-react";
import {
  SidebarGroup,
  SidebarGroupLabel,
  SidebarMenu,
} from "@/components/common/sidebar.tsx";
import { Link, useLocation } from "react-router-dom";
import type * as React from "react";

// Discriminated union for feature items
type BaseFeature = {
  title: string;
  url: string;
  icon: LucideIcon;
};

type ExternalFeature = BaseFeature & {
  isExternal: true;
  onClick: (e: React.MouseEvent) => void;
};

type InternalFeature = BaseFeature & {
  isExternal?: false;
  onClick?: never;
};

export type Feature = InternalFeature | ExternalFeature;

// Internal item renderer without SidebarMenuItem wrapper
function InternalItem({
  feature,
  isActive,
}: {
  feature: InternalFeature;
  isActive: boolean;
}) {
  return (
    <div className="flex items-center gap-2 rounded-lg px-3 py-2 transition-colors">
      <Link
        to={feature.url}
        className={`${
          isActive
            ? "bg-primary text-primary-content"
            : "hover:bg-primary/30"
        } w-full flex items-center rounded-lg p-3 text-left gap-3`}
      >
        <feature.icon className="size-4" />
        <span>{feature.title}</span>
      </Link>
    </div>
  );
}

// External item renderer without SidebarMenuItem wrapper
function ExternalItem({ feature }: { feature: ExternalFeature }) {
  return (
    <div className="flex items-center gap-2 rounded-lg px-3 py-2 transition-colors">
      <button
        type="button"
        onClick={feature.onClick}
        className="hover:bg-primary/30 w-full flex items-center rounded-lg p-3 text-left gap-3"
      >
        <feature.icon className="size-4" />
        <span>{feature.title}</span>
        <ExternalLink className="size-3 ml-auto opacity-60" />
      </button>
    </div>
  );
}

export function NavFeatures({
  title,
  features,
}: {
  title: string;
  features: Feature[];
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

          if (feature.isExternal) {
            return <ExternalItem key={feature.title} feature={feature} />;
          }

          return <InternalItem key={feature.title} feature={feature} isActive={isActive} />;
        })}
      </SidebarMenu>
    </SidebarGroup>
  );
}
