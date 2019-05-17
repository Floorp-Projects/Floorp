/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { setupCommands, clientCommands } from "../commands";

function makeThreadCLient(resp) {
  // Coerce this to any to avoid supplying the additional members needed in a
  // thread client.
  return ({
    pauseGrip: () => ({
      getPrototypeAndProperties: async () => resp,
    }),
  }: any);
}

function makeDependencies() {
  return {
    debuggerClient: (null: any),
    supportsWasm: true,
    tabTarget: (null: any),
  };
}

function makeGrip(actor = "") {
  return {
    actor,
    class: "",
    displayClass: "",
    name: "",
    extensible: true,
    location: {
      url: "",
      line: 2,
      column: 34,
    },
    frozen: false,
    ownPropertyLength: 1,
    preview: {},
    sealed: false,
    optimizedOut: false,
    type: "",
  };
}

describe("firefox commands", () => {
  describe("getProperties", () => {
    it("empty response", async () => {
      const { getProperties } = clientCommands;
      const threadClient = makeThreadCLient({
        ownProperties: {},
        safeGetterValues: {},
      });

      setupCommands({ ...makeDependencies(), threadClient });
      const props = await getProperties("", makeGrip());
      expect(props).toMatchSnapshot();
    });

    it("simple properties", async () => {
      const { getProperties } = clientCommands;
      const threadClient = makeThreadCLient({
        ownProperties: {
          obj: { value: "obj" },
          foo: { value: "foo" },
        },
        safeGetterValues: {},
      });

      setupCommands({ ...makeDependencies(), threadClient });
      const props = await getProperties("", makeGrip());
      expect(props).toMatchSnapshot();
    });

    it("getter values", async () => {
      const { getProperties } = clientCommands;
      const threadClient = makeThreadCLient({
        ownProperties: {
          obj: { value: "obj" },
          foo: { value: "foo" },
        },
        safeGetterValues: {
          obj: { getterValue: "getter", enumerable: true, writable: false },
        },
      });

      setupCommands({ ...makeDependencies(), threadClient });
      const props = await getProperties("", makeGrip());
      expect(props).toMatchSnapshot();
    });

    it("new getter values", async () => {
      const { getProperties } = clientCommands;
      const threadClient = makeThreadCLient({
        ownProperties: {
          foo: { value: "foo" },
        },
        safeGetterValues: {
          obj: { getterValue: "getter", enumerable: true, writable: false },
        },
      });

      setupCommands({ ...makeDependencies(), threadClient });
      const props = await getProperties("", makeGrip());
      expect(props).toMatchSnapshot();
    });
  });
});
