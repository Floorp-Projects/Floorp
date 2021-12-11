/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  createInitial,
  insertResources,
  updateResources,
  makeMapWithArgs,
  makeWeakQuery,
  makeShallowQuery,
  makeStrictQuery,
} from "..";

const makeResource = id => ({
  id,
  name: `name-${id}`,
  data: 42,
  obj: {},
});

// Jest's mock type just wouldn't cooperate below, so this is a custom version
// that does what I need.

// We need to pass the 'needsArgs' prop through to the query fn so we use
// this utility to do that and at the same time preserve typechecking.
const mockFn = f => Object.assign(jest.fn(f), f);

const mockFilter = callback => mockFn(callback);
const mockMapNoArgs = callback => mockFn(callback);
const mockMapWithArgs = callback => mockFn(makeMapWithArgs(callback));
const mockReduce = callback => mockFn(callback);

describe("resource query operations", () => {
  let r1, r2, r3;
  let initialState;
  let mapNoArgs, mapWithArgs, reduce;

  beforeEach(() => {
    r1 = makeResource("id-1");
    r2 = makeResource("id-2");
    r3 = makeResource("id-3");

    initialState = createInitial();

    initialState = insertResources(initialState, [r1, r2, r3]);

    mapNoArgs = mockMapNoArgs((resource, ident) => resource);
    mapWithArgs = mockMapWithArgs((resource, ident, args) => resource);
    reduce = mockReduce((mapped, ids, args) => {
      return mapped.reduce((acc, item, i) => {
        acc[ids[i]] = item;
        return acc;
      }, {});
    });
  });

  describe("weak cache", () => {
    let filter;

    beforeEach(() => {
      filter = mockFilter(
        // eslint-disable-next-line max-nested-callbacks
        (values, args) => args
      );
    });

    describe("no args", () => {
      let query;

      beforeEach(() => {
        query = makeWeakQuery({ filter, map: mapNoArgs, reduce });
      });

      it("should return same with same state and same args", () => {
        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result2 = query(initialState, args);
        expect(result2).toBe(result1);
        expect(result2).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return same with updated other state and same args 1", () => {
        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Updating r2 does not affect cached result that only cares about r2.
        const state = updateResources(initialState, [
          {
            id: r3.id,
            obj: {},
          },
        ]);

        const result2 = query(state, args);
        expect(result2).toBe(result1);
        expect(result2).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return same with updated other state and same args 2", () => {
        // eslint-disable-next-line max-nested-callbacks
        mapNoArgs.mockImplementation(resource => ({ ...resource, name: "" }));

        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: { ...r1, name: "" },
          [r2.id]: { ...r2, name: "" },
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Since the map function ignores the name value, updating it should
        // not reset the cached for this query.
        const state = updateResources(initialState, [
          {
            id: r3.id,
            name: "newName",
          },
        ]);

        const result2 = query(state, args);
        expect(result2).toBe(result1);
        expect(result2).toEqual({
          [r1.id]: { ...r1, name: "" },
          [r2.id]: { ...r2, name: "" },
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return diff with updated id state and same args", () => {
        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Since the mapper returns a value with name, changing a name will
        // invalidate the cache.
        const state = updateResources(initialState, [
          {
            id: r1.id,
            name: "newName",
          },
        ]);

        const result2 = query(state, args);
        expect(result2).not.toBe(result1);
        expect(result2).toEqual({
          [r1.id]: { ...r1, name: "newName" },
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(3);
        expect(reduce.mock.calls).toHaveLength(2);
      });

      it("should return diff with same state and diff args", () => {
        const firstArgs = [r1.id, r2.id];
        const secondArgs = [r1.id, r2.id];
        const result1 = query(initialState, firstArgs);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result2 = query(initialState, secondArgs);
        expect(result2).not.toBe(result1);
        expect(result2).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(2);

        // Same result from first query still available.
        const result3 = query(initialState, firstArgs);
        expect(result3).not.toBe(result2);
        expect(result3).toBe(result1);
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(2);

        // Same result from second query still available.
        const result4 = query(initialState, secondArgs);
        expect(result4).toBe(result2);
        expect(result4).not.toBe(result3);
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(2);
      });
    });

    describe("with args", () => {
      let query;

      beforeEach(() => {
        query = makeWeakQuery({ filter, map: mapWithArgs, reduce });
      });

      it("should return same with same state and same args", () => {
        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result2 = query(initialState, args);
        expect(result2).toBe(result1);
        expect(result2).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return same with updated other state and same args 1", () => {
        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Updating r2 does not affect cached result that only cares about r2.
        const state = updateResources(initialState, [
          {
            id: r3.id,
            obj: {},
          },
        ]);

        const result2 = query(state, args);
        expect(result2).toBe(result1);
        expect(result2).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return same with updated other state and same args 2", () => {
        // eslint-disable-next-line max-nested-callbacks
        mapWithArgs.mockImplementation(resource => ({ ...resource, name: "" }));

        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: { ...r1, name: "" },
          [r2.id]: { ...r2, name: "" },
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Since the map function ignores the name value, updating it should
        // not reset the cached for this query.
        const state = updateResources(initialState, [
          {
            id: r3.id,
            name: "newName",
          },
        ]);

        const result2 = query(state, args);
        expect(result2).toBe(result1);
        expect(result2).toEqual({
          [r1.id]: { ...r1, name: "" },
          [r2.id]: { ...r2, name: "" },
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return diff with updated id state and same args", () => {
        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Since the mapper returns a value with name, changing a name will
        // invalidate the cache.
        const state = updateResources(initialState, [
          {
            id: r1.id,
            name: "newName",
          },
        ]);

        const result2 = query(state, args);
        expect(result2).not.toBe(result1);
        expect(result2).toEqual({
          [r1.id]: { ...r1, name: "newName" },
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(3);
        expect(reduce.mock.calls).toHaveLength(2);
      });

      it("should return diff with same state and diff args", () => {
        const firstArgs = [r1.id, r2.id];
        const secondArgs = [r1.id, r2.id];

        const result1 = query(initialState, firstArgs);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result2 = query(initialState, secondArgs);
        expect(result2).not.toBe(result1);
        expect(result2).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(4);
        expect(reduce.mock.calls).toHaveLength(2);

        // Same result from first query still available.
        const result3 = query(initialState, firstArgs);
        expect(result3).not.toBe(result2);
        expect(result3).toBe(result1);
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(4);
        expect(reduce.mock.calls).toHaveLength(2);

        // Same result from second query still available.
        const result4 = query(initialState, secondArgs);
        expect(result4).toBe(result2);
        expect(result4).not.toBe(result3);
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(4);
        expect(reduce.mock.calls).toHaveLength(2);
      });
    });
  });

  describe("shallow cache", () => {
    let filter;

    beforeEach(() => {
      filter = mockFilter(
        // eslint-disable-next-line max-nested-callbacks
        (values, { ids }) => ids
      );
    });

    describe("no args", () => {
      let query;

      beforeEach(() => {
        query = makeShallowQuery({ filter, map: mapNoArgs, reduce });
      });

      it("should return last with same state and same args", () => {
        const ids = [r1.id, r2.id];
        const result1 = query(initialState, { ids });
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result2 = query(initialState, { ids });
        expect(result2).toBe(result1);
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return last with updated other state and same args 1", () => {
        const ids = [r1.id, r2.id];
        const result1 = query(initialState, { ids });
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Updating r2 does not affect cached result that only cares about r2.
        const state = updateResources(initialState, [
          {
            id: r3.id,
            obj: {},
          },
        ]);

        const result2 = query(state, { ids });
        expect(result2).toBe(result1);
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return last with updated other state and same args 2", () => {
        // eslint-disable-next-line max-nested-callbacks
        mapNoArgs.mockImplementation(resource => ({ ...resource, name: "" }));

        const ids = [r1.id, r2.id];
        const result1 = query(initialState, { ids });
        expect(result1).toEqual({
          [r1.id]: { ...r1, name: "" },
          [r2.id]: { ...r2, name: "" },
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Since the map function ignores the name value, updating it should
        // not reset the cached for this query.
        const state = updateResources(initialState, [
          {
            id: r3.id,
            name: "newName",
          },
        ]);

        const result2 = query(state, { ids });
        expect(result2).toBe(result1);
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return new with updated id state and same args", () => {
        const ids = [r1.id, r2.id];
        const result1 = query(initialState, { ids });
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Updating r2 does not affect cached result that only cares about r2.
        const state = updateResources(initialState, [
          {
            id: r2.id,
            name: "newName",
          },
        ]);

        const result2 = query(state, { ids });
        expect(result2).not.toBe(result1);
        expect(result2).toEqual({
          [r1.id]: r1,
          [r2.id]: { ...r2, name: "newName" },
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(3);
        expect(reduce.mock.calls).toHaveLength(2);
      });

      it("should return diff with same state and diff args", () => {
        const ids = [r1.id, r2.id];
        const result1 = query(initialState, { ids, flag: true });
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result2 = query(initialState, { ids, flag: false });
        expect(result2).toBe(result1);
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result3 = query(initialState, { ids, flag: true });
        expect(result3).toBe(result1);
        expect(filter.mock.calls).toHaveLength(3);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result4 = query(initialState, { ids, flag: false });
        expect(result4).toBe(result1);
        expect(filter.mock.calls).toHaveLength(4);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });
    });

    describe("with args", () => {
      let query;

      beforeEach(() => {
        query = makeShallowQuery({ filter, map: mapWithArgs, reduce });
      });

      it("should return last with same state and same args", () => {
        const ids = [r1.id, r2.id];
        const result1 = query(initialState, { ids });
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result2 = query(initialState, { ids });
        expect(result2).toBe(result1);
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return last with updated other state and same args 1", () => {
        const ids = [r1.id, r2.id];
        const result1 = query(initialState, { ids });
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Updating r2 does not affect cached result that only cares about r2.
        const state = updateResources(initialState, [
          {
            id: r3.id,
            obj: {},
          },
        ]);

        const result2 = query(state, { ids });
        expect(result2).toBe(result1);
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return last with updated other state and same args 2", () => {
        // eslint-disable-next-line max-nested-callbacks
        mapWithArgs.mockImplementation(resource => ({ ...resource, name: "" }));

        const ids = [r1.id, r2.id];
        const result1 = query(initialState, { ids });
        expect(result1).toEqual({
          [r1.id]: { ...r1, name: "" },
          [r2.id]: { ...r2, name: "" },
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Since the map function ignores the name value, updating it should
        // not reset the cached for this query.
        const state = updateResources(initialState, [
          {
            id: r3.id,
            name: "newName",
          },
        ]);

        const result2 = query(state, { ids });
        expect(result2).toBe(result1);
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return new with updated id state and same args", () => {
        const ids = [r1.id, r2.id];
        const result1 = query(initialState, { ids });
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Updating r2 does not affect cached result that only cares about r2.
        const state = updateResources(initialState, [
          {
            id: r2.id,
            name: "newName",
          },
        ]);

        const result2 = query(state, { ids });
        expect(result2).not.toBe(result1);
        expect(result2).toEqual({
          [r1.id]: r1,
          [r2.id]: { ...r2, name: "newName" },
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(3);
        expect(reduce.mock.calls).toHaveLength(2);
      });

      it("should return diff with same state and diff args", () => {
        const ids = [r1.id, r2.id];
        const result1 = query(initialState, { ids, flag: true });
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result2 = query(initialState, { ids, flag: false });
        expect(result2).toBe(result1);
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(4);
        expect(reduce.mock.calls).toHaveLength(1);

        const result3 = query(initialState, { ids, flag: true });
        expect(result3).toBe(result1);
        expect(filter.mock.calls).toHaveLength(3);
        expect(mapWithArgs.mock.calls).toHaveLength(6);
        expect(reduce.mock.calls).toHaveLength(1);

        const result4 = query(initialState, { ids, flag: false });
        expect(result4).toBe(result1);
        expect(filter.mock.calls).toHaveLength(4);
        expect(mapWithArgs.mock.calls).toHaveLength(8);
        expect(reduce.mock.calls).toHaveLength(1);
      });
    });
  });

  describe("strict cache", () => {
    let filter;

    beforeEach(() => {
      filter = mockFilter(
        // eslint-disable-next-line max-nested-callbacks
        (values, ids) => ids
      );
    });

    describe("no args", () => {
      let query;

      beforeEach(() => {
        query = makeStrictQuery({ filter, map: mapNoArgs, reduce });
      });

      it("should return same with same state and same args", () => {
        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result2 = query(initialState, args);
        expect(result2).toBe(result1);
        expect(result2).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return same with updated other state and same args 1", () => {
        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Updating r2 does not affect cached result that only cares about r2.
        const state = updateResources(initialState, [
          {
            id: r3.id,
            obj: {},
          },
        ]);

        const result2 = query(state, args);
        expect(result2).toBe(result1);
        expect(result2).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return same with updated other state and same args 2", () => {
        // eslint-disable-next-line max-nested-callbacks
        mapNoArgs.mockImplementation(resource => ({ ...resource, name: "" }));

        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: { ...r1, name: "" },
          [r2.id]: { ...r2, name: "" },
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Since the map function ignores the name value, updating it should
        // not reset the cached for this query.
        const state = updateResources(initialState, [
          {
            id: r3.id,
            name: "newName",
          },
        ]);

        const result2 = query(state, args);
        expect(result2).toBe(result1);
        expect(result2).toEqual({
          [r1.id]: { ...r1, name: "" },
          [r2.id]: { ...r2, name: "" },
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return diff with updated id state and same args", () => {
        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Since the mapper returns a value with name, changing a name will
        // invalidate the cache.
        const state = updateResources(initialState, [
          {
            id: r1.id,
            name: "newName",
          },
        ]);

        const result2 = query(state, args);
        expect(result2).not.toBe(result1);
        expect(result2).toEqual({
          [r1.id]: { ...r1, name: "newName" },
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(3);
        expect(reduce.mock.calls).toHaveLength(2);
      });

      it("should return diff with same state and diff args", () => {
        const firstArgs = [r1.id, r2.id];
        const secondArgs = [r1.id, r2.id];
        const result1 = query(initialState, firstArgs);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result2 = query(initialState, secondArgs);
        expect(result2).toBe(result1);
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result3 = query(initialState, firstArgs);
        expect(result3).toBe(result2);
        expect(filter.mock.calls).toHaveLength(3);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result4 = query(initialState, secondArgs);
        expect(result4).toBe(result3);
        expect(filter.mock.calls).toHaveLength(4);
        expect(mapNoArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });
    });

    describe("with args", () => {
      let query;

      beforeEach(() => {
        query = makeStrictQuery({ filter, map: mapWithArgs, reduce });
      });

      it("should return same with same state and same args", () => {
        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result2 = query(initialState, args);
        expect(result2).toBe(result1);
        expect(result2).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return same with updated other state and same args 1", () => {
        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Updating r2 does not affect cached result that only cares about r2.
        const state = updateResources(initialState, [
          {
            id: r3.id,
            obj: {},
          },
        ]);

        const result2 = query(state, args);
        expect(result2).toBe(result1);
        expect(result2).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return same with updated other state and same args 2", () => {
        // eslint-disable-next-line max-nested-callbacks
        mapWithArgs.mockImplementation(resource => ({ ...resource, name: "" }));

        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: { ...r1, name: "" },
          [r2.id]: { ...r2, name: "" },
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Since the map function ignores the name value, updating it should
        // not reset the cached for this query.
        const state = updateResources(initialState, [
          {
            id: r3.id,
            name: "newName",
          },
        ]);

        const result2 = query(state, args);
        expect(result2).toBe(result1);
        expect(result2).toEqual({
          [r1.id]: { ...r1, name: "" },
          [r2.id]: { ...r2, name: "" },
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);
      });

      it("should return diff with updated id state and same args", () => {
        const args = [r1.id, r2.id];
        const result1 = query(initialState, args);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        // Since the mapper returns a value with name, changing a name will
        // invalidate the cache.
        const state = updateResources(initialState, [
          {
            id: r1.id,
            name: "newName",
          },
        ]);

        const result2 = query(state, args);
        expect(result2).not.toBe(result1);
        expect(result2).toEqual({
          [r1.id]: { ...r1, name: "newName" },
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(3);
        expect(reduce.mock.calls).toHaveLength(2);
      });

      it("should return diff with same state and diff args", () => {
        const firstArgs = [r1.id, r2.id];
        const secondArgs = [r1.id, r2.id];

        const result1 = query(initialState, firstArgs);
        expect(result1).toEqual({
          [r1.id]: r1,
          [r2.id]: r2,
        });
        expect(filter.mock.calls).toHaveLength(1);
        expect(mapWithArgs.mock.calls).toHaveLength(2);
        expect(reduce.mock.calls).toHaveLength(1);

        const result2 = query(initialState, secondArgs);
        expect(result2).toBe(result1);
        expect(filter.mock.calls).toHaveLength(2);
        expect(mapWithArgs.mock.calls).toHaveLength(4);
        expect(reduce.mock.calls).toHaveLength(1);

        const result3 = query(initialState, firstArgs);
        expect(result3).toBe(result2);
        expect(filter.mock.calls).toHaveLength(3);
        expect(mapWithArgs.mock.calls).toHaveLength(6);
        expect(reduce.mock.calls).toHaveLength(1);

        const result4 = query(initialState, secondArgs);
        expect(result4).toBe(result3);
        expect(filter.mock.calls).toHaveLength(4);
        expect(mapWithArgs.mock.calls).toHaveLength(8);
        expect(reduce.mock.calls).toHaveLength(1);
      });
    });
  });
});
