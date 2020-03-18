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
  gStore?: Store;
  gInit(perfFront: PerfFront, pageContext: PageContext): void;
  gDestroy(): void;
  gReportReady?(): void;
  gIsPanelDestroyed?: boolean;
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
  on: (type: string, listener: () => void) => void;
  off: (type: string, listener: () => void) => void;
  destroy: () => void,
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
  // it, or it was already started.
  | "recording"
  // Profiling is not available when in private browsing mode.
  | "locked-by-private-browsing";

// We are currently migrating to a new UX workflow with about:profiling.
// This type provides an easy way to change the implementation based
// on context.
export type PageContext =
  | "devtools"
  | "devtools-remote"
  | "aboutprofiling"
  | "aboutprofiling-remote";

export interface State {
  recordingState: RecordingState;
  recordingUnexpectedlyStopped: boolean;
  isSupportedPlatform: boolean;
  interval: number;
  entries: number;
  features: string[];
  threads: string[];
  objdirs: string[];
  presetName: string;
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

export type SetRecordingPreferences = (settings: RecordingStateFromPreferences) => void;

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
  presetName: string;
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
  // The current list of presets, loaded in from a JSM.
  presets: Presets;
  // Determine the current page context.
  pageContext: PageContext;
  // The popup and devtools panel use different codepaths for getting symbol tables.
  getSymbolTableGetter: (profile: object) => GetSymbolTableCallback;
  // The list of profiler features that the current target supports. Note that
  // this value is only null to support older Firefox browsers that are targeted
  // by the actor system. This compatibility can be required when the ESR version
  // is running at least Firefox 72.
  supportedFeatures: string[] | null
  // Allow different devtools contexts to open about:profiling with different methods.
  // e.g. via a new tab, or page navigation.
  openAboutProfiling?: () => void,
  // Allow about:profiling to switch back to the remote devtools panel.
  openRemoteDevTools?: () => void,
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
      presets: Presets;
      pageContext: PageContext;
      openAboutProfiling?: () => void,
      openRemoteDevTools?: () => void,
      recordingSettingsFromPreferences: RecordingStateFromPreferences;
      getSymbolTableGetter: (profile: object) => GetSymbolTableCallback;
      supportedFeatures: string[] | null;
    }
  | {
      type: "CHANGE_PRESET";
      presetName: string;
      preset: PresetDefinition | undefined;
    };

export interface InitializeStoreValues {
  perfFront: PerfFront;
  receiveProfile: ReceiveProfile;
  setRecordingPreferences: SetRecordingPreferences;
  presets: Presets;
  pageContext: PageContext;
  recordingPreferences: RecordingStateFromPreferences;
  supportedFeatures: string[] | null;
  getSymbolTableGetter: (profile: object) => GetSymbolTableCallback;
  openAboutProfiling?: () => void;
  openRemoteDevTools?: () => void;
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
   * The recording preferences by default are controlled by different presets.
   * This pref stores that preset.
   */
  Preset: "devtools.performance.recording.preset";
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
   * The profiler popup has some introductory text explaining what it is the first
   * time that you open it. After that, it is not displayed by default.
   */
  PopupIntroDisplayed: "devtools.performance.popup.intro-displayed";
  /**
   * This preference is used outside of the performance-new type system
   * (in DevToolsStartup). It toggles the availability of the profiler menu
   * button in the customization palette.
   */
  PopupFeatureFlag: "devtools.performance.popup.feature-flag";
}

/**
 * This interface represents the global values that are potentially on the window
 * object in the popup. Coerce the "window" object into this interface.
 */
export interface PopupWindow extends Window {
  gResizePopup?: (height: number) => void;
  gIsDarkMode?: boolean;
}

/**
 * Scale a number value.
 */
export type NumberScaler = (value: number) => number;

/**
 * A collection of functions to scale numbers.
 */
export interface ScaleFunctions {
  fromFractionToValue: NumberScaler,
  fromValueToFraction: NumberScaler,
  fromFractionToSingleDigitValue: NumberScaler,
}

export interface PresetDefinition {
  label: string;
  description: string;
  entries: number;
  interval: number;
  features: string[];
  threads: string[];
  duration: number;
}

export interface Presets {
  [presetName: string]: PresetDefinition;
}

export type MessageFromFrontend =
  | {
      type: "STATUS_QUERY";
      requestId: number;
    }
  | {
      type: "ENABLE_MENU_BUTTON";
      requestId: number;
    };

export type MessageToFrontend =
  | {
      type: "STATUS_RESPONSE";
      menuButtonIsEnabled: boolean;
      requestId: number;
    }
  | {
      type: "ENABLE_MENU_BUTTON_DONE";
      requestId: number;
    }

/**
 * This represents an event channel that can talk to a content page on the web.
 * This interface is a manually typed version of toolkit/modules/WebChannel.jsm
 * and is opinionated about the types of messages we can send with it.
 *
 * The definition is here rather than gecko.d.ts because it was simpler than getting
 * generics working with the ChromeUtils.import machinery.
 */
export class ProfilerWebChannel {
  constructor(id: string, url: MockedExports.nsIURI);
  send: (message: MessageToFrontend, target: MockedExports.WebChannelTarget) => void;
  listen: (
    handler: (idle: string, message: MessageFromFrontend, target: MockedExports.WebChannelTarget) => void
  ) => void;
}

/**
 * Describes all of the profiling features that can be turned on and
 * off in about:profiling.
 */
export interface FeatureDescription {
  // The name of the feature as shown in the UI.
  name: string,
  // The key value of the feature, this will be stored in prefs, and used in the
  // nsiProfiler interface.
  value: string,
  // The full description of the preset, this will need to be localized.
  title: string,
  // This will give the user a hint that it's recommended on.
  recommended?: boolean,
  // This will give a reason if the feature is disabled.
  disabledReason?: string,
}
