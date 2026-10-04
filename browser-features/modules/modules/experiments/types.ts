// SPDX-License-Identifier: MPL-2.0

export type Variant = {
  id: string;
  weight?: number;
  configUrl?: string;
  [k: string]: unknown;
};

export type Experiment = {
  id: string;
  name?: string;
  description?: string;
  salt?: string;
  rollout?: number;
  start?: string;
  end?: string;
  variants?: Variant[];
  [k: string]: unknown;
};
