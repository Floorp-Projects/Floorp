// SPDX-License-Identifier: MPL-2.0

export interface HubDomProbeLink {
  label: string;
  route: string;
}

export interface HubDomProbeRead {
  documentURI: string;
  hash: string;
  settingsRootReady: boolean;
  messageTitle: string | null;
  messageBody: string | null;
  unavailableTitle: string | null;
  unavailableBody: string | null;
  dynamicLinks: HubDomProbeLink[];
  builtInHomeLink: HubDomProbeLink | null;
  disabledNavigationLabels: string[];
  pageshow: {
    count: number;
    persistedCount: number;
    lastPersisted: boolean | null;
  };
  protocol: {
    connectCount: number;
    blockedConnectCount: number;
    blockedAckCount: number;
    disconnectCount: number;
    connectBlocked: boolean;
    ackBlocked: boolean;
    savedAcknowledgementAvailable: boolean;
    snapshotCount: number;
    ackCount: number;
    latest: {
      requestId: string | null;
      connectionNonce: string | null;
      epoch: string | null;
      revision: number | null;
    };
    lastAcknowledgement: {
      requestId: string | null;
      connectionNonce: string | null;
      epoch: string | null;
      revision: number | null;
    };
  };
}

export type HubDomProbeCommand =
  | "Read"
  | "ClickRoute"
  | "UnblockConnect"
  | "BlockReactAcknowledgements"
  | "UnblockReactAcknowledgements"
  | "ReplaySavedReactAcknowledgement"
  | "CreateSameOriginFrame"
  | "RemoveSameOriginFrame"
  | "FaultInjectTransportDisconnect"
  | "Resync";

export type HubDomProbeTestWindowGlobal =
  & Parameters<
    typeof import("../HubPageDiscovery.sys.mts").HubPageDiscoveryService.connectActor
  >[1]
  & {
    readonly innerWindowId: number;
  };

export type HubDomProbeTestActor = Parameters<
  typeof import("../HubPageDiscovery.sys.mts").HubPageDiscoveryService.connectActor
>[0];

export interface HubDomProbeActorLookup {
  readonly actor: HubDomProbeTestActor | null;
  readonly registrationError: string | null;
}

export interface HubDomProbeTestBrowsingContext {
  readonly currentWindowGlobal: HubDomProbeTestWindowGlobal | null;
  readonly children: readonly HubDomProbeTestBrowsingContext[];
}

export type HubDomProbeTestTab = XULElement & {
  linkedBrowser: {
    browsingContext:
      & HubDomProbeTestWindowGlobal["browsingContext"]
      & HubDomProbeTestBrowsingContext;
    loadURI?: (
      uri: nsIURI,
      options: { triggeringPrincipal: nsIPrincipal },
    ) => void;
    webNavigation?: {
      readonly canGoBack: boolean;
      goBack(): void;
    };
  };
};

export type HubDomProbeTestGBrowser = {
  addTab(
    url: string,
    options: {
      inBackground?: boolean;
      triggeringPrincipal: nsIPrincipal;
    },
  ): HubDomProbeTestTab;
  selectedTab: HubDomProbeTestTab;
  removeTab(tab: HubDomProbeTestTab): void;
};

export interface HubDomProbeTestBrowserWindow extends Window {
  gBrowser?: HubDomProbeTestGBrowser;
  gBrowserInit?: { delayedStartupFinished?: boolean };
}

export type HubDomProbeWindowIdentityModule =
  typeof import("../WindowIdentity.sys.mts");
export type HubDomProbeTestUtilsModule =
  typeof import("./HubDomProbeTestUtils.sys.mts");
