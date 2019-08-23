/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { SettledValue, FulfilledValue } from "./utils/async-value";
import type { SourcePayload } from "./client/firefox/types";
import type { SourceActorId, SourceActor } from "./reducers/source-actors";
import type { SourceBase } from "./reducers/sources";

export type { SourceActorId, SourceActor, SourceBase };

export type SearchModifiers = {
  caseSensitive: boolean,
  wholeWord: boolean,
  regexMatch: boolean,
};

export type Mode =
  | String
  | {
      name: string,
      typescript?: boolean,
      base?: {
        name: string,
        typescript: boolean,
      },
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

export type QueuedSourceData =
  | { type: "original", data: OriginalSourceData }
  | { type: "generated", data: GeneratedSourceData };

export type OriginalSourceData = {|
  id: string,
  url: string,
|};

export type GeneratedSourceData = {
  thread: ThreadId,
  source: SourcePayload,

  // Many of our tests rely on being able to set a specific ID for the Source
  // object. We may want to consider avoiding that eventually.
  id?: string,
};

export type SourceActorLocation = {|
  +sourceActor: SourceActorId,
  +line: number,
  +column?: number,
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
  +sourceUrl?: string,
|};

export type MappedLocation = {
  +location: SourceLocation,
  +generatedLocation: SourceLocation,
};

export type PartialPosition = {
  +line: number,
  +column?: number,
};

export type Position = {
  +line: number,
  +column: number,
};

export type PartialRange = { end: PartialPosition, start: PartialPosition };
export type Range = { end: Position, start: Position };

export type PendingLocation = {
  +line: number,
  +column?: number,
  +sourceUrl?: string,
};

// Type of location used when setting breakpoints in the server. Exactly one of
// { sourceUrl, sourceId } must be specified. Soon this will replace
// SourceLocation and PendingLocation, and SourceActorLocation will be removed
// (bug 1524374).
export type BreakpointLocation = {
  +line: number,
  +column?: number,
  +sourceUrl?: string,
  +sourceId?: SourceActorId,
};

export type ASTLocation = {|
  +name: ?string,
  +offset: PartialPosition,
  +index: number,
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
  +options: BreakpointOptions,
|};

/**
 * Options for a breakpoint that can be modified by the user.
 */
export type BreakpointOptions = {
  hidden?: boolean,
  condition?: string | null,
  logValue?: string | null,
  logGroupId?: string | null,
};

export type BreakpointActor = {|
  +actor: ActorId,
  +source: SourceActor,
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
  +text: string,
|};

/**
 * Breakpoint Result is the return from an add/modify Breakpoint request
 *
 * @memberof types
 * @static
 */
export type BreakpointResult = {
  id: ActorId,
  actualLocation: SourceActorLocation,
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
  +options: BreakpointOptions,
};

/**
 * Frame ID
 *
 * @memberof types
 * @static
 */
export type FrameId = string;

type Expr = string;

export type XScopeVariable = {
  name: string,
  expr?: Expr,
};

export type XScopeVariables = {
  vars: XScopeVariable[],
  frameBase?: Expr | null,
};

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
  originalVariables?: XScopeVariables,
  library?: string,
};

export type ChromeFrame = {
  id: FrameId,
  displayName: string,
  scopeChain: any,
  generatedLocation: SourceLocation,
  location: ?SourceLocation,
};

export type OriginalFrame = {|
  displayName: string,
  variables?: Object,
  location?: SourceLocation,
|};

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
  click: Function,
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
  frameFinished?: Object,
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
      frameFinished?: Object,
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
  type: string,
};

export type LoadedObject = {
  objectId: string,
  parentId: string,
  name: string,
  value: any,
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
  loadedObjects?: LoadedObject[],
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
  updating: boolean,
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
  name?: string,
};

export type TextSourceContent = {|
  type: "text",
  value: string,
  contentType: string | void,
|};
export type WasmSourceContent = {|
  type: "wasm",
  value: {| binary: Object |},
|};
export type SourceContent = TextSourceContent | WasmSourceContent;

export type SourceWithContent = $ReadOnly<{
  ...SourceBase,
  +content: SettledValue<SourceContent> | null,
}>;
export type SourceWithContentAndType<+Content: SourceContent> = $ReadOnly<{
  ...SourceBase,
  +content: FulfilledValue<Content>,
}>;

/**
 * Source
 *
 * @memberof types
 * @static
 */

export type Source = {
  +id: SourceId,
  +url: string,
  +isBlackBoxed: boolean,
  +isPrettyPrinted: boolean,
  +relativeUrl: string,
  +introductionUrl: ?string,
  +introductionType: ?string,
  +extensionName: ?string,
  +isExtension: boolean,
  +isWasm: boolean,
};

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
  value: any,
};

/**
 * Defines map of binding name to its content.
 */
export type ScopeBindings = {
  [name: string]: BindingContents,
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
    this?: BindingContents | null,
  },
  object: ?Object,
  function: ?{
    actor: ActorId,
    class: string,
    displayName: string,
    location: SourceLocation,
    parameterNames: string[],
  },
  type: string,
  scopeKind: string,
|};

export type Thread = {
  +actor: ThreadId,
  +url: string,
  +type: string,
  +name: string,
};

export type Worker = Thread;
export type ThreadList = Array<Thread>;

export type Cancellable = {
  cancel: () => void,
};

export type EventListenerBreakpoints = string[];

export type SourceDocuments = { [string]: Object };

export type BreakpointPosition = MappedLocation;
export type BreakpointPositions = { [number]: BreakpointPosition[] };

export type DOMMutationBreakpoint = {
  id: number,
  nodeFront: Object,
  mutationType: "subtree" | "attribute" | "removal",
  enabled: boolean,
};

export type { Context, ThreadContext } from "./utils/context";

export type Previews = {
  [line: string]: Array<Preview>,
};

export type Preview = {
  name: string,
  value: any,
  column: number,
};
