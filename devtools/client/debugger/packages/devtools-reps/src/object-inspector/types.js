/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

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

export type GripProperties = {
  ownProperties?: Object,
  ownSymbols?: Array<Object>,
  safeGetterValues?: Object,
  prototype?: Object,
  fullText?: string,
};

export type NodeContents = {
  value: Object | number | string | boolean | null | typeof undefined,
};

export type NodeMeta = {
  startIndex: number,
  endIndex: number,
};

export type Path = string;
export type Node = {
  contents: Array<Node> | NodeContents,
  name: string,
  path: Path,
  type?: Symbol,
  meta?: NodeMeta,
};

export type RdpGrip = {
  actor: string,
  class: string,
  displayClass: string,
  extensible: boolean,
  frozen: boolean,
  ownPropertyLength: number,
  preview: Object,
  sealed: boolean,
  type: string,
};

export type PropertiesIterator = {
  count: number,
  slice: (start: number, count: number) => Promise<GripProperties>,
};

export type ObjectClient = {
  enumEntries: () => Promise<PropertiesIterator>,
  enumProperties: (options: Object) => Promise<PropertiesIterator>,
  enumSymbols: () => Promise<PropertiesIterator>,
  getPrototype: () => Promise<{ prototype: Object }>,
  getProxySlots: () => Promise<{ proxyTarget: Object, proxyHandler: Object }>,
};

export type LongStringClient = {
  substring: (
    start: number,
    end: number,
    response: {
      substring?: string,
      error?: Error,
      message?: string,
    }
  ) => void,
};

export type CreateObjectClient = RdpGrip => ObjectClient;

export type CreateLongStringClient = RdpGrip => LongStringClient;

export type CachedNodes = Map<Path, Array<Node>>;

export type LoadedProperties = Map<Path, GripProperties>;

export type Evaluations = Map<
  Path,
  { getterValue: GripProperties, evaluation: number }
>;

export type Mode = MODE.TINY | MODE.SHORT | MODE.LONG;

const { MODE } = require("../reps/constants");

export type Props = {
  autoExpandAll: boolean,
  autoExpandDepth: number,
  focusable: boolean,
  itemHeight: number,
  inline: boolean,
  mode: Mode,
  roots: Array<Node>,
  disableWrap: boolean,
  dimTopLevelWindow: boolean,
  releaseActor: string => void,
  createObjectClient: CreateObjectClient,
  createLongStringClient: CreateLongStringClient,
  onFocus: ?(Node) => any,
  onActivate: ?(Node) => any,
  onDoubleClick: ?(
    item: Node,
    options: {
      depth: number,
      focused: boolean,
      expanded: boolean,
    }
  ) => any,
  onCmdCtrlClick: ?(
    item: Node,
    options: {
      depth: number,
      event: MouseEvent,
      focused: boolean,
      expanded: boolean,
    }
  ) => any,
  onLabelClick: ?(
    item: Node,
    options: {
      depth: number,
      focused: boolean,
      expanded: boolean,
      setExpanded: (Node, boolean) => any,
    }
  ) => any,
  actors: Set<string>,
  expandedPaths: Set<Path>,
  focusedItem: ?Node,
  activeItem: ?Node,
  loadedProperties: LoadedProperties,
  evaluations: Evaluations,
  loading: Map<Path, Array<Promise<GripProperties>>>,
};

export type ReduxAction = {
  type: string,
  data: {},
};

export type State = {
  actors: Set<string>,
  expandedPaths: Set<Path>,
  focusedItem: ?Node,
  activeItem: ?Node,
  loadedProperties: LoadedProperties,
};

export type Store = {
  dispatch: any => any,
  getState: () => State,
};

export type ObjectInspectorId = string | number;

export type PersistedStates = Map<ObjectInspectorId, State>;
