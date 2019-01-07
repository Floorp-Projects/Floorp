/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getLibraryFromUrl } from "../getLibraryFromUrl";

describe("getLibraryFromUrl", () => {
  describe("When Preact is on the frame", () => {
    it("should return Preact and not React", () => {
      const frame = {
        displayName: "name",
        location: {
          line: 12
        },
        source: {
          url: "https://cdnjs.cloudflare.com/ajax/libs/preact/8.2.5/preact.js"
        }
      };

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
        "vue.runtime.min.js"
      ];

      buildTypeList.forEach(buildType => {
        const frame = {
          displayName: "name",
          location: {
            line: 42
          },
          source: {
            url: `https://cdnjs.cloudflare.com/ajax/libs/vue/2.5.17/${buildType}`
          }
        };

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
        "https://debugger-react-example.com/react/test.js"
      ];
      notReactUrlList.forEach(notReactUrl => {
        const frame = {
          displayName: "name",
          location: {
            line: 12
          },
          source: {
            url: notReactUrl
          }
        };
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
        "/node_modules/react-dom/test.js"
      ];
      reactUrlList.forEach(reactUrl => {
        const frame = {
          displayName: "name",
          location: {
            line: 12
          },
          source: {
            url: reactUrl
          }
        };
        expect(getLibraryFromUrl(frame)).toEqual("React");
      });
    });
  });

  describe("When zone.js is on the frame", () => {
    it("should not return Angular when no callstack", () => {
      const frame = {
        displayName: "name",
        location: {
          line: 12
        },
        source: {
          url: "/node_modules/zone/zone.js"
        }
      };

      expect(getLibraryFromUrl(frame)).toEqual(null);
    });

    it("should not return Angular when stack without Angular frames", () => {
      const frame = {
        displayName: "name",
        location: {
          line: 12
        },
        source: {
          url: "/node_modules/zone/zone.js"
        }
      };
      const callstack = [frame];

      expect(getLibraryFromUrl(frame, callstack)).toEqual(null);
    });

    it("should return Angular when stack with Angular frames", () => {
      const frame = {
        displayName: "name",
        location: {
          line: 12
        },
        source: {
          url: "/node_modules/zone/zone.js"
        }
      };
      const callstack = [
        frame,
        {
          source: {
            url: "https://cdnjs.cloudflare.com/ajax/libs/angular/angular.js"
          }
        }
      ];

      expect(getLibraryFromUrl(frame, callstack)).toEqual("Angular");
    });
  });
});
