// SPDX-License-Identifier: MPL-2.0

export interface SessionIndicatorTab extends Element {
  linkedBrowser: Element & {
    browsingContext?: { originAttributes?: { userContextId?: number } };
  };
  linkedPanel?: string;
}

export interface SessionIndicatorState {
  actualId: number;
  defaultId: number;
  pending: boolean;
  unavailable: boolean;
  exceptional: boolean;
  name: string;
  label: string;
}
