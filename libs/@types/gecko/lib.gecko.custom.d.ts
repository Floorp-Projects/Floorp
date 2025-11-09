/**
 * Custom artisanal types, handcrafted especially for Gecko.
 */

declare global {
  interface nsIMacSSBSupport extends nsISupports {
    install(id: string, name: string, icon?: imgIContainer | null): void;
    uninstall(id: string, name: string): void;
    applyDockIntegration(
      id: string,
      name: string,
      icon?: imgIContainer | null,
    ): void;
  }

  interface Ci {
    readonly nsIMacSSBSupport: nsJSIID<nsIMacSSBSupport>;
  }

  type DeclaredLazy<T> = {
    [P in keyof T]: T[P] extends () => infer U
      ? U
      : T[P] extends keyof LazyModules
        ? Exports<T[P], P>
        : T[P] extends { pref: string; default?: infer U }
          ? Widen<U>
          : T[P] extends { service: string; iid?: infer U }
            ? nsQIResult<U>
            : never;
  };

  type LazyDefinition = Record<
    string,
    | string
    | (() => any)
    | { service: string; iid: nsIID }
    | { pref: string; default?; onUpdate?; transform? }
  >;
}

type Exports<M, P> = M extends keyof LazyModules
  ? IfKey<LazyModules[M], P>
  : never;
type IfKey<T, K> = K extends keyof T ? T[K] : never;

type LazyModules = import("./generated/lib.gecko.modules").LazyModules;

type Widen<T> = T extends boolean
  ? boolean
  : T extends number
    ? number
    : T extends string
      ? string
      : never;

export {};
