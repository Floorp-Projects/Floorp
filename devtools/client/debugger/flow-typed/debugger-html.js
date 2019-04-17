/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

declare module "debugger-html" {
  /**
   * Breakpoint ID
   *
   * @memberof types
   * @static
   */
  declare type BreakpointId = string;

  /**
   * Source ID
   *
   * @memberof types
   * @static
   */
  declare type SourceId = string;

  /**
   * Actor ID
   *
   * @memberof types
   * @static
   */
  declare type ActorId = string;

  /**
   * Source File Location
   *
   * @memberof types
   * @static
   */
  declare type SourceLocation = {|
    sourceId: SourceId,
    line: number,
    column?: number,
    sourceUrl?: string
  |};

  /**
   * Breakpoint
   *
   * @memberof types
   * @static
   */
  declare type Breakpoint = {
    id: BreakpointId,
    location: SourceLocation,
    loading: boolean,
    disabled: boolean,
    text: string,
    condition: ?string
  };

  /**
   * Breakpoint Result is the return from an add/modify Breakpoint request
   *
   * @memberof types
   * @static
   */
  declare type BreakpointResult = {
    id: ActorId,
    actualLocation: SourceLocation
  };

  /**
   * Source
   *
   * @memberof types
   * @static
   */
  declare type Source = {
    id: SourceId,
    url?: string,
    sourceMapURL?: string,
    isWasm: boolean,
    text?: string,
    contentType?: string
  };

  /**
   * Frame ID
   *
   * @memberof types
   * @static
   */
  declare type FrameId = string;

  /**
   * Frame
   * @memberof types
   * @static
   */
  declare type Frame = {
    id: FrameId,
    displayName: string,
    location: SourceLocation,
    source?: Source,
    scope: Scope,
    // FIXME Define this type more clearly
    this: Object
  };

  /**
   * Original Frame
   *
   * @memberof types
   * @static
   */
  declare type OriginalFrame = {
    displayName: string,
    location?: SourceLocation,
    thread?: string
  };

  /**
   * why
   * @memberof types
   * @static
   */
  declare type Why = {
    type: string
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
  declare type WhyPaused = {
    type: string
  };

  declare type LoadedObject = {
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
  declare type Pause = {
    frames: Frame[],
    why: Why,
    getIn: (string[]) => any,
    loadedObjects?: LoadedObject[]
  };
  /**
   * Expression
   * @memberof types
   * @static
   */
  declare type Expression = {
    id: number,
    input: string
  };

  /**
   * Grip
   * @memberof types
   * @static
   */
  declare type Grip = {
    actor: string,
    class: string,
    extensible: boolean,
    frozen: boolean,
    isGlobal: boolean,
    ownPropertyLength: number,
    preview: {
      kind: string,
      url: string
    },
    sealed: boolean,
    type: string
  };

  /**
   * SourceText
   * @memberof types
   * @static
   */
  declare type SourceText = {
    id: string,
    text: string,
    contentType: string
  };

  /**
   * SourceScope
   * @memberof types
   * @static
   */
  declare type SourceScope = {
    type: string,
    start: SourceLocation,
    end: SourceLocation,
    bindings: {
      [name: string]: SourceLocation[]
    }
  };

  /*
 * MappedScopeBindings
 * @memberof types
 * @static
 */
  declare type MappedScopeBindings = {
    type: string,
    bindings: {
      [originalName: string]: string
    }
  };

  /**
   * Script
   * This describes scripts which are sent to the debug server to be eval'd
   * @memberof types
   * @static
   * FIXME: This needs a real type definition
   */
  declare type Script = any;

  declare type SyntheticScope = {
    type: string,
    bindingsNames: string[]
  };

  /**
   * Scope
   * @memberof types
   * @static
   */
  declare type Scope = {
    actor: ActorId,
    parent: Scope,
    bindings: {
      // FIXME Define these types more clearly
      arguments: Array<Object>,
      variables: Object
    },
    function: {
      actor: ActorId,
      class: string,
      displayName: string,
      location: SourceLocation,
      // FIXME Define this type more clearly
      parameterNames: Array<Object>
    },
    syntheticScopes?: {
      scopes: SyntheticScope[],
      groupIndex: number,
      groupLength: number
    },
    type: string
  };
}
