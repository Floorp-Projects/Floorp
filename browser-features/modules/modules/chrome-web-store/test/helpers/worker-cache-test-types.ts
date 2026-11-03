// SPDX-License-Identifier: MPL-2.0
export interface PrivilegedCacheScope {
  CacheStorage: new (
    namespace: "chrome" | "content",
    principal: nsIPrincipal,
  ) => CacheStorage;
  Response: typeof Response;
  fetch(input: string): Promise<Response>;
}
