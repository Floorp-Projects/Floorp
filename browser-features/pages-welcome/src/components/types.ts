import type { ReactNode } from "react";

export interface SetupSelectProps {
  id: string;
  label: string;
  hint?: string;
  icon?: ReactNode;
  value: string;
  disabled?: boolean;
  onChange: (value: string) => void;
  options: { value: string; label: string }[];
}

export interface NavigationProps {
  finalAction?: ReactNode;
}
export interface SetupSummaryData {
  language: string;
  engine: string;
}

export interface CurrentSettingProps {
  icon: ReactNode;
  label: string;
  value: string;
  detail?: string;
}
