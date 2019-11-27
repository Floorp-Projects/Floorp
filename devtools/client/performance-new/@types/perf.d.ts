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
 * The actor version of the ActorReadyGeckoProfilerInterface returns promises,
 * while if it's instantiated directly it will not return promises.
 */
type MaybePromise<T> = Promise<T> | T;

/**
 * TS-TODO - Stub.
 *
 * Any method here that returns a MaybePromise<T> is because the
 * ActorReadyGeckoProfilerInterface returns T while the PerfFront returns Promise<T>.
 * Any method here that returns Promise<T> is because both the
 * ActorReadyGeckoProfilerInterface and the PerfFront return promises.
 */
export interface PerfFront {
  startProfiler: (options: RecordingStateFromPreferences) => MaybePromise<boolean>;
  getProfileAndStopProfiler: () => Promise<any>;
  stopProfilerAndDiscardProfile: () => MaybePromise<void>;
  getSymbolTable: (path: string, breakpadId: string) => Promise<[number[], number[], number[]]>;
  isActive: () => MaybePromise<boolean>;
  isSupportedPlatform: () => MaybePromise<boolean>;
  isLockedForPrivateBrowsing: () => MaybePromise<boolean>;
  /**
   * This method was was added in Firefox 72.
   */
  getSupportedFeatures: () => MaybePromise<string[]>
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
  promptEnvRestart: null | string
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

export type SetRecordingPreferences = (settings: RecordingStateFromPreferences) => MaybePromise<void>;

/**
 * This is the type signature for a function to restart the browser with a given
 * environment variable. Currently only implemented for the popup.
 */
export type RestartBrowserWithEnvironmentVariable =
    (envName: string, value: string) => void;

/**
 * This is the type signature for a function to query the browser for an
 * environment variable. Currently only implemented for the popup.
 */
export type GetEnvironmentVariable = (envName: string) => string;

/**
 * This is the type signature for a function to query the browser for the
 * ID of BrowsingContext of active tab.
 */
export type GetActiveBrowsingContextID = () => number;

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
  // The duration is currently not wired up to the UI yet. See Bug 1587165.
  duration?: number;
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
  // The list of profiler features that the current target supports. Note that
  // this value is only null to support older Firefox browsers that are targeted
  // by the actor system. This compatibility can be required when the ESR version
  // is running at least Firefox 72.
  supportedFeatures: string[] | null
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
      promptEnvRestart: string | null
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
      supportedFeatures: string[] | null;
    };

export interface InitializeStoreValues {
  perfFront: PerfFront;
  receiveProfile: ReceiveProfile;
  setRecordingPreferences: SetRecordingPreferences;
  isPopup: boolean;
  recordingPreferences: RecordingStateFromPreferences;
  supportedFeatures: string[] | null;
  getSymbolTableGetter: (profile: object) => GetSymbolTableCallback;
}

export type PopupBackgroundFeatures = { [feature: string]: boolean };

/**
 * The state of the profiler popup.
 */
export interface PopupBackgroundState {
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

/**
 * This interface serves as documentation for all of the prefs used by the
 * performance-new client. Each preference access string access can be coerced to
 * one of the properties of this interface.
 */
export interface PerformancePref {
  /**
   * Stores the total number of entries to be used in the profile buffer.
   */
  Entries: "devtools.performance.recording.entries";
  /**
   * The recording interval, stored in microseconds. Note that the StartProfiler
   * interface uses milliseconds, but this lets us store higher precision numbers
   * inside of an integer preference store.
   */
  Interval: "devtools.performance.recording.interval";
  /**
   * The features enabled for the profiler, stored as a comma-separated list.
   */
  Features: "devtools.performance.recording.features";
  /**
   * The threads to profile, stored as a comma-separated list.
   */
  Threads: "devtools.performance.recording.threads";
  /**
   * The location of the objdirs to use, stored as a comma-separated list.
   */
  ObjDirs: "devtools.performance.recording.objdirs";
  /**
   * The duration of the profiling window to use in seconds. Setting this to 0
   * will cause no profile window to be used, and the values will naturally roll
   * off from the profiling buffer.
   *
   * This is currently not hooked up to any UI. See Bug 1587165.
   */
  Duration: "devtools.performance.recording.duration";
  /**
   * Normally this defaults to https://profiler.firefox.com, but this can be overridden
   * to point the profiler to a different URL, such as http://localhost:4242/ for
   * local development workflows.
   */
  UIBaseUrl: "devtools.performance.recording.ui-base-url";
  /**
   * This pref allows tests to override the /from-addon in order to more easily
   * test the profile injection mechanism.
   */
  UIBaseUrlPathPref: "devtools.performance.recording.ui-base-url-path";
  /**
   * The profiler's menu button and its popup can be enabled/disabled by the user.
   * This pref controls whether the user has turned it on or not. Note that this
   * preference is also used outside of the type-checked files, so make sure
   * and update it elsewhere.
   */
  PopupEnabled: "devtools.performance.popup.enabled";
}

/**
 * This interface represents the global values that are potentially on the window
 * object in the popup. Coerce the "window" object into this interface.
 */
export interface PopupWindow extends Window {
  gResizePopup?: (height: number) => void;
}
