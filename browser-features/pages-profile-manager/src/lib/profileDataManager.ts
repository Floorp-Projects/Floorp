declare global {
  interface Window {
    NRGetCurrentProfile?: (cb: (data: unknown) => void) => void;
    NRGetFxAccountsInfo?: (cb: (info: unknown) => void) => void;
    NRGetProfiles?: (cb: (res: unknown) => void) => void;
    NROpenUrl?: (url: string) => void;
    NROpenProfile?: (id: string) => void;
    NRCreateProfileWizard?: () => void;
    NRFlushProfiles?: () => void;
    NRRemoveProfile?: (
      id: string,
      deleteFiles: boolean,
      cb?: (res: unknown) => void,
    ) => void;
    NRRenameProfile?: (
      id: string,
      newName: string,
      cb?: (res: unknown) => void,
    ) => void;
    NRSetDefaultProfile?: (id: string, cb?: (res: unknown) => void) => void;
    NRRestart?: (safe: boolean) => void;
  }
}
declare global {
  interface GlobalThis {
    NRGetCurrentProfile?: (cb: (data: unknown) => void) => void;
    NRGetFxAccountsInfo?: (cb: (info: unknown) => void) => void;
    NRGetProfiles?: (cb: (res: unknown) => void) => void;
    NROpenUrl?: (url: string) => void;
    NROpenProfile?: (id: string) => void;
    NRCreateProfileWizard?: () => void;
    NRFlushProfiles?: () => void;
    NRRemoveProfile?: (
      id: string,
      deleteFiles: boolean,
      cb?: (res: unknown) => void,
    ) => void;
    NRRenameProfile?: (
      id: string,
      newName: string,
      cb?: (res: unknown) => void,
    ) => void;
    NRSetDefaultProfile?: (id: string, cb?: (res: unknown) => void) => void;
    NRRestart?: (safe: boolean) => void;
  }
}
export {};

// Shared helpers to avoid repeating unsafe `any` casts everywhere.
const __meta = import.meta as unknown as { env?: { DEV?: boolean } };
const __isDev = Boolean(__meta.env?.DEV);
const __g = globalThis as unknown as Record<string, unknown>;
const __hasChrome = typeof __g.ChromeUtils !== "undefined";

export async function getCurrentProfile(): Promise<{
  profileName: string;
  profilePath: string;
} | null> {
  // If we're running in a non-dev chrome context and privileged APIs are
  // available, perform the same direct operations as the Parent would.
  const isDev = __isDev;
  const hasChrome = __hasChrome;

  if (!isDev && hasChrome) {
    try {
      const Components = __g.Components as unknown as Record<string, unknown>;
      const dirService = Components?.classes?.[
        "@mozilla.org/file/directory_service;1"
      ]?.getService?.(
        Components?.interfaces?.nsIProperties,
      ) as unknown as Record<string, unknown>;

      const profileDir = (dirService as any).get(
        "ProfD",
        Components.interfaces.nsIFile,
      );
      const profilePath = profileDir.path;
      const profileName = profileDir.leafName;
      return { profileName, profilePath };
    } catch (e) {
      console.error("[profileDataManager] getCurrentProfile (privileged):", e);
      return null;
    }
  }

  // Fallback to actor/exported function (development or when ChromeUtils isn't present)
  return await new Promise((resolve) => {
    try {
      const getCurrentProfileFn = (globalThis as GlobalThis)
        .NRGetCurrentProfile;
      if (typeof getCurrentProfileFn === "function") {
        getCurrentProfileFn((data: unknown) => {
          if (!data) {
            resolve(null);
            return;
          }

          if (typeof data === "string") {
            try {
              const parsed = JSON.parse(data);

              resolve(parsed as { profileName: string; profilePath: string });
            } catch (err) {
              console.error(
                "[profileDataManager] getCurrentProfile: JSON parse failed",
                err,
              );
              resolve(null);
            }
          } else {
            resolve(data as { profileName: string; profilePath: string });
          }
        });
      } else {
        resolve(null);
      }
    } catch (e) {
      console.error("[profileDataManager] getCurrentProfile: exception", e);
      resolve(null);
    }
  });
}

export async function getProfiles(): Promise<unknown[]> {
  const isDev = __isDev;
  const hasChrome = __hasChrome;

  if (!isDev && hasChrome) {
    try {
      const Components = __g.Components as unknown as Record<string, unknown>;
      const profileService = Components.classes[
        "@mozilla.org/toolkit/profile-service;1"
      ]?.getService?.(
        Components.interfaces.nsIToolkitProfileService,
      ) as unknown as Record<string, unknown>;

      const defaultProfile = (() => {
        try {
          return profileService.defaultProfile;
        } catch (_) {
          return null;
        }
      })();

      const currentProfile = profileService.currentProfile;

      const profiles: any[] = [];
      for (const profile of profileService.profiles) {
        let isCurrentProfile = profile == currentProfile;
        let isInUse = isCurrentProfile;
        if (!isInUse) {
          try {
            let lock = profile.lock({});
            lock.unlock();
          } catch (err) {
            const r = (err as any)?.result;
            if (
              r != (globalThis as any).Cr?.NS_ERROR_FILE_NOT_DIRECTORY &&
              r != (globalThis as any).Cr?.NS_ERROR_FILE_NOT_FOUND
            ) {
              isInUse = true;
            }
          }
        }

        profiles.push({
          name: profile.name,
          rootDir: { path: profile.rootDir.path },
          localDir: { path: profile.localDir.path },
          isDefault: profile == defaultProfile,
          isCurrentProfile,
          isInUse,
        });
      }

      return profiles;
    } catch (e) {
      console.error("[profileDataManager] getProfiles (privileged):", e);
      return [];
    }
  }

  return await new Promise((resolve) => {
    try {
      const getProfilesFn = (globalThis as GlobalThis).NRGetProfiles;
      if (typeof getProfilesFn === "function") {
        getProfilesFn((res: unknown) => {
          let parsed: unknown = res;
          if (typeof res === "string") {
            try {
              parsed = JSON.parse(res);
            } catch (err) {
              console.error(
                "[profileDataManager] getProfiles: failed to parse string response",
                err,
              );
              resolve([]);
              return;
            }
          }

          if (Array.isArray(parsed)) {
            resolve(parsed as unknown[]);
          } else {
            resolve([]);
          }
        });
      } else {
        resolve([]);
      }
    } catch (e) {
      console.error("[profileDataManager] getProfiles: exception", e);
      resolve([]);
    }
  });
}

export async function getFxAccountsInfo(): Promise<unknown | null> {
  const isDev = __isDev;
  const hasChrome = __hasChrome;

  if (!isDev && hasChrome) {
    try {
      const FxAccounts = (globalThis as any).ChromeUtils.importESModule(
        "resource://gre/modules/FxAccounts.sys.mjs",
      ).getFxAccountsSingleton();

      const info = await FxAccounts.getSignedInUser();
      return info;
    } catch (e) {
      console.error("[profileDataManager] getFxAccountsInfo (privileged):", e);
      return null;
    }
  }

  return await new Promise((resolve) => {
    try {
      const getFxAccountsFn = (globalThis as GlobalThis).NRGetFxAccountsInfo;
      if (typeof getFxAccountsFn === "function") {
        getFxAccountsFn((info: unknown) => {
          if (typeof info === "string") {
            try {
              const parsed = JSON.parse(info);

              resolve(parsed);
              return;
            } catch (err) {
              console.error(
                "[profileDataManager] getFxAccountsInfo: failed to parse string",
                err,
              );
              resolve(null);
              return;
            }
          }
          resolve(info);
        });
      } else {
        resolve(null);
      }
    } catch (e) {
      console.error("[profileDataManager] getFxAccountsInfo: exception", e);
      resolve(null);
    }
  });
}

export function openUrl(url: string): void {
  const isDev = __isDev;
  const hasChrome = __hasChrome;
  if (!isDev && hasChrome) {
    try {
      const Services = (globalThis as any).Services;
      const win = Services.wm.getMostRecentWindow("navigator:browser");
      if (win) {
        try {
          (win as any).openUILinkIn(url, "tab");
        } catch {
          (win as any).gBrowser.addTab(url);
        }
      }
      return;
    } catch (e) {
      console.error("[profileDataManager] openUrl (privileged):", e);
    }
  }

  try {
    const openUrlFn = (globalThis as GlobalThis).NROpenUrl;
    if (typeof openUrlFn === "function") {
      openUrlFn(url);
    } else {
      console.warn("NROpenUrl not available");
    }
  } catch (e) {
    console.error(e);
  }
}

export function openProfile(profileIdentifier: string): void {
  const isDev = __isDev;
  const hasChrome = __hasChrome;
  if (!isDev && hasChrome) {
    try {
      const profileService = (globalThis as any).Components.classes[
        "@mozilla.org/toolkit/profile-service;1"
      ]?.getService?.(
        (globalThis as any).Components.interfaces.nsIToolkitProfileService,
      ) as any;

      let target = null;
      for (const p of profileService.profiles) {
        if (
          p.name === profileIdentifier ||
          p.rootDir?.path === profileIdentifier ||
          p.localDir?.path === profileIdentifier
        ) {
          target = p;
          break;
        }
      }
      if (target) {
        (globalThis as any).Services.startup.createInstanceWithProfile(target);
      }
      return;
    } catch (e) {
      console.error("[profileDataManager] openProfile (privileged):", e);
    }
  }

  try {
    const openProfileFn = (globalThis as GlobalThis).NROpenProfile;
    if (typeof openProfileFn === "function") {
      openProfileFn(profileIdentifier);
    }
  } catch (e) {
    console.error(e);
  }
}

export function createProfileWizard(): void {
  const isDev = __isDev;
  const hasChrome = __hasChrome;
  if (!isDev && hasChrome) {
    try {
      const win = (globalThis as any).Services.wm.getMostRecentWindow(
        "navigator:browser",
      );
      if (win) {
        (win as any).openDialog(
          "chrome://mozapps/content/profile/createProfileWizard.xhtml",
          "",
          "centerscreen,chrome,modal,titlebar",
        );
      }
      return;
    } catch (e) {
      console.error(
        "[profileDataManager] createProfileWizard (privileged):",
        e,
      );
    }
  }

  try {
    const createWizardFn = (globalThis as GlobalThis).NRCreateProfileWizard;
    if (typeof createWizardFn === "function") {
      createWizardFn();
    }
  } catch (e) {
    console.error(e);
  }
}

export function flushProfiles(): void {
  const isDev = __isDev;
  const hasChrome = __hasChrome;
  if (!isDev && hasChrome) {
    try {
      const profileService = (globalThis as any).Components.classes[
        "@mozilla.org/toolkit/profile-service;1"
      ]?.getService?.(
        (globalThis as any).Components.interfaces.nsIToolkitProfileService,
      ) as any;
      profileService.flush();
      return;
    } catch (e) {
      console.error("[profileDataManager] flushProfiles (privileged):", e);
    }
  }

  try {
    const flushFn = (globalThis as GlobalThis).NRFlushProfiles;
    if (typeof flushFn === "function") {
      flushFn();
    }
  } catch (e) {
    console.error(e);
  }
}

export async function removeProfile(
  profileIdentifier: string,
  deleteFiles = false,
): Promise<boolean> {
  const isDev = __isDev;
  const hasChrome = __hasChrome;
  if (!isDev && hasChrome) {
    try {
      const profileService = (globalThis as any).Components.classes[
        "@mozilla.org/toolkit/profile-service;1"
      ]?.getService?.(
        (globalThis as any).Components.interfaces.nsIToolkitProfileService,
      ) as any;

      let target = null;
      for (const p of profileService.profiles) {
        if (
          p.name === profileIdentifier ||
          p.rootDir?.path === profileIdentifier ||
          p.localDir?.path === profileIdentifier
        ) {
          target = p;
          break;
        }
      }
      if (target) {
        try {
          target.removeInBackground(Boolean(deleteFiles));
          return true;
        } catch (err) {
          console.error(
            "[profileDataManager] removeProfile (privileged) error:",
            err,
          );
          return false;
        }
      }
      return false;
    } catch (e) {
      console.error(e);
      return false;
    }
  }

  return await new Promise((resolve) => {
    try {
      const removeFn = (globalThis as GlobalThis).NRRemoveProfile;
      if (typeof removeFn === "function") {
        removeFn(profileIdentifier, deleteFiles, (res: unknown) =>
          resolve(Boolean(res)),
        );
      } else {
        resolve(false);
      }
    } catch (e) {
      console.error(e);
      resolve(false);
    }
  });
}

export async function renameProfile(
  profileIdentifier: string,
  newName: string,
): Promise<boolean> {
  const isDev = __isDev;
  const hasChrome = __hasChrome;
  if (!isDev && hasChrome) {
    try {
      const profileService = (globalThis as any).Components.classes[
        "@mozilla.org/toolkit/profile-service;1"
      ]?.getService?.(
        (globalThis as any).Components.interfaces.nsIToolkitProfileService,
      ) as any;

      let target = null;
      for (const p of profileService.profiles) {
        if (
          p.name === profileIdentifier ||
          p.rootDir?.path === profileIdentifier ||
          p.localDir?.path === profileIdentifier
        ) {
          target = p;
          break;
        }
      }
      if (target) {
        try {
          target.name = newName;
          return true;
        } catch (err) {
          console.error(
            "[profileDataManager] renameProfile (privileged) error:",
            err,
          );
          return false;
        }
      }
      return false;
    } catch (e) {
      console.error(e);
      return false;
    }
  }

  return await new Promise((resolve) => {
    try {
      const renameFn = (globalThis as GlobalThis).NRRenameProfile;
      if (typeof renameFn === "function") {
        renameFn(profileIdentifier, newName, (res: unknown) =>
          resolve(Boolean(res)),
        );
      } else {
        resolve(false);
      }
    } catch (e) {
      console.error(e);
      resolve(false);
    }
  });
}

export async function setDefaultProfile(
  profileIdentifier: string,
): Promise<boolean> {
  const isDev = __isDev;
  const hasChrome = __hasChrome;
  if (!isDev && hasChrome) {
    try {
      const profileService = (globalThis as any).Components.classes[
        "@mozilla.org/toolkit/profile-service;1"
      ]?.getService?.(
        (globalThis as any).Components.interfaces.nsIToolkitProfileService,
      ) as any;

      let target = null;
      for (const p of profileService.profiles) {
        if (
          p.name === profileIdentifier ||
          p.rootDir?.path === profileIdentifier ||
          p.localDir?.path === profileIdentifier
        ) {
          target = p;
          break;
        }
      }
      if (target) {
        try {
          profileService.defaultProfile = target;
          return true;
        } catch (err) {
          console.error(
            "[profileDataManager] setDefaultProfile (privileged) error:",
            err,
          );
          return false;
        }
      }
      return false;
    } catch (e) {
      console.error(e);
      return false;
    }
  }

  return await new Promise((resolve) => {
    try {
      const setDefaultFn = (globalThis as GlobalThis).NRSetDefaultProfile;
      if (typeof setDefaultFn === "function") {
        setDefaultFn(profileIdentifier, (res: unknown) =>
          resolve(Boolean(res)),
        );
      } else {
        resolve(false);
      }
    } catch (e) {
      console.error(e);
      resolve(false);
    }
  });
}

export function restart(safeMode = false): void {
  const isDev = __isDev;
  const hasChrome = __hasChrome;
  if (!isDev && hasChrome) {
    try {
      let cancelQuit = (globalThis as any).Cc[
        "@mozilla.org/supports-PRBool;1"
      ].createInstance((globalThis as any).Ci.nsISupportsPRBool);
      (globalThis as any).Services.obs.notifyObservers(
        cancelQuit,
        "quit-application-requested",
        "restart",
      );
      if (cancelQuit.data) {
        return;
      }
      const flags = (globalThis as any).Ci.nsIAppStartup
        ? (globalThis as any).Ci.nsIAppStartup.eAttemptQuit |
          (globalThis as any).Ci.nsIAppStartup.eRestart
        : 0;
      if (safeMode) {
        (globalThis as any).Services.startup.restartInSafeMode(flags);
      } else {
        (globalThis as any).Services.startup.quit(flags);
      }
      return;
    } catch (e) {
      console.error("[profileDataManager] restart (privileged):", e);
    }
  }

  try {
    const restartFn = (globalThis as GlobalThis).NRRestart;
    if (typeof restartFn === "function") {
      restartFn(safeMode);
    }
  } catch (e) {
    console.error(e);
  }
}
