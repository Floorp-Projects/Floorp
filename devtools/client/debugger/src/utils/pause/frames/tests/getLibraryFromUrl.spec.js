/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getLibraryFromUrl } from "../getLibraryFromUrl";
import { makeMockFrameWithURL } from "../../../test-mockup";

describe("getLibraryFromUrl", () => {
  describe("When Preact is on the frame", () => {
    it("should return Preact and not React", () => {
      const frame = makeMockFrameWithURL(
        "https://cdnjs.cloudflare.com/ajax/libs/preact/8.2.5/preact.js"
      );
      expect(getLibraryFromUrl(frame)).toEqual("Preact");
    });
  });

  describe("When Vue is on the frame", () => {
    it("should return VueJS for different builds", () => {
      const buildTypeList = [
        "vue.js",
        "vue.common.js",
        "vue.esm.js",
        "vue.runtime.js",
        "vue.runtime.common.js",
        "vue.runtime.esm.js",
        "vue.min.js",
        "vue.runtime.min.js",
      ];

      buildTypeList.forEach(buildType => {
        const frame = makeMockFrameWithURL(
          `https://cdnjs.cloudflare.com/ajax/libs/vue/2.5.17/${buildType}`
        );
        expect(getLibraryFromUrl(frame)).toEqual("VueJS");
      });
    });
  });

  describe("When React is in the URL", () => {
    it("should not return React if it is not part of the filename", () => {
      const notReactUrlList = [
        "https://react.js.com/test.js",
        "https://debugger-example.com/test.js",
        "https://debugger-react-example.com/test.js",
        "https://debugger-react-example.com/react/test.js",
        "https://debugger-example.com/react-contextmenu.js",
      ];
      notReactUrlList.forEach(notReactUrl => {
        const frame = makeMockFrameWithURL(notReactUrl);
        expect(getLibraryFromUrl(frame)).toBeNull();
      });
    });
    it("should return React if it is part of the filename", () => {
      const reactUrlList = [
        "https://debugger-example.com/react.js",
        "https://debugger-example.com/react.development.js",
        "https://debugger-example.com/react.production.min.js",
        "https://debugger-react-example.com/react.js",
        "https://debugger-react-example.com/react/react.js",
        "/node_modules/react/test.js",
        "/node_modules/react-dom/test.js",
      ];
      reactUrlList.forEach(reactUrl => {
        const frame = makeMockFrameWithURL(reactUrl);
        expect(getLibraryFromUrl(frame)).toEqual("React");
      });
    });
  });

  describe("When Angular is in the URL", () => {
    it("should return Angular for AngularJS (1.x)", () => {
      const frame = makeMockFrameWithURL(
        "https://cdnjs.cloudflare.com/ajax/libs/angular/angular.js"
      );
      expect(getLibraryFromUrl(frame)).toEqual("Angular");
    });

    it("should return Angular for Angular (2.x)", () => {
      const frame = makeMockFrameWithURL(
        "https://stackblitz.io/turbo_modules/@angular/core@7.2.4/bundles/core.umd.js"
      );
      expect(getLibraryFromUrl(frame)).toEqual("Angular");
    });

    it("should not return Angular for Angular components", () => {
      const frame = makeMockFrameWithURL(
        "https://firefox-devtools-angular-log.stackblitz.io/~/src/app/hello.component.ts"
      );
      expect(getLibraryFromUrl(frame)).toBeNull();
    });
  });

  describe("When zone.js is on the frame", () => {
    it("should not return Angular when no callstack", () => {
      const frame = makeMockFrameWithURL("/node_modules/zone/zone.js");
      expect(getLibraryFromUrl(frame)).toEqual(null);
    });

    it("should not return Angular when stack without Angular frames", () => {
      const frame = makeMockFrameWithURL("/node_modules/zone/zone.js");
      const callstack = [frame];

      expect(getLibraryFromUrl(frame, callstack)).toEqual(null);
    });

    it("should return Angular when stack with AngularJS (1.x) frames", () => {
      const frame = makeMockFrameWithURL("/node_modules/zone/zone.js");
      const callstack = [
        frame,
        makeMockFrameWithURL(
          "https://cdnjs.cloudflare.com/ajax/libs/angular/angular.js"
        ),
      ];

      expect(getLibraryFromUrl(frame, callstack)).toEqual("Angular");
    });
  });
});
