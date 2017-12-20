/* globals assert, beforeEach, describe, it */
import {_PerfService} from "common/PerfService.jsm";
import {FakePerformance} from "test/unit/utils.js";

let perfService;

describe("_PerfService", () => {
  let sandbox;
  let fakePerfObj;

  beforeEach(() => {
    sandbox = sinon.sandbox.create();
    fakePerfObj = new FakePerformance();
    perfService = new _PerfService({performanceObj: fakePerfObj});
  });

  afterEach(() => {
    sandbox.restore();
  });

  describe("#absNow", () => {
    it("should return a number > the time origin", () => {
      const absNow = perfService.absNow();

      assert.isAbove(absNow, perfService.timeOrigin);
    });
  });
  describe("#getEntriesByName", () => {
    it("should call getEntriesByName on the appropriate Window.performance",
    () => {
      sandbox.spy(fakePerfObj, "getEntriesByName");

      perfService.getEntriesByName("monkey", "mark");

      assert.calledOnce(fakePerfObj.getEntriesByName);
      assert.calledWithExactly(fakePerfObj.getEntriesByName, "monkey", "mark");
    });

    it("should return entries with the given name", () => {
      sandbox.spy(fakePerfObj, "getEntriesByName");
      perfService.mark("monkey");
      perfService.mark("dog");

      let marks = perfService.getEntriesByName("monkey", "mark");

      assert.isArray(marks);
      assert.lengthOf(marks, 1);
      assert.propertyVal(marks[0], "name", "monkey");
    });
  });

  describe("#getMostRecentAbsMarkStartByName", () => {
    it("should throw an error if there is no mark with the given name", () => {
      function bogusGet() {
        perfService.getMostRecentAbsMarkStartByName("rheeeet");
      }

      assert.throws(bogusGet, Error, /No marks with the name/);
    });

    it("should return the Number from the most recent mark with the given name + the time origin",
      () => {
        perfService.mark("dog");
        perfService.mark("dog");

        let absMarkStart = perfService.getMostRecentAbsMarkStartByName("dog");

        // 2 because we want the result of the 2nd call to mark, and an instance
        // of FakePerformance just returns the number of time mark has been
        // called.
        assert.equal(absMarkStart - perfService.timeOrigin, 2);
      });
  });

  describe("#mark", () => {
    it("should call the wrapped version of mark", () => {
      sandbox.spy(fakePerfObj, "mark");

      perfService.mark("monkey");

      assert.calledOnce(fakePerfObj.mark);
      assert.calledWithExactly(fakePerfObj.mark, "monkey");
    });
  });

  describe("#timeOrigin", () => {
    it("should get the origin of the wrapped performance object", () => {
      assert.equal(perfService.timeOrigin, fakePerfObj.timeOrigin);
    });
  });
});
