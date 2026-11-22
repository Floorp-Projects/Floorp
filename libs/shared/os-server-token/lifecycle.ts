// SPDX-License-Identifier: MPL-2.0

import type {
  TokenDestination,
  TokenPathGuard,
  TokenStorage,
} from "./types.ts";

/** Preexisting-link defense, not atomic no-follow: a malicious same-user process
 * can still swap nodes between checks and operations. Windows profile mode is
 * unavailable until an audited native reparse-point guard exists. */
export function assertTokenPath(
  target: TokenDestination,
  guard: TokenPathGuard,
): void {
  if (!target.profileScoped) return; // legacy compatibility is not a new security claim
  if (guard.platform() === "WINNT") {
    throw new Error("Profile token files unavailable on Windows");
  }
  let path: string | null = target.path;
  while (path) {
    const kind = guard.inspect(path);
    if (kind === "link") throw new Error("Refusing linked token location");
    if (path === target.path ? kind === "directory" : kind === "file") {
      throw new Error("Unexpected token location type");
    }
    path = guard.parent(path);
  }
}

/** One process/profile owner. All mutations, including failed-write cleanup, serialize.
 * Gecko profile locking supplies cross-process exclusion for the profile destination;
 * the legacy compatibility location is not a cross-process ownership protocol. */
export class TokenFileLifecycle {
  private queue: Promise<void> = Promise.resolve();
  private generation = 0;
  private owned: TokenDestination | null = null;
  constructor(private readonly storage: TokenStorage) {}

  private enqueue(operation: () => Promise<void>): Promise<void> {
    const result = this.queue.then(operation);
    this.queue = result.catch(() => {});
    return result;
  }

  publish(destination: TokenDestination | null, token: string): Promise<void> {
    const generation = ++this.generation;
    destination = destination ? { ...destination } : null;
    return this.enqueue(async () => {
      if (generation !== this.generation) return;
      if (!destination) {
        // A refused opt-in also withdraws our previous advertisement.
        if (this.owned) {
          await this.storage.remove(this.owned);
          this.owned = null;
        }
        return;
      }
      if (this.owned && this.owned.path !== destination.path) {
        await this.storage.remove(this.owned);
        this.owned = null;
      }
      await this.storage.prepare(destination);
      // Preparation must establish a private directory before secret bytes exist.
      // Claim before writing so partial writes and permission failures are cleaned.
      this.owned = destination;
      try {
        await this.storage.write(destination, token);
        await this.storage.secure(destination);
        if (generation !== this.generation) {
          await this.storage.remove(destination);
          this.owned = null;
        }
      } catch (error) {
        await this.storage.remove(destination);
        this.owned = null;
        throw error;
      }
    });
  }

  cleanup(): Promise<void> {
    ++this.generation;
    return this.enqueue(async () => {
      if (!this.owned) return;
      await this.storage.remove(this.owned);
      this.owned = null;
    });
  }
}
