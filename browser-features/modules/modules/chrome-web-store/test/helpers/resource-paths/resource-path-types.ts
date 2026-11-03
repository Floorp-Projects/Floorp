// SPDX-License-Identifier: MPL-2.0
import type {
  NativeCorpusExtension,
  NativeCorpusWindow,
} from "../../../../../../../tools/chrome-extension-runtime/native-test-types.ts";
export type OracleWindow = NativeCorpusWindow;
export interface OracleExtension extends NativeCorpusExtension {
  backgroundState?: string;
  policy: { backgroundTypeModule: boolean };
}
export interface OracleParent {
  GlobalManager: { extensionMap: Map<string, OracleExtension> };
}
export interface OracleResult {
  worker?: boolean;
  module?: boolean;
  location?: string;
  scripts?: { id: string; js?: string[]; css?: string[] }[];
  snapshot?: {
    registered: string | null;
    direct: string | null;
    outline: string;
    border: string;
    href: string;
  };
  result?: { result?: string }[];
  inserted?: { result: { border: string } }[];
  removed?: { result: string }[];
  before?: string;
  rejected?: boolean;
  available?: boolean;
  returned?: string | null;
  errorName?: string;
}
export interface Envelope {
  serial: number;
  result?: OracleResult;
  error?: string;
}
export interface Toolbox {
  inspectorExtensionSidebars: Map<string, { title: string }>;
  selectTool(id: string): Promise<void>;
  getPanel(id: string): Inspector;
  destroy(): Promise<void>;
}
export interface Inspector {
  panelDoc: Document;
  sidebar: { show(id: string): void; getTabPanel(id: string): Element | null };
  store: { getState(): { extensionsSidebar: Record<string, unknown> } };
}
export interface DevtoolsModule {
  gDevTools: {
    showToolboxForTab(
      tab: NativeCorpusWindow["gBrowser"]["selectedTab"],
      options: { toolId: string; hostType: string },
    ): Promise<Toolbox>;
  };
}
