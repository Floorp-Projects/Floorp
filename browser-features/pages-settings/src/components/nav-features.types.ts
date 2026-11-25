import type { LucideIcon } from "lucide-react";
import type * as React from "react";

type BaseFeature = {
  reactKey?: string;
  title: string;
  url: string;
  icon: LucideIcon;
};

export type ExternalFeature = BaseFeature & {
  isExternal: true;
  onClick: (event: React.MouseEvent) => void;
};

export type InternalFeature = BaseFeature & {
  isExternal?: false;
  onClick?: never;
  disabled?: boolean;
  exactMatch?: boolean;
  focusKey?: string;
};

export type Feature = InternalFeature | ExternalFeature;

export type InternalItemProps = {
  feature: InternalFeature;
  isActive: boolean;
};

export type InternalItemFocusState = {
  node: HTMLElement | null;
  hadFocus: boolean;
};

export type NavigationFocusRemoval = {
  focusKey: string;
  node: HTMLElement;
};

export type NavigationFocusScopeContextValue = {
  recordFocusedRemoval: (focusKey: string, node: HTMLElement) => void;
};

export type NavigationFocusScopeProps = {
  children: React.ReactNode;
};

export type ExternalItemProps = {
  feature: ExternalFeature;
};

export type NavFeaturesProps = {
  title: string;
  features: Feature[];
};
