import type { LucideIcon } from "lucide-react";
export interface SettingFieldDefinition {
  id: string;
  route: string;
  titleKey: string;
  descriptionKey?: string;
}
export interface SectionDefinition {
  id: string;
  route: string;
  titleKey: string;
  descriptionKey?: string;
  icon?: LucideIcon;
  priority: number;
  textKey: string;
}
export interface SettingsSearchDocument {
  id: string;
  route: string;
  title: string;
  icon?: LucideIcon;
  priority: number;
  textContent: string;
  preview: string;
  normalizedTitle: string;
  normalizedText: string;
}
