/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  addHighlightToTargetSiblings,
  removeHighlightForTargetSiblings,
} from "../Popup";

describe("addHighlightToTargetSiblings", () => {
  it("should add preview highlight class to related target siblings", async () => {
    const div = document.createElement("div");
    const divChildren = ["a", "divided", "token"];
    divChildren.forEach(function (span) {
      const child = document.createElement("span");
      const text = document.createTextNode(span);
      child.appendChild(text);
      child.classList.add("cm-property");
      div.appendChild(child);
    });

    const target = div.children[1];
    const props = {
      preview: {
        expression: "adividedtoken",
      },
    };

    addHighlightToTargetSiblings(target, props);

    const previous = target.previousElementSibling;
    expect(previous.className.includes("preview-token")).toEqual(true);

    const next = target.nextElementSibling;
    expect(next.className.includes("preview-token")).toEqual(true);
  });

  it("should not add preview highlight class to target's related siblings after non-element nodes", () => {
    const div = document.createElement("div");

    const elementBeforePeriod = document.createElement("span");
    elementBeforePeriod.innerHTML = "object";
    elementBeforePeriod.classList.add("cm-property");
    div.appendChild(elementBeforePeriod);

    const period = document.createTextNode(".");
    div.appendChild(period);

    const target = document.createElement("span");
    target.innerHTML = "property";
    target.classList.add("cm-property");
    div.appendChild(target);

    const anotherPeriod = document.createTextNode(".");
    div.appendChild(anotherPeriod);

    const elementAfterPeriod = document.createElement("span");
    elementAfterPeriod.innerHTML = "anotherProperty";
    elementAfterPeriod.classList.add("cm-property");
    div.appendChild(elementAfterPeriod);

    const props = {
      preview: {
        expression: "object.property.anotherproperty",
      },
    };
    addHighlightToTargetSiblings(target, props);

    expect(elementBeforePeriod.className.includes("preview-token")).toEqual(
      false
    );
    expect(elementAfterPeriod.className.includes("preview-token")).toEqual(
      false
    );
  });
});

describe("removeHighlightForTargetSiblings", () => {
  it("should remove preview highlight class from target's related siblings", async () => {
    const div = document.createElement("div");
    const divChildren = ["a", "divided", "token"];
    divChildren.forEach(function (span) {
      const child = document.createElement("span");
      const text = document.createTextNode(span);
      child.appendChild(text);
      child.classList.add("preview-token");
      div.appendChild(child);
    });
    const target = div.children[1];

    removeHighlightForTargetSiblings(target);

    const previous = target.previousElementSibling;
    expect(previous.className.includes("preview-token")).toEqual(false);

    const next = target.nextElementSibling;
    expect(next.className.includes("preview-token")).toEqual(false);
  });
});
