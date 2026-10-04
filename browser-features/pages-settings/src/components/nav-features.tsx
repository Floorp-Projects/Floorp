import type { LucideIcon } from "lucide-react";
import { ExternalLink } from "lucide-react";
import {
  SidebarGroup,
  SidebarGroupLabel,
  SidebarMenu,
  useSidebar,
} from "@/components/common/sidebar.tsx";
import { Link, useLocation } from "react-router-dom";
import type * as React from "react";
import styles from "./common/sidebar.module.css";

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
  const { setOpen } = useSidebar();
  return (
    <div>
      <Link
        to={feature.url}
        onClick={() => setOpen(false)}
        className={styles.link}
        aria-current={isActive ? "page" : undefined}
      >
        <feature.icon className="size-4" />
        <span>{feature.title}</span>
      </Link>
    </div>
  );
}

// External item renderer without SidebarMenuItem wrapper
function ExternalItem({ feature }: { feature: ExternalFeature }) {
  const { setOpen } = useSidebar();
  return (
    <div>
      <button
        type="button"
        onClick={(event) => {
          feature.onClick(event);
          setOpen(false);
        }}
        className={styles.link}
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

          if (feature.isExternal === true) {
            return <ExternalItem key={feature.title} feature={feature} />;
          }

          return (
            <InternalItem
              key={feature.title}
              feature={feature}
              isActive={isActive}
            />
          );
        })}
      </SidebarMenu>
    </SidebarGroup>
  );
}
