/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains the shared types for the performance-new client.
 */

import {
  Reducer as ReduxReducer,
  Store as ReduxStore,
} from "devtools/client/shared/vendor/redux";

export interface PanelWindow {
  gToolbox?: any;
  gTarget?: any;
  gInit(perfFront: any, preferenceFront: any): void;
  gDestroy(): void;
}

/**
 * TS-TODO - Stub.
 */
export interface Target {
  // TODO
  client: any;
}

/**
 * TS-TODO - Stub.
 */
export interface Toolbox {
  target: Target;
}

/**
 * TS-TODO - Stub.
 */
export interface PerfFront {
  startProfiler: any;
  getProfileAndStopProfiler: any;
  stopProfilerAndDiscardProfile: any;
  getSymbolTable: any;
  isActive: any;
  isSupportedPlatform: any;
  isLockedForPrivateBrowsing: any;
}

/**
 * TS-TODO - Stub
 */
export interface PreferenceFront {
  clearUserPref: (prefName: string) => Promise<void>;
  getStringPref: (prefName: string) => Promise<string>;
  setStringPref: (prefName: string, value: string) => Promise<void>;
  getCharPref: (prefName: string) => Promise<string>;
  setCharPref: (prefName: string, value: string) => Promise<void>;
  getIntPref: (prefName: string) => Promise<number>;
  setIntPref: (prefName: string, value: number) => Promise<void>;
}

export type RecordingState =
  // The initial state before we've queried the PerfActor
  | "not-yet-known"
  // The profiler is available, we haven't started recording yet.
  | "available-to-record"
  // An async request has been sent to start the profiler.
  | "request-to-start-recording"
  // An async request has been sent to get the profile and stop the profiler.
  | "request-to-get-profile-and-stop-profiler"
  // An async request has been sent to stop the profiler.
  | "request-to-stop-profiler"
  // The profiler notified us that our request to start it actually started
  // it.
  | "recording"
  // Some other code with access to the profiler started it.
  | "other-is-recording"
  // Profiling is not available when in private browsing mode.
  | "locked-by-private-browsing";

export interface State {
  recordingState: RecordingState;
  recordingUnexpectedlyStopped: boolean;
  isSupportedPlatform: boolean;
  interval: number;
  entries: number;
  features: string[];
  threads: string[];
  objdirs: string[];
  initializedValues: InitializedValues | null;
}

export type Selector<T> = (state: State) => T;

export type ThunkDispatch = <Returns>(action: ThunkAction<Returns>) => Returns;
export type PlainDispatch = (action: Action) => Action;
export type GetState = () => State;
export type SymbolTableAsTuple = [Uint32Array, Uint32Array, Uint8Array];

/**
 * The `dispatch` function can accept either a plain action or a thunk action.
 * This is similar to a type `(action: Action | ThunkAction) => any` except this
 * allows to type the return value as well.
 */
export type Dispatch = PlainDispatch & ThunkDispatch;

export type ThunkAction<Returns> = (
  dispatch: Dispatch,
  getState: GetState
) => Returns;

export interface Library {
  start: number;
  end: number;
  offset: number;
  name: string;
  path: string;
  debugName: string;
  debugPath: string;
  breakpadId: string;
  arch: string;
}

export interface GeckoProfile {
  // Only type properties that we rely on.
}

export type GetSymbolTableCallback = (
  debugName: string,
  breakpadId: string
) => Promise<SymbolTableAsTuple>;

export type ReceiveProfile = (
  geckoProfile: GeckoProfile,
  getSymbolTableCallback: GetSymbolTableCallback
) => void;

export type SetRecordingPreferences = (settings: object) => void;

/**
 * This interface is injected into profiler.firefox.com
 */
interface GeckoProfilerFrameScriptInterface {
  getProfile: () => Promise<object>;
  getSymbolTable: GetSymbolTableCallback;
}

export interface RecordingStateFromPreferences {
  entries: number;
  interval: number;
  features: string[];
  threads: string[];
  objdirs: string[];
}

/**
 * A Redux Reducer that knows about the performance-new client's Actions.
 */
export type Reducer<S> = (state: S | undefined, action: Action) => S;

export interface InitializedValues {
  // The current Front to the Perf actor.
  perfFront: PerfFront;
  // A function to receive the profile and open it into a new window.
  receiveProfile: ReceiveProfile;
  // A function to set the recording settings.
  setRecordingPreferences: SetRecordingPreferences;
  // A boolean value that sets lets the UI know if it is in the popup window
  // or inside of devtools.
  isPopup: boolean;
  // The popup and devtools panel use different codepaths for getting symbol tables.
  getSymbolTableGetter: (profile: object) => GetSymbolTableCallback;
}

/**
 * Export a store that is opinionated about our State definition, and the union
 * of all Actions, as well as specific Dispatch behavior.
 */
export type Store = ReduxStore<State, Action>;

export type Action =
  | {
      type: "CHANGE_RECORDING_STATE";
      state: RecordingState;
      didRecordingUnexpectedlyStopped: boolean;
    }
  | {
      type: "REPORT_PROFILER_READY";
      isSupportedPlatform: boolean;
      recordingState: RecordingState;
    }
  | {
      type: "CHANGE_INTERVAL";
      interval: number;
    }
  | {
      type: "CHANGE_ENTRIES";
      entries: number;
    }
  | {
      type: "CHANGE_FEATURES";
      features: string[];
    }
  | {
      type: "CHANGE_THREADS";
      threads: string[];
    }
  | {
      type: "CHANGE_OBJDIRS";
      objdirs: string[];
    }
  | {
      type: "INITIALIZE_STORE";
      perfFront: PerfFront;
      receiveProfile: ReceiveProfile;
      setRecordingPreferences: SetRecordingPreferences;
      isPopup: boolean;
      recordingSettingsFromPreferences: RecordingStateFromPreferences;
      getSymbolTableGetter: (profile: object) => GetSymbolTableCallback;
    };

export type PopupBackgroundFeatures = { [feature: string]: boolean };

/**
 * The state of the profiler popup.
 */
export interface PopupBackgroundState {
  isRunning: boolean;
  settingsOpen: boolean;
  features: PopupBackgroundFeatures;
  buffersize: number;
  windowLength: number;
  interval: number;
  threads: string;
}

// TS-TODO - Stub
export interface ContentFrameMessageManager {
  addMessageListener: (event: string, listener: (event: any) => void) => void;
  addEventListener: (event: string, listener: (event: any) => void) => void;
  sendAsyncMessage: (name: string, data: any) => void;
}
