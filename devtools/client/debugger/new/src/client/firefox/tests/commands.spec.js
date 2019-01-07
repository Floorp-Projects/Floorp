/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { setupCommands, clientCommands } from "../commands";

function makeThreadCLient(resp) {
  return {
    pauseGrip: () => ({
      getPrototypeAndProperties: async () => resp
    })
  };
}

describe("firefox commands", () => {
  describe("getProperties", () => {
    it("empty response", async () => {
      const { getProperties } = clientCommands;
      const threadClient = makeThreadCLient({
        ownProperties: {},
        safeGetterValues: {}
      });

      setupCommands({ threadClient });
      const props = await getProperties({});
      expect(props).toMatchSnapshot();
    });

    it("simple properties", async () => {
      const { getProperties } = clientCommands;
      const threadClient = makeThreadCLient({
        ownProperties: {
          obj: { value: "obj" },
          foo: { value: "foo" }
        },
        safeGetterValues: {}
      });

      setupCommands({ threadClient });
      const props = await getProperties({});
      expect(props).toMatchSnapshot();
    });

    it("getter values", async () => {
      const { getProperties } = clientCommands;
      const threadClient = makeThreadCLient({
        ownProperties: {
          obj: { value: "obj" },
          foo: { value: "foo" }
        },
        safeGetterValues: {
          obj: { getterValue: "getter", enumerable: true, writable: false }
        }
      });

      setupCommands({ threadClient });
      const props = await getProperties({});
      expect(props).toMatchSnapshot();
    });

    it("new getter values", async () => {
      const { getProperties } = clientCommands;
      const threadClient = makeThreadCLient({
        ownProperties: {
          foo: { value: "foo" }
        },
        safeGetterValues: {
          obj: { getterValue: "getter", enumerable: true, writable: false }
        }
      });

      setupCommands({ threadClient });
      const props = await getProperties({});
      expect(props).toMatchSnapshot();
    });
  });
});
