// SPDX-License-Identifier: MPL-2.0

export interface ContextualEngine {
  name: string;
  identifier: string;
}

export interface Selector {
  getEngineConfiguration(): Promise<unknown[]>;
  findContextualSearchEngineByHost(
    host: string,
  ): Promise<ContextualEngine | null>;
  findContextualSearchEngineById(id: string): Promise<ContextualEngine | null>;
  reset(): void;
}

export interface RemoteSettingsClient {
  get(...args: unknown[]): Promise<unknown[]>;
}
