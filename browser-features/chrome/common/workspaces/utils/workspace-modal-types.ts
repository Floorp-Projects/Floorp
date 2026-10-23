// SPDX-License-Identifier: MPL-2.0

export type WorkspaceModalMode = "create" | "edit";

export interface WorkspaceModalTexts {
  name: string;
  namePlaceholder: string;
  container: string;
  noContainer: string;
  icon: string;
  editTitle: string;
  createTitle: string;
  createDescription: string;
  editDescription: string;
  create: string;
  save: string;
  cancel: string;
  newSession: string;
  chooseSession: string;
}

export interface WorkspaceSessionOption {
  name: string;
  userContextId: number;
}

export interface WorkspaceSessionFormLabels {
  label: string;
  standard: string;
  shareCurrent: string;
  newSession: string;
  chooseSession: string;
}
