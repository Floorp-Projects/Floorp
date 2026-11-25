// SPDX-License-Identifier: MPL-2.0

import type { AppMenuOwner } from "../../../modules/modules/feature-host/AppMenuRegistry.sys.mts";

export interface HubPanelMenuFocusContext {
  readonly trigger: Element;
  readonly settings: Element;
  readonly isMenuOpen: () => boolean;
  readonly isForeground: () => boolean;
}

export interface OwnedHubPanelMenuView {
  readonly node: Element & { disabled: boolean; focus: () => void };
  readonly dispose: (reason?: "replacement" | "remove") => void;
  readonly setEnabled: (enabled: boolean) => void;
}

export interface HubPanelMenuViewOptions {
  readonly parent: Element;
  readonly marker: Element | null;
  readonly buttonId: string;
  readonly localizationKey: string;
  readonly enabled: boolean;
  readonly owner?: AppMenuOwner;
  readonly onCommand: () => void;
  readonly readLabel: (key: string, fallback: string) => unknown;
  readonly observeTranslations: (update: () => void) => void | (() => void);
  readonly reportFailure?: (owner: AppMenuOwner, error: Error) => void;
  readonly focusContext?: HubPanelMenuFocusContext;
}
