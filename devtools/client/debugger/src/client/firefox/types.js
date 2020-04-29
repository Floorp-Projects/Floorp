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
  PendingLocation,
  SourceId,
  Range,
  URL,
} from "../../types";

import type { EventListenerCategoryList } from "../../actions/types";

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

export type FramePacket = {|
  +actor: ActorId,
  +arguments: any[],
  +displayName: string,
  +this: any,
  +depth?: number,
  +oldest?: boolean,
  +type: "pause" | "call",
  +where: ServerLocation,
|};

type ServerLocation = {| actor: string, line: number, column: number |};

/**
 * Frame Packet
 * @memberof firefox/packets
 * @static
 */
export type FrameFront = {
  +typeName: "frame",
  +data: FramePacket,
  +getEnvironment: () => Promise<*>,
  +where: ServerLocation,
  actorID: string,
  displayName: string,
  this: any,
  asyncCause: null | string,
  state: "on-stack" | "suspended" | "dead",
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
  sourceMapBaseURL: URL | null,
  sourceMapURL: URL | null,
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
  frame: FrameFront,
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

export type TabPayload = {
  actor: ActorId,
  animationsActor: ActorId,
  consoleActor: ActorId,
  contentViewerActor: ActorId,
  cssPropertiesActor: ActorId,
  directorManagerActor: ActorId,
  eventLoopLagActor: ActorId,
  framerateActor: ActorId,
  inspectorActor: ActorId,
  memoryActor: ActorId,
  monitorActor: ActorId,
  outerWindowID: number,
  performanceActor: ActorId,
  performanceEntriesActor: ActorId,
  profilerActor: ActorId,
  reflowActor: ActorId,
  responsiveActor: ActorId,
  storageActor: ActorId,
  styleEditorActor: ActorId,
  styleSheetsActor: ActorId,
  timelineActor: ActorId,
  title: string,
  url: URL,
  webExtensionInspectedWindowActor: ActorId,
};

/**
 * Tab Target gives access to the browser tabs
 * @memberof firefox
 * @static
 */
export type Target = {
  off: (string, Function) => void,
  on: (string, Function) => void,
  emit: (string, any) => void,
  getFront: string => Promise<ConsoleFront>,
  form: { consoleActor: any },
  root: any,
  navigateTo: ({ url: URL }) => Promise<*>,
  attach: () => Promise<*>,
  attachThread: Object => Promise<ThreadFront>,
  listWorkers: () => Promise<*>,
  reload: () => Promise<*>,
  destroy: () => void,
  threadFront: ThreadFront,
  name: string,
  isBrowsingContext: boolean,
  isContentProcess: boolean,
  isWorkerTarget: boolean,
  traits: Object,
  chrome: boolean,
  url: URL,
  isParentProcess: boolean,
  isServiceWorker: boolean,
  targetForm: Object,

  // Property installed by the debugger itself.
  debuggerServiceWorkerStatus: string,
};

type ConsoleFront = {
  evaluateJSAsync: (
    script: Script,
    func: Function,
    params?: { frameActor: ?FrameId }
  ) => Promise<{ result: ExpressionResult }>,
  autocomplete: (
    input: string,
    cursor: number,
    func: Function,
    frameId: ?string
  ) => void,
  emit: (string, any) => void,
};

/**
 * Clients for accessing the Firefox debug server and browser
 * @memberof firefox/clients
 * @static
 */

/**
 * DevToolsClient
 * @memberof firefox
 * @static
 */
export type DevToolsClient = {
  _activeRequests: {
    get: any => any,
    delete: any => void,
  },
  mainRoot: {
    traits: any,
    getFront: string => Promise<*>,
    listProcesses: () => Promise<Array<ProcessDescriptor>>,
    listAllWorkerTargets: () => Promise<*>,
    listServiceWorkerRegistrations: () => Promise<*>,
    getWorker: any => Promise<*>,
    on: (string, Function) => void,
  },
  connect: () => Promise<*>,
  request: (packet: Object) => Promise<*>,
  attachConsole: (actor: String, listeners: Array<*>) => Promise<*>,
  createObjectFront: (grip: Grip, thread: ThreadFront) => ObjectFront,
  release: (actor: String) => {},
  getFrontByID: (actor: String) => { release: () => Promise<*> },
};

type ProcessDescriptor = Object;

/**
 * DevToolsClient
 * @memberof firefox
 * @static
 */
export type TargetList = {
  watchTargets: (Array<string>, Function, Function) => void,
  unwatchTargets: (Array<string>, Function, Function) => void,
  getAllTargets: string => Array<Target>,
  targetFront: Target,
  TYPES: {
    FRAME: string,
    PROCESS: string,
    WORKER: string,
  },
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
export type Grip = {|
  actor?: string,
  class: string,
  displayClass: string,
  displayName?: string,
  parameterNames?: string[],
  userDisplayName?: string,
  name: string,
  extensible: boolean,
  location: {
    url: URL,
    line: number,
    column: number,
  },
  frozen: boolean,
  ownPropertyLength: number,
  preview: Object,
  sealed: boolean,
  optimizedOut: boolean,
  type: string,
|};

export type FunctionGrip = {|
  class: "Function",
  name: string,
  parameterNames: string[],
  displayName: string,
  userDisplayName: string,
  url: URL,
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
 * ObjectFront
 * @memberof firefox
 * @static
 */
export type ObjectFront = {
  actorID: string,
  getGrip: () => Grip,
  getPrototypeAndProperties: () => any,
  getProperty: string => { descriptor: any },
  addWatchpoint: (
    property: string,
    label: string,
    watchpointType: string
  ) => {},
  removeWatchpoint: (property: string) => {},
  release: () => Promise<*>,
};

export type LongStringFront = {
  actorID: string,
  getGrip: () => Grip,
  release: () => Promise<*>,
};

export type ExpressionResult =
  | ObjectFront
  | LongStringFront
  | Grip
  | string
  | number
  | null;

/**
 * ThreadFront
 * @memberof firefox
 * @static
 */
export type ThreadFront = {
  actorID: string,
  parentFront: Target,
  getFrames: (number, number) => Promise<{| frames: FrameFront[] |}>,
  resume: Function => Promise<*>,
  stepIn: Function => Promise<*>,
  stepOver: Function => Promise<*>,
  stepOut: Function => Promise<*>,
  breakOnNext: () => Promise<*>,
  // FIXME: unclear if SourceId or ActorId here
  source: ({ actor: SourceId }) => SourceClient,
  pauseGrip: (Grip | Function) => ObjectFront,
  pauseOnExceptions: (boolean, boolean) => Promise<*>,
  setBreakpoint: (BreakpointLocation, BreakpointOptions) => Promise<*>,
  removeBreakpoint: PendingLocation => Promise<*>,
  setXHRBreakpoint: (path: string, method: string) => Promise<boolean>,
  removeXHRBreakpoint: (path: string, method: string) => Promise<boolean>,
  interrupt: () => Promise<*>,
  eventListeners: () => Promise<*>,
  on: (string, Function) => void,
  off: (string, Function) => void,
  getSources: () => Promise<SourcesPacket>,
  reconfigure: ({ observeAsmJS: boolean }) => Promise<*>,
  getLastPausePacket: () => ?PausedPacket,
  _parent: Target,
  actor: ActorId,
  actorID: ActorId,
  request: (payload: Object) => Promise<*>,
  url: URL,
  setActiveEventBreakpoints: (string[]) => Promise<void>,
  getAvailableEventBreakpoints: () => Promise<EventListenerCategoryList>,
  skipBreakpoints: boolean => Promise<{| skip: boolean |}>,
  detach: () => Promise<void>,
  fetchAncestorFramePositions: Function => Promise<*>,
  get: string => FrameFront,
  dumpThread: Function => void,
};

export type Panel = {|
  emit: (eventName: string) => void,
  openLink: (url: URL) => void,
  openInspector: () => void,
  openElementInInspector: (grip: Object) => void,
  openConsoleAndEvaluate: (input: string) => void,
  highlightDomElement: (grip: Object) => void,
  unHighlightDomElement: (grip: Object) => void,
  getToolboxStore: () => any,
  panelWin: Object,
|};
