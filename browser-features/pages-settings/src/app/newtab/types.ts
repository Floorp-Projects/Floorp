// SPDX-License-Identifier: MPL-2.0
import type { ReactNode } from "react";
export type NewTabTab = "general" | "dial" | "feed" | "sponsors";
export interface NewTabSettingField {
  id: string;
  tab: NewTabTab;
  titleKey: string;
  descriptionKey?: string;
}
export interface SettingRowProps {
  id: string;
  title: string;
  hint?: string;
  children: ReactNode;
}
export interface SettingsSectionProps {
  title: string;
  description: string;
  children: ReactNode;
}
export interface DesignPreferenceTransport {
  getStringPrefState(
    name: string,
  ): Promise<{ value: string | null; typeMismatch: boolean }>;
  compareAndSetStringPref(
    name: string,
    expected: string | null,
    value: string,
  ): Promise<{
    updated: boolean;
    currentValue: string | null;
    typeMismatch?: boolean;
  }>;
}
