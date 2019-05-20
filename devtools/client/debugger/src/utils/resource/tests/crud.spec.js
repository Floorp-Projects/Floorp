/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
declare var describe: (name: string, func: () => void) => void;
declare var it: (desc: string, func: () => void) => void;
declare var expect: (value: any) => any;

import {
  createInitial,
  insertResources,
  removeResources,
  updateResources,
  hasResource,
  getResourceIds,
  getResource,
  getMappedResource,
  type Resource,
  type ResourceIdentity,
} from "..";

type TestResource = Resource<{
  id: string,
  name: string,
  data: number,
  obj: {},
}>;

const makeResource = (id: string): TestResource => ({
  id,
  name: `name-${id}`,
  data: 42,
  obj: {},
});

const mapName = (resource: TestResource) => resource.name;
const mapWithIdent = (resource: TestResource, identity: ResourceIdentity) => ({
  resource,
  identity,
  obj: {},
});

const clone = <T>(v: T): T => (JSON.parse((JSON.stringify(v): any)): any);

describe("resource CRUD operations", () => {
  let r1, r2, r3;
  let originalInitial;
  let initialState;
  beforeEach(() => {
    r1 = makeResource("id-1");
    r2 = makeResource("id-2");
    r3 = makeResource("id-3");

    initialState = createInitial();
    originalInitial = clone(initialState);
  });

  describe("insert", () => {
    it("should work", () => {
      const state = insertResources(initialState, [r1, r2, r3]);

      expect(initialState).toEqual(originalInitial);
      expect(getResource(state, r1.id)).toBe(r1);
      expect(getResource(state, r2.id)).toBe(r2);
      expect(getResource(state, r3.id)).toBe(r3);
    });

    it("should throw on duplicate", () => {
      const state = insertResources(initialState, [r1]);
      expect(() => {
        insertResources(state, [r1]);
      }).toThrow(/already exists/);

      expect(() => {
        insertResources(state, [r2, r2]);
      }).toThrow(/already exists/);
    });

    it("should be a no-op when given no resources", () => {
      const state = insertResources(initialState, []);

      expect(state).toBe(initialState);
    });
  });

  describe("read", () => {
    beforeEach(() => {
      initialState = insertResources(initialState, [r1, r2, r3]);
    });

    it("should allow reading all IDs", () => {
      expect(getResourceIds(initialState)).toEqual([r1.id, r2.id, r3.id]);
    });

    it("should allow checking for existing of an ID", () => {
      expect(hasResource(initialState, r1.id)).toBe(true);
      expect(hasResource(initialState, r2.id)).toBe(true);
      expect(hasResource(initialState, r3.id)).toBe(true);
      expect(hasResource(initialState, "unknownId")).toBe(false);
    });

    it("should allow reading an item", () => {
      expect(getResource(initialState, r1.id)).toBe(r1);
      expect(getResource(initialState, r2.id)).toBe(r2);
      expect(getResource(initialState, r3.id)).toBe(r3);

      expect(() => {
        getResource(initialState, "unknownId");
      }).toThrow(/does not exist/);
    });

    it("should allow reading and mapping an item", () => {
      expect(getMappedResource(initialState, r1.id, mapName)).toBe(r1.name);
      expect(getMappedResource(initialState, r2.id, mapName)).toBe(r2.name);
      expect(getMappedResource(initialState, r3.id, mapName)).toBe(r3.name);

      expect(() => {
        getMappedResource(initialState, "unknownId", mapName);
      }).toThrow(/does not exist/);
    });

    it("should allow reading and mapping an item with identity", () => {
      const r1Ident = getMappedResource(initialState, r1.id, mapWithIdent);
      const r2Ident = getMappedResource(initialState, r2.id, mapWithIdent);

      const state = updateResources(initialState, [{ ...r1, obj: {} }]);

      const r1NewIdent = getMappedResource(state, r1.id, mapWithIdent);
      const r2NewIdent = getMappedResource(state, r2.id, mapWithIdent);

      // The update changed the resource object, but not the identity.
      expect(r1NewIdent.resource).not.toBe(r1Ident.resource);
      expect(r1NewIdent.resource).toEqual(r1Ident.resource);
      expect(r1NewIdent.identity).toBe(r1Ident.identity);

      // The update did not change the r2 resource.
      expect(r2NewIdent.resource).toBe(r2Ident.resource);
      expect(r2NewIdent.identity).toBe(r2Ident.identity);
    });
  });

  describe("update", () => {
    beforeEach(() => {
      initialState = insertResources(initialState, [r1, r2, r3]);
      originalInitial = clone(initialState);
    });

    it("should work", () => {
      const r1Ident = getMappedResource(initialState, r1.id, mapWithIdent);
      const r2Ident = getMappedResource(initialState, r2.id, mapWithIdent);
      const r3Ident = getMappedResource(initialState, r3.id, mapWithIdent);

      const state = updateResources(initialState, [
        {
          id: r1.id,
          data: 21,
        },
        {
          id: r2.id,
          name: "newName",
        },
      ]);

      expect(initialState).toEqual(originalInitial);
      expect(getResource(state, r1.id)).toEqual({ ...r1, data: 21 });
      expect(getResource(state, r2.id)).toEqual({ ...r2, name: "newName" });
      expect(getResource(state, r3.id)).toBe(r3);

      const r1NewIdent = getMappedResource(state, r1.id, mapWithIdent);
      const r2NewIdent = getMappedResource(state, r2.id, mapWithIdent);
      const r3NewIdent = getMappedResource(state, r3.id, mapWithIdent);

      // The update changed the resource object, but not the identity.
      expect(r1NewIdent.resource).not.toBe(r1Ident.resource);
      expect(r1NewIdent.resource).toEqual({
        ...r1Ident.resource,
        data: 21,
      });
      expect(r1NewIdent.identity).toBe(r1Ident.identity);

      // The update changed the resource object, but not the identity.
      expect(r2NewIdent.resource).toEqual({
        ...r2Ident.resource,
        name: "newName",
      });
      expect(r2NewIdent.identity).toBe(r2Ident.identity);

      // The update did not change the r3 resource.
      expect(r3NewIdent.resource).toBe(r3Ident.resource);
      expect(r3NewIdent.identity).toBe(r3Ident.identity);
    });

    it("should throw if not found", () => {
      expect(() => {
        updateResources(initialState, [
          {
            ...r1,
            id: "unknownId",
          },
        ]);
      }).toThrow(/does not exists/);
    });

    it("should be a no-op when new fields are strict-equal", () => {
      const state = updateResources(initialState, [r1]);
      expect(state).toBe(initialState);
    });

    it("should be a no-op when given no resources", () => {
      const state = updateResources(initialState, []);
      expect(state).toBe(initialState);
    });
  });

  describe("delete", () => {
    beforeEach(() => {
      initialState = insertResources(initialState, [r1, r2, r3]);
      originalInitial = clone(initialState);
    });

    it("should work with objects", () => {
      const state = removeResources(initialState, [r1]);

      expect(initialState).toEqual(originalInitial);
      expect(hasResource(state, r1.id)).toBe(false);
      expect(hasResource(state, r2.id)).toBe(true);
      expect(hasResource(state, r3.id)).toBe(true);
    });

    it("should work with object subsets", () => {
      const state = removeResources(initialState, [{ id: r1.id }]);

      expect(initialState).toEqual(originalInitial);
      expect(hasResource(state, r1.id)).toBe(false);
      expect(hasResource(state, r2.id)).toBe(true);
      expect(hasResource(state, r3.id)).toBe(true);
    });

    it("should work with ids", () => {
      const state = removeResources(initialState, [r1.id]);

      expect(initialState).toEqual(originalInitial);
      expect(hasResource(state, r1.id)).toBe(false);
      expect(hasResource(state, r2.id)).toBe(true);
      expect(hasResource(state, r3.id)).toBe(true);
    });

    it("should throw if not found", () => {
      expect(() => {
        removeResources(initialState, [makeResource("unknownId")]);
      }).toThrow(/does not exist/);
      expect(() => {
        removeResources(initialState, [{ id: "unknownId" }]);
      }).toThrow(/does not exist/);
      expect(() => {
        removeResources(initialState, ["unknownId"]);
      }).toThrow(/does not exist/);
    });

    it("should be a no-op when given no resources", () => {
      const state = removeResources(initialState, []);
      expect(state).toBe(initialState);
    });
  });
});
