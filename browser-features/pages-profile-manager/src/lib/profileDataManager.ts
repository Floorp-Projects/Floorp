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

export async function getCurrentProfile(): Promise<{
  profileName: string;
  profilePath: string;
} | null> {
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
  try {
    const restartFn = (globalThis as GlobalThis).NRRestart;
    if (typeof restartFn === "function") {
      restartFn(safeMode);
    }
  } catch (e) {
    console.error(e);
  }
}
