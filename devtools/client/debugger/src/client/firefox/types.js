/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * These are Firefox specific types that allow us to type check
 * the packet information exchanged using the Firefox Remote Debug Protocol
 * https://wiki.mozilla.org/Remote_Debugging_Protocol
 */

import type {
  BreakpointLocation,
  BreakpointOptions,
  FrameId,
  ActorId,
  Script,
  Pause,
  PendingLocation,
  Frame,
  SourceId,
  QueuedSourceData,
  Worker,
  Range,
} from "../../types";

import type { EventListenerCategoryList } from "../../actions/types";

type URL = string;

/**
 * The protocol is carried by a reliable, bi-directional byte stream; data sent
 * in both directions consists of JSON objects, called packets. A packet is a
 * top-level JSON object, not contained inside any other value.
 *
 * Every packet sent to the client has the form:
 *  `{ "to":actor, "type":type, ... }`
 *
 * where actor is the name of the actor to whom the packet is directed and type
 * is a string specifying what sort of packet it is. Additional properties may
 * be present, depending on type.
 *
 * Every packet sent from the server has the form:
 *  `{ "from":actor, ... }`
 *
 * where actor is the name of the actor that sent it. The packet may have
 * additional properties, depending on the situation.
 *
 * If a packet is directed to an actor that no longer exists, the server
 * sends a packet to the client of the following form:
 *  `{ "from":actor, "error":"noSuchActor" }`
 *
 * where actor is the name of the non-existent actor. (It is strange to receive
 * messages from actors that do not exist, but the client evidently believes
 * that actor exists, and this reply allows the client to pair up the error
 * report with the source of the problem.)
 *
 * Clients should silently ignore packet properties they do not recognize. We
 * expect that, as the protocol evolves, we will specify new properties that
 * can appear in existing packets, and experimental implementations will do
 * the same.
 *
 * @see https://wiki.mozilla.org/Remote_Debugging_Protocol#Packets
 * @memberof firefox/packets
 * @static
 */

/**
 * Frame Packet
 * @memberof firefox/packets
 * @static
 */
export type FramePacket = {
  actor: ActorId,
  arguments: any[],
  displayName: string,
  environment: any,
  this: any,
  depth?: number,
  oldest?: boolean,
  type: "pause" | "call",
  where: {| actor: string, line: number, column: number |},
};

/**
 * Firefox Source File payload
 * introductionType can be a "scriptElement"
 * @memberof firefox/payloads
 * @static
 */
export type SourcePayload = {
  actor: ActorId,
  url: URL | null,
  isBlackBoxed: boolean,
  sourceMapURL: URL | null,
  introductionUrl: URL | null,
  introductionType: string | null,
  extensionName: string | null,
};

/**
 * Source Packet sent when there is a "new source" event
 * coming from the debug server
 * @memberof firefox/packets
 * @static
 */
export type SourcePacket = {
  from: ActorId,
  source: SourcePayload,
  type: string,
};

/**
 * Sources Packet from calling threadFront.getSources();
 * @memberof firefox/packets
 * @static
 */
export type SourcesPacket = {
  from: ActorId,
  sources: SourcePayload[],
};

/**
 * Pause Packet sent when the server is in a "paused" state
 *
 * @memberof firefox
 * @static
 */
export type PausedPacket = {
  actor: ActorId,
  from: ActorId,
  type: string,
  frame: FramePacket,
  why: {
    actors: ActorId[],
    type: string,
    onNext?: Function,
  },
};

/**
 * Response from the `getFrames` function call
 * @memberof firefox
 * @static
 */
export type FramesResponse = {
  frames: FramePacket[],
  from: ActorId,
};

export type TabPayload = {
  actor: ActorId,
  animationsActor: ActorId,
  consoleActor: ActorId,
  cssPropertiesActor: ActorId,
  directorManagerActor: ActorId,
  emulationActor: ActorId,
  eventLoopLagActor: ActorId,
  framerateActor: ActorId,
  inspectorActor: ActorId,
  memoryActor: ActorId,
  monitorActor: ActorId,
  outerWindowID: number,
  performanceActor: ActorId,
  performanceEntriesActor: ActorId,
  profilerActor: ActorId,
  promisesActor: ActorId,
  reflowActor: ActorId,
  storageActor: ActorId,
  styleEditorActor: ActorId,
  styleSheetsActor: ActorId,
  timelineActor: ActorId,
  title: string,
  url: URL,
  webExtensionInspectedWindowActor: ActorId,
};

/**
 * Actions
 * @memberof firefox
 * @static
 */
export type Actions = {
  paused: Pause => void,
  resumed: ActorId => void,
  newQueuedSources: (QueuedSourceData[]) => void,
  fetchEventListeners: () => void,
  updateWorkers: () => void,
};

/**
 * Tab Target gives access to the browser tabs
 * @memberof firefox
 * @static
 */
export type TabTarget = {
  on: (string, Function) => void,
  emit: (string, any) => void,
  threadFront: ThreadFront,
  activeConsole: {
    evaluateJS: (
      script: Script,
      func: Function,
      params?: { frameActor: ?FrameId }
    ) => void,
    evaluateJSAsync: (
      script: Script,
      func: Function,
      params?: { frameActor: ?FrameId }
    ) => Promise<{ result: Grip | null }>,
    autocomplete: (
      input: string,
      cursor: number,
      func: Function,
      frameId: ?string
    ) => void,
    emit: (string, any) => void,
  },
  form: { consoleActor: any },
  root: any,
  navigateTo: ({ url: string }) => Promise<*>,
  listWorkers: () => Promise<*>,
  reload: () => Promise<*>,
  destroy: () => void,
  isBrowsingContext: boolean,
  isContentProcess: boolean,
  traits: Object,
};

/**
 * Clients for accessing the Firefox debug server and browser
 * @memberof firefox/clients
 * @static
 */

/**
 * DebuggerClient
 * @memberof firefox
 * @static
 */
export type DebuggerClient = {
  _activeRequests: {
    get: any => any,
    delete: any => void,
  },
  mainRoot: {
    traits: any,
    getFront: string => Promise<*>,
  },
  connect: () => Promise<*>,
  request: (packet: Object) => Promise<*>,
  attachConsole: (actor: String, listeners: Array<*>) => Promise<*>,
  createObjectClient: (grip: Grip) => {},
  release: (actor: String) => {},
};

export type TabClient = {
  listWorkers: () => Promise<*>,
  addListener: (string, Function) => void,
  on: (string, Function) => void,
};

/**
 * A grip is a JSON value that refers to a specific JavaScript value in the
 * debuggee. Grips appear anywhere an arbitrary value from the debuggee needs
 * to be conveyed to the client: stack frames, object property lists, lexical
 * environments, paused packets, and so on.
 *
 * For mutable values like objects and arrays, grips do not merely convey the
 * value's current state to the client. They also act as references to the
 * original value, by including an actor to which the client can send messages
 * to modify the value in the debuggee.
 *
 * @see https://wiki.mozilla.org/Remote_Debugging_Protocol#Grips
 * @memberof firefox
 * @static
 */
export type Grip = {
  actor: string,
  class: string,
  displayClass: string,
  displayName?: string,
  parameterNames?: string[],
  userDisplayName?: string,
  name: string,
  extensible: boolean,
  location: {
    url: string,
    line: number,
    column: number,
  },
  frozen: boolean,
  ownPropertyLength: number,
  preview: Object,
  sealed: boolean,
  optimizedOut: boolean,
  type: string,
};

export type FunctionGrip = {|
  class: "Function",
  name: string,
  parameterNames: string[],
  displayName: string,
  userDisplayName: string,
  url: string,
  line: number,
  column: number,
|};

/**
 * SourceClient
 * @memberof firefox
 * @static
 */
export type SourceClient = {
  source: () => { source: any, contentType?: string },
  _activeThread: ThreadFront,
  actor: string,
  getBreakpointPositionsCompressed: (range: ?Range) => Promise<any>,
  prettyPrint: number => Promise<*>,
  disablePrettyPrint: () => Promise<*>,
  blackBox: (range?: Range) => Promise<*>,
  unblackBox: (range?: Range) => Promise<*>,
  getBreakableLines: () => Promise<number[]>,
};

/**
 * ObjectClient
 * @memberof firefox
 * @static
 */
export type ObjectClient = {
  getPrototypeAndProperties: () => any,
};

/**
 * ThreadFront
 * @memberof firefox
 * @static
 */
export type ThreadFront = {
  resume: Function => Promise<*>,
  stepIn: Function => Promise<*>,
  stepOver: Function => Promise<*>,
  stepOut: Function => Promise<*>,
  rewind: Function => Promise<*>,
  reverseStepOver: Function => Promise<*>,
  breakOnNext: () => Promise<*>,
  // FIXME: unclear if SourceId or ActorId here
  source: ({ actor: SourceId }) => SourceClient,
  pauseGrip: (Grip | Function) => ObjectClient,
  pauseOnExceptions: (boolean, boolean) => Promise<*>,
  setBreakpoint: (BreakpointLocation, BreakpointOptions) => Promise<*>,
  removeBreakpoint: PendingLocation => Promise<*>,
  setXHRBreakpoint: (path: string, method: string) => Promise<boolean>,
  removeXHRBreakpoint: (path: string, method: string) => Promise<boolean>,
  interrupt: () => Promise<*>,
  eventListeners: () => Promise<*>,
  getFrames: (number, number) => FramesResponse,
  getEnvironment: (frame: Frame) => Promise<*>,
  on: (string, Function) => void,
  getSources: () => Promise<SourcesPacket>,
  reconfigure: ({ observeAsmJS: boolean }) => Promise<*>,
  getLastPausePacket: () => ?PausedPacket,
  _parent: TabClient,
  actor: ActorId,
  actorID: ActorId,
  request: (payload: Object) => Promise<*>,
  url: string,
  setActiveEventBreakpoints: (string[]) => void,
  getAvailableEventBreakpoints: () => Promise<EventListenerCategoryList>,
  skipBreakpoints: boolean => Promise<{| skip: boolean |}>,
};

export type Panel = {|
  emit: (eventName: string) => void,
  openLink: (url: string) => void,
  openWorkerToolbox: (worker: Worker) => void,
  openElementInInspector: (grip: Object) => void,
  openConsoleAndEvaluate: (input: string) => void,
  highlightDomElement: (grip: Object) => void,
  unHighlightDomElement: (grip: Object) => void,
  getToolboxStore: () => any,
|};
