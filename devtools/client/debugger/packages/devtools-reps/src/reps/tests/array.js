/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { shallow } = require("enzyme");

const { REPS, getRep } = require("../rep");

const { MODE } = require("../constants");
const { ArrayRep, Rep } = REPS;
const { maxLengthMap } = ArrayRep;

describe("Array", () => {
  it("selects Array Rep as expected", () => {
    const stub = [];
    expect(getRep(stub, undefined, true)).toBe(ArrayRep.rep);
  });

  it("renders empty array as expected", () => {
    const object = [];
    const renderRep = props => shallow(Rep({ object, noGrip: true, ...props }));

    const defaultOutput = "[]";
    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });

  it("renders basic array as expected", () => {
    const object = [1, "foo", {}];
    const renderRep = props => shallow(Rep({ object, noGrip: true, ...props }));

    const defaultOutput = '[ 1, "foo", {} ]';
    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("[…]");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });

  it("renders array with more than SHORT mode max props as expected", () => {
    const object = Array(maxLengthMap.get(MODE.SHORT) + 1).fill("foo");
    const renderRep = props => shallow(Rep({ object, noGrip: true, ...props }));

    const defaultShortOutput = `[ ${Array(maxLengthMap.get(MODE.SHORT))
      .fill('"foo"')
      .join(", ")}, … ]`;
    expect(renderRep({ mode: undefined }).text()).toBe(defaultShortOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("[…]");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultShortOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(
      `[ ${Array(maxLengthMap.get(MODE.SHORT) + 1)
        .fill('"foo"')
        .join(", ")} ]`
    );
  });

  it("renders array with more than LONG mode maximum props as expected", () => {
    const object = Array(maxLengthMap.get(MODE.LONG) + 1).fill("foo");
    const renderRep = props => shallow(Rep({ object, noGrip: true, ...props }));

    const defaultShortOutput = `[ ${Array(maxLengthMap.get(MODE.SHORT))
      .fill('"foo"')
      .join(", ")}, … ]`;
    expect(renderRep({ mode: undefined }).text()).toBe(defaultShortOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("[…]");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultShortOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(
      `[ ${Array(maxLengthMap.get(MODE.LONG))
        .fill('"foo"')
        .join(", ")}, … ]`
    );
  });

  it("renders recursive array as expected", () => {
    const object = [1];
    object.push(object);
    const renderRep = props => shallow(Rep({ object, noGrip: true, ...props }));

    const defaultOutput = "[ 1, […] ]";
    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("[…]");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });

  it("renders array containing an object as expected", () => {
    const object = [
      {
        p1: "s1",
        p2: ["a1", "a2", "a3"],
        p3: "s3",
        p4: "s4",
      },
    ];
    const renderRep = props => shallow(Rep({ object, noGrip: true, ...props }));

    const defaultOutput = "[ {…} ]";
    expect(renderRep({ mode: undefined }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.TINY }).text()).toBe("[…]");
    expect(renderRep({ mode: MODE.SHORT }).text()).toBe(defaultOutput);
    expect(renderRep({ mode: MODE.LONG }).text()).toBe(defaultOutput);
  });
});
