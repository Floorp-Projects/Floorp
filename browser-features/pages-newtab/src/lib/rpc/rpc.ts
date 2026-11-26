import { moduleUrlServedByDevHttpServer } from "../../../../../libs/shared/dev-ports/defines.ts";
import type { NRSettingsParentFunctions } from "../../../../modules/common/defines.ts";
import { createBirpc } from "birpc";

declare global {
  interface Window {
    NRGetStringPref: (
      prefName: string,
      callback: (value: string | null) => void,
    ) => void;
    NRSetStringPref: (
      prefName: string,
      value: string,
      callback: () => void,
    ) => void;
    NRSettingsSend: (data: string) => void;
    NRSettingsRegisterReceiveCallback: (
      callback: (data: string) => void,
    ) => void;
  }
}

// Dev-mode detection is offset-agnostic (FDH Gate 2 task 2-B).
const isNewtabDev = moduleUrlServedByDevHttpServer(import.meta.url);

type StartPagePrefFunctions = Pick<
  NRSettingsParentFunctions,
  "getStringPref" | "setStringPref"
>;

const startPageActorFunctions: StartPagePrefFunctions = {
  getStringPref: (prefName) =>
    new Promise((resolve) =>
      (globalThis as unknown as Window).NRGetStringPref(prefName, resolve)
    ),
  setStringPref: (prefName, value) =>
    new Promise((resolve) =>
      (globalThis as unknown as Window).NRSetStringPref(
        prefName,
        value,
        () => resolve(),
      )
    ),
};

export const rpc = isNewtabDev
  ? createBirpc<NRSettingsParentFunctions, Record<string, never>>(
    {},
    {
      post: (data) => (globalThis as unknown as Window).NRSettingsSend(data),
      on: (callback) => {
        (globalThis as unknown as Window).NRSettingsRegisterReceiveCallback(
          callback,
        );
      },
      serialize: (v) => JSON.stringify(v),
      deserialize: (v) => JSON.parse(v),
    },
  )
  : startPageActorFunctions;
