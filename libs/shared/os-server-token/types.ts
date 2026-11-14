// SPDX-License-Identifier: MPL-2.0

export type TokenDestination = {
  directory: string;
  path: string;
  profileScoped: boolean;
};
export interface TokenPathGuard {
  platform(): string;
  inspect(path: string): "missing" | "link" | "directory" | "file";
  parent(path: string): string | null;
}

export interface TokenStorage {
  prepare(destination: TokenDestination): Promise<void>;
  write(destination: TokenDestination, token: string): Promise<void>;
  secure(destination: TokenDestination): Promise<void>;
  remove(destination: TokenDestination): Promise<void>;
}
