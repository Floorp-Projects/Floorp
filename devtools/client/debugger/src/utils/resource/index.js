/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import type { UnorderedState, OrderedState } from "./core";
export type {
  UnorderedState as ResourceState,
  OrderedState as OrderedResourceState
};
export type {
  FieldByIDGetter,
  FieldByIDMapper,
  FieldByIDReducer,
  FieldReducer
} from "./caching";

export {
  createInitial,
  createInitialOrdered,
  insertResourceAtIndex,
  insertResourcesAtIndex,
  insertResource,
  insertResources,
  removeResource,
  removeResources,
  setFieldsValue,
  setFieldsValues,
  setFieldValue,
  setFieldValues
} from "./actions";
export {
  hasResource,
  getResource,
  listItems,
  getItem,
  getFieldValue
} from "./selectors";
export {
  createFieldByIDGetter,
  createFieldByIDMapper,
  createFieldByIDReducer,
  createFieldReducer
} from "./caching";
