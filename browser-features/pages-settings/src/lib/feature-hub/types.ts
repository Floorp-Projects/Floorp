import type { ReactNode } from "react";

export type HubScalar = string | number | boolean;

export type HubTranslationParams = Record<string, HubScalar>;

export interface HubTranslationResult {
  value: string;
  found: boolean;
}

export type HubTranslation = (
  key: string,
  params?: HubTranslationParams,
) => HubTranslationResult;

export interface ActiveHubRequest {
  requestId: string;
  connectionNonce: string | null;
}

export interface HubAcceptedSnapshot {
  requestId: string;
  connectionNonce: string;
  epoch: string;
  revision: number;
}

export interface HubProviderProps {
  children: ReactNode;
}

export interface HubViewBoundaryProps {
  children: ReactNode;
}

export interface HubViewErrorBoundaryProps extends HubViewBoundaryProps {
  fallback: string;
}

export interface HubViewErrorBoundaryState {
  failed: boolean;
}

export interface HubNavigationCandidate {
  key: string;
  title: string;
  url: string;
  order: number;
}

export interface HubNavigationItem {
  key: string;
  title: string;
  url: string;
}

export interface HubEntry {
  featureId: string;
  ownerHostEpoch: string;
  ownerGeneration: number;
  localId: string;
  route: string;
  titleKey: string;
  navigation?: { labelKey: string; order?: number };
  view: {
    kind: "message";
    titleKey: string;
    bodyKey: string;
    params?: Readonly<Record<string, HubScalar>>;
  };
}

export interface HubSnapshot {
  epoch: string;
  hubRevision: number;
  entries: HubEntry[];
}

export interface HubEnvelope {
  requestId: string;
  connectionNonce: string;
  snapshot: HubSnapshot;
}

export interface HubState {
  status: "connecting" | "resyncing" | "ready" | "disconnected";
  requestId: string | null;
  connectionNonce: string | null;
  snapshot: HubSnapshot;
  renderedCommit: string | null;
  markRendered(commit: string | null): void;
  translate: HubTranslation;
}
