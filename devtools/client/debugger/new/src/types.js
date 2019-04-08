/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

export type SearchModifiers = {
  caseSensitive: boolean,
  wholeWord: boolean,
  regexMatch: boolean
};

export type Mode =
  | String
  | {
      name: string,
      typescript?: boolean,
      base?: {
        name: string,
        typescript: boolean
      }
    };

export type ThreadId = string;

/**
 * Breakpoint ID
 *
 * @memberof types
 * @static
 */
export type BreakpointId = string;

/**
 * Source ID
 *
 * @memberof types
 * @static
 */
export type SourceId = string;

/**
 * Actor ID
 *
 * @memberof types
 * @static
 */
export type ActorId = string;

export type SourceActorLocation = {|
  +sourceActor: SourceActor,
  +line: number,
  +column?: number
|};

/**
 * Source File Location
 *
 * @memberof types
 * @static
 */
export type SourceLocation = {|
  +sourceId: SourceId,
  +line: number,
  +column?: number,
  +sourceUrl?: string
|};

export type MappedLocation = {
  +location: SourceLocation,
  +generatedLocation: SourceLocation
};

export type PartialPosition = {
  +line: number,
  +column?: number
};

export type Position = {
  +line: number,
  +column: number
};

export type PartialRange = { end: PartialPosition, start: PartialPosition };
export type Range = { end: Position, start: Position };

export type PendingLocation = {
  +line: number,
  +column?: number,
  +sourceUrl?: string
};

// Type of location used when setting breakpoints in the server. Exactly one of
// { sourceUrl, sourceId } must be specified. Soon this will replace
// SourceLocation and PendingLocation, and SourceActorLocation will be removed
// (bug 1524374).
export type BreakpointLocation = {
  +line: number,
  +column?: number,
  +sourceUrl?: string,
  +sourceId?: SourceId
};

export type ASTLocation = {|
  +name: ?string,
  +offset: PartialPosition,
  +index: number
|};

/**
 * Breakpoint is associated with a Source.
 *
 * @memberof types
 * @static
 */
export type Breakpoint = {|
  +id: BreakpointId,
  +location: SourceLocation,
  +astLocation: ?ASTLocation,
  +generatedLocation: SourceLocation,
  +disabled: boolean,
  +text: string,
  +originalText: string,
  +options: BreakpointOptions
|};

/**
 * Options for a breakpoint that can be modified by the user.
 */
export type BreakpointOptions = {
  hidden?: boolean,
  condition?: string | null,
  logValue?: string | null,
  logGroupId?: string | null
};

export type BreakpointActor = {|
  +actor: ActorId,
  +source: SourceActor
|};

/**
 * XHR Breakpoint
 * @memberof types
 * @static
 */
export type XHRBreakpoint = {|
  +path: string,
  +method: "GET" | "POST" | "DELETE" | "ANY",
  +loading: boolean,
  +disabled: boolean,
  +text: string
|};

/**
 * Breakpoint Result is the return from an add/modify Breakpoint request
 *
 * @memberof types
 * @static
 */
export type BreakpointResult = {
  id: ActorId,
  actualLocation: SourceActorLocation
};

/**
 * PendingBreakpoint
 *
 * @memberof types
 * @static
 */
export type PendingBreakpoint = {
  +location: PendingLocation,
  +astLocation: ASTLocation,
  +generatedLocation: PendingLocation,
  +disabled: boolean,
  +text: string,
  +options: BreakpointOptions
};

/**
 * Frame ID
 *
 * @memberof types
 * @static
 */
export type FrameId = string;

/**
 * Frame
 * @memberof types
 * @static
 */
export type Frame = {
  id: FrameId,
  thread: string,
  displayName: string,
  location: SourceLocation,
  generatedLocation: SourceLocation,
  source: ?Source,
  scope: Scope,
  // FIXME Define this type more clearly
  this: Object,
  framework?: string,
  isOriginal?: boolean,
  originalDisplayName?: string,
  library?: string
};

export type ChromeFrame = {
  id: FrameId,
  displayName: string,
  scopeChain: any,
  generatedLocation: SourceLocation,
  location: ?SourceLocation
};

/**
 * ContextMenuItem
 *
 * @memberof types
 * @static
 */
export type ContextMenuItem = {
  id: string,
  label: string,
  accesskey: string,
  disabled: boolean,
  click: Function
};

/**
 * why
 * @memberof types
 * @static
 */
export type ExceptionReason = {|
  exception: string | Grip,
  message: string,
  type: "exception",
  frameFinished?: Object
|};

/**
 * why
 * @memberof types
 * @static
 */
export type Why =
  | ExceptionReason
  | {
      type: string,
      frameFinished?: Object
    };

/**
 * Why is the Debugger Paused?
 * This is the generic state handling the reason the debugger is paused.
 * Reasons are usually related to "breakpoint" or "debuggerStatement"
 * and should eventually be specified here as an enum.  For now we will
 * just offer it as a string.
 * @memberof types
 * @static
 */
export type WhyPaused = {
  type: string
};

export type LoadedObject = {
  objectId: string,
  parentId: string,
  name: string,
  value: any
};

/**
 * Pause
 * @memberof types
 * @static
 */
export type Pause = {
  thread: string,
  frame: Frame,
  frames: Frame[],
  why: Why,
  loadedObjects?: LoadedObject[]
};

/**
 * Expression
 * @memberof types
 * @static
 */
export type Expression = {
  input: string,
  value: Object,
  from: string,
  updating: boolean
};

/**
 * PreviewGrip
 * @memberof types
 * @static
 */

/**
 * Grip
 * @memberof types
 * @static
 */
export type Grip = {
  actor: string,
  class: string,
  extensible: boolean,
  frozen: boolean,
  isGlobal: boolean,
  ownPropertyLength: number,
  ownProperties: Object,
  preview?: Grip,
  sealed: boolean,
  type: string,
  url?: string,
  fileName?: string,
  message?: string,
  name?: string
};

export type SourceActor = {|
  +actor: ActorId,
  +source: SourceId,
  +thread: ThreadId
|};

/**
 * BaseSource
 *
 * @memberof types
 * @static
 */

type BaseSource = {|
  +id: SourceId,
  +url: string,
  +sourceMapURL?: string,
  +isBlackBoxed: boolean,
  +isPrettyPrinted: boolean,
  +contentType?: string,
  +error?: string,
  +loadedState: "unloaded" | "loading" | "loaded",
  +relativeUrl: string,
  +introductionUrl: ?string,
  +introductionType: ?string,
  +isExtension: boolean,
  +actors: SourceActor[]
|};

/**
 * JsSource
 *
 * @memberof types
 * @static
 */

export type JsSource = {|
  ...BaseSource,
  +isWasm: false,
  +text?: string
|};

/**
 * WasmSource
 *
 * @memberof types
 * @static
 */

export type WasmSource = {|
  ...BaseSource,
  +isWasm: true,
  +text?: {| binary: Object |}
|};

export type Source = JsSource | WasmSource;

/**
 * Script
 * This describes scripts which are sent to the debug server to be eval'd
 * @memberof types
 * @static
 * FIXME: This needs a real type definition
 */
export type Script = any;

/**
 * Describes content of the binding.
 * FIXME Define these type more clearly
 */
export type BindingContents = {
  value: any
};

/**
 * Defines map of binding name to its content.
 */
export type ScopeBindings = {
  [name: string]: BindingContents
};

/**
 * Scope
 * @memberof types
 * @static
 */
export type Scope = {|
  actor: ActorId,
  parent: ?Scope,
  bindings?: {
    arguments: Array<ScopeBindings>,
    variables: ScopeBindings,
    this?: BindingContents | null
  },
  object: ?Object,
  function: ?{
    actor: ActorId,
    class: string,
    displayName: string,
    location: SourceLocation,
    parameterNames: string[]
  },
  type: string
|};

export type MainThread = {
  +actor: string,
  +url: string,
  +type: number
};

export type Worker = {
  +actor: string,
  +url: string,
  +type: number
};

export type Thread = MainThread & Worker;
export type ThreadList = Array<Thread>;
export type WorkerList = Array<Worker>;

export type Cancellable = {
  cancel: () => void
};

export type EventListenerBreakpoints = string[];

export type SourceDocuments = { [string]: Object };

export type BreakpointPosition = MappedLocation;
export type BreakpointPositions = BreakpointPosition[];

export type { Context, ThreadContext } from "./utils/context";
