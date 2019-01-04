/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getFramework } from "../frameworks";
import { getSource, getOriginalSource } from "./helpers";
import { setSource } from "../sources";

describe("Parser.frameworks", () => {
  describe("no framework", () => {
    it("is undefined when no framework", () => {
      const source = getOriginalSource("frameworks/plainJavascript");
      setSource(source);
      expect(getFramework(source.id)).toBeUndefined();
    });

    it("does not get confused with angular (#6833)", () => {
      const source = getOriginalSource("frameworks/angular1FalsePositive");
      setSource(source);
      expect(getFramework(source.id)).toBeUndefined();
    });
  });

  describe("react", () => {
    it("recognizes ES6 React component", () => {
      const source = getOriginalSource("frameworks/reactComponent");
      setSource(source);
      expect(getFramework(source.id)).toBe("React");
    });

    it("recognizes ES5 React component", () => {
      const source = getSource("frameworks/reactComponentEs5");
      setSource(source);
      expect(getFramework(source.id)).toBe("React");
    });
  });

  describe("angular 1.*", () => {
    it("recognizes Angular 1 module", () => {
      const source = getOriginalSource("frameworks/angular1Module");
      setSource(source);
      expect(getFramework(source.id)).toBe("Angular");
    });
  });

  describe("vue", () => {
    it("recognizes declarative Vue file", () => {
      const source = getOriginalSource("frameworks/vueFileDeclarative");
      setSource(source);
      expect(getFramework(source.id)).toBe("Vue");
    });

    it("recognizes component Vue file", () => {
      const source = getOriginalSource("frameworks/vueFileComponent");
      setSource(source);
      expect(getFramework(source.id)).toBe("Vue");
    });
  });
});
