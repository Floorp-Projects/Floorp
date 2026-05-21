// SPDX-License-Identifier: MPL-2.0

/**
 * Type-safe Event System with Proxy Dispatch
 *
 * Provides a type-safe mechanism for event dispatching and handling.
 * Acts as a proxy, so you can call methods directly on the instance.
 */
export class EventSystem<T extends object> {
  private handlers = new Map<keyof T, T[keyof T]>();

  constructor() {
    return new Proxy(this, {
      get: (target, prop) => {
        if (prop in target) {
          return (target as any)[prop];
        }
        // Return a dispatcher function
        return (...args: any[]) => {
          return target.dispatch(prop as keyof T, ...(args as any));
        };
      },
    });
  }

  /**
   * Register a handler for a specific event/method
   */
  on<K extends keyof T>(event: K, handler: T[K]): void {
    this.handlers.set(event, handler);
  }

  /**
   * Remove a handler
   */
  off<K extends keyof T>(event: K): void {
    this.handlers.delete(event);
  }

  /**
   * Register multiple handlers at once (Implementation)
   */
  implement(implementation: Partial<T>): void {
    for (const [key, handler] of Object.entries(implementation)) {
      if (typeof handler === "function") {
        this.on(key as keyof T, handler as any);
      }
    }
  }

  /**
   * Dispatch an event/call a method
   */
  dispatch<K extends keyof T>(
    event: K,
    ...args: T[K] extends (...args: infer P) => any ? P : never
  ): T[K] extends (...args: any[]) => infer R ? R | undefined : never {
    const handler = this.handlers.get(event);
    if (handler && typeof handler === "function") {
      return (handler as any)(...args);
    }
    return undefined as any;
  }

  /**
   * Check if an event has a handler
   */
  has<K extends keyof T>(event: K): boolean {
    return this.handlers.has(event);
  }
}

/**
 * Helper type to merge EventSystem API with the Event definitions
 */
export type EventSystemType<T extends object> = EventSystem<T> & T;

/**
 * Factory to create a properly typed Proxy-based EventSystem
 */
export function createEventSystem<T extends object>(): EventSystemType<T> {
  return new EventSystem<T>() as unknown as EventSystemType<T>;
}
