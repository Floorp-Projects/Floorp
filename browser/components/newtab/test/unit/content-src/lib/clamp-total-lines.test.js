import {clampTotalLines} from "content-src/lib/clamp-total-lines";
import {GlobalOverrider} from "test/unit/utils";

const HEIGHT = 20;

describe("clampTotalLines", () => {
  let globals;
  let sandbox;
  let children;

  const node = () => document.createElement("div");
  function child(lines, clamp) {
    const ret = node();
    ret.classList.add("clamp");
    sandbox.stub(ret, "scrollHeight").get(() => lines * HEIGHT);
    if (clamp) {
      ret.dataset.clamp = clamp;
    }
    return ret;
  }
  function test(totalLines) {
    const parentNode = node();
    parentNode.setAttribute("data-total-lines", totalLines);

    // Convert children line sizes into clamp nodes with appropriate height
    children = children.map(childLines => parentNode.appendChild(
      typeof childLines === "number" ? child(childLines) : childLines));

    clampTotalLines(parentNode);
  }
  function check(index, lines) {
    assert.propertyVal(children[index].style, "webkitLineClamp", `${lines}`);
  }

  beforeEach(() => {
    globals = new GlobalOverrider();
    ({sandbox} = globals);
    children = [];
    globals.set("getComputedStyle", () => ({lineHeight: `${HEIGHT}px`}));
  });
  afterEach(() => {
    globals.restore();
  });

  it("should do nothing with nothing", () => {
    clampTotalLines();
  });
  it("should clamp single long child to total", () => {
    children = [10];

    test(6);

    check(0, 6);
  });
  it("should clamp single short child to its height", () => {
    children = [2];

    test(6);

    check(0, 2);
  });
  it("should clamp long children preferring first", () => {
    children = [10, 10];

    test(6);

    check(0, 5);
    check(1, 1);
  });
  it("should clamp short children to their heights", () => {
    children = [2, 2];

    test(6);

    check(0, 2);
    check(1, 2);
  });
  it("should give remainder to last child", () => {
    children = [4, 4];

    test(6);

    check(0, 4);
    check(1, 2);
  });
  it("should handle smaller totals", () => {
    children = [3, 3];

    test(4);

    check(0, 3);
    check(1, 1);
  });
  it("should allow explicit child clamp", () => {
    children = [child(3, 2), 3];

    test(4);

    check(0, 2);
    check(1, 2);
  });
  it("should skip non-clamp children", () => {
    children = [node(), 3, node(), 3];

    test(4);

    check(1, 3);
    check(3, 1);
  });
  it("should skip no-height children", () => {
    children = [0, 3, 0, 3];

    test(4);

    check(1, 3);
    check(3, 1);
  });
  it("should handle larger totals", () => {
    children = [4, 4];

    test(8);

    check(0, 4);
    check(1, 4);
  });
  it("should handle even larger totals", () => {
    children = [4, 4];

    test(10);

    check(0, 4);
    check(1, 4);
  });
  it("should clamp many children preferring earlier", () => {
    children = [2, 2, 2, 2];

    test(6);

    check(0, 2);
    check(1, 2);
    check(2, 1);
    check(3, 1);
  });
  it("should give lines to children that need them", () => {
    children = [1, 2, 3, 4];

    test(8);

    check(0, 1);
    check(1, 2);
    check(2, 3);
    check(3, 2);
  });
});
