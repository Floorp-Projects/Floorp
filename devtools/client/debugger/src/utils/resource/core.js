/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * This file defines the root resource datatypes and functions to manipulate
 * them. We specifically want these types to be opaque values because we want
 * to restrict access to them to the set of functions exposed specifically
 * for manipulation in order to promote good methods of access and good
 * memoization logic.
 */

type PropertiesById<ID, T> = $Exact<$ObjMap<T, <V>(v: V) => { [ID]: V }>>;

export type Resource = {
  +item: { +id: string }
};
export type Value<R: Resource, K: $Keys<R>> = $ElementType<R, K>;
export type Item<R: Resource> = Value<R, "item">;
export type Id<R: Resource> = $ElementType<Item<R>, "id">;

export type Order<R: Resource> = Array<Id<R>>;
export type ReadOnlyOrder<R: Resource> = $ReadOnlyArray<Id<R>>;

export type Fields<R: Resource> = {|
  // This spread clears the covariance on 'item'.
  ...PropertiesById<Id<R>, R>
|};
export type ReadOnlyFields<R: Resource> = $ReadOnly<Fields<R>>;

// The resource type without "item" so it is all the types you're allow to
// mutate via the resource API.
export type MutableResource<R: Resource> = $Rest<
  R,
  { item: $ElementType<R, "item"> }
>;
export type MutableFields<R: Resource> = PropertiesById<
  Id<R>,
  MutableResource<R>
>;

// The general structure of a state store. Each top-level property of the
// UnorderedState type will be stored as its own key/value object since the
// types will likely mutate independent of one another and this approach
// allows us to easily memoize values based on Redux selectors.
export opaque type UnorderedState<R: Resource> = {
  fields: ReadOnlyFields<R>
};

// An ordered version of the state where order is represented
// as an array alongside the item lookup so that reordering items
// does not need to affect any results that may have been computed
// over the items as a whole ignoring sort order.
export opaque type OrderedState<R: Resource> = {
  order: ReadOnlyOrder<R>,
  fields: ReadOnlyFields<R>
};

export type State<R: Resource> = OrderedState<R> | UnorderedState<R>;

/**
 * Provide the default Redux state for an unordered store.
 */
export function createUnordered<R: Resource>(
  fields: Fields<R>
): UnorderedState<R> {
  if (Object.keys(fields.item).length !== 0) {
    throw new Error("The initial 'fields' object should be empty.");
  }

  return {
    fields
  };
}

/**
 * Provide the default Redux state for an ordered store.
 */
export function createOrdered<R: Resource>(fields: Fields<R>): OrderedState<R> {
  if (Object.keys(fields.item).length !== 0) {
    throw new Error("The initial 'fields' object should be empty.");
  }

  return {
    order: [],
    fields
  };
}

export function mutateState<R: Resource, S: State<R>, T>(
  state: S,
  arg: T,
  fieldsMutator: (T, Fields<R>) => void,
  orderMutator?: (T, Order<R>) => Order<R> | void
): S {
  // $FlowIgnore - Flow can't quite tell that this is a valid way to copy state.
  state = { ...state };

  state.fields = { ...state.fields };

  fieldsMutator(arg, state.fields);

  if (orderMutator && state.order) {
    const mutableOrder = state.order.slice();

    state.order = orderMutator(arg, mutableOrder) || mutableOrder;
  }

  return state;
}

export function getOrder<R: Resource>(
  state: OrderedState<R>
): ReadOnlyOrder<R> {
  return state.order;
}

export function getFields<R: Resource>(state: State<R>): ReadOnlyFields<R> {
  return state.fields;
}
