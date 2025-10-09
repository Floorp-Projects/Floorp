import * as t from "io-ts";

/**
 * ワークスペースの詳細情報
 */
export interface WorkspaceDetail {
  name: string;
  tabs: unknown[];
  defaultWorkspace: boolean;
  id: string;
  icon: string | null;
  userContextId?: number;
  isPrivateContainerWorkspace?: boolean;
}

/**
 * 選択中ワークスペース情報
 */
export interface WorkspacePreference {
  selectedWorkspaceId?: string;
  defaultWorkspace?: string;
}

/**
 * ウィンドウ内のワークスペースレコード
 */
export type WindowWorkspaces = Record<
  string,
  WorkspaceDetail | WorkspacePreference
>;

/**
 * PF11以前のワークスペース全体構造
 */
export interface Floorp11Workspaces {
  windows: Record<string, WindowWorkspaces>;
}

// io-ts コーデック定義
export const zWorkspaceDetail = t.intersection([
  t.type({
    name: t.string,
    tabs: t.array(t.unknown),
    defaultWorkspace: t.boolean,
    id: t.string,
    icon: t.union([t.string, t.null]),
  }),
  t.partial({
    userContextId: t.number,
    isPrivateContainerWorkspace: t.boolean,
  }),
]);

export const zWorkspacePreference = t.partial({
  selectedWorkspaceId: t.string,
  defaultWorkspace: t.string,
});

export const zWindowWorkspaces = t.record(
  t.string,
  t.union([zWorkspaceDetail, zWorkspacePreference]),
);

export const zFloorp11WorkspacesSchema = t.type({
  windows: t.record(t.string, zWindowWorkspaces),
});
