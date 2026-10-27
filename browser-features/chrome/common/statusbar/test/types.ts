// SPDX-License-Identifier: MPL-2.0

export type Fixture = {
  setEnabled: (enabled: boolean) => void;
  panel: HTMLElement;
  text: HTMLElement;
  label: HTMLElement;
  writes: () => number;
  dispose: () => void;
};
