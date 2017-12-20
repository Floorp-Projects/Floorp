import {Dedupe} from "common/Dedupe.jsm";

describe("Dedupe", () => {
  let instance;
  beforeEach(() => {
    instance = new Dedupe();
  });
  describe("group", () => {
    it("should remove duplicates inside the groups", () => {
      const beforeItems = [[1, 1, 1], [2, 2, 2], [3, 3, 3]];
      const afterItems = [[1], [2], [3]];
      assert.deepEqual(instance.group(...beforeItems), afterItems);
    });
    it("should remove duplicates between groups, favouring earlier groups", () => {
      const beforeItems = [[1, 2, 3], [2, 3, 4], [3, 4, 5]];
      const afterItems = [[1, 2, 3], [4], [5]];
      assert.deepEqual(instance.group(...beforeItems), afterItems);
    });
    it("should remove duplicates from groups of objects", () => {
      instance = new Dedupe(item => item.id);
      const beforeItems = [[{id: 1}, {id: 1}, {id: 2}], [{id: 1}, {id: 3}, {id: 2}], [{id: 1}, {id: 2}, {id: 5}]];
      const afterItems = [[{id: 1}, {id: 2}], [{id: 3}], [{id: 5}]];
      assert.deepEqual(instance.group(...beforeItems), afterItems);
    });
  });
});
