import type { ReactNode } from "react";

export interface DemoPlaybackState {
  phase: number;
  revision: number;
  manual: boolean;
}

export interface DemoChromeProps {
  title: string;
  address: string;
  action?: ReactNode;
}
