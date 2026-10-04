// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getPublicContainerOptions } from "../containerUtils.ts";
import { SsbContainerSelect } from "../SsbContainerSelect.tsx";
import { h, safeRender } from "@nora/preact-xul";
import { signal } from "@preact/signals";
import { act } from "preact/test-utils";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name: "container selection clears the previous native checked item",
    async fn() {
      const { PwaContainerExperiment } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/PwaContainerExperiment.sys.mjs",
      );
      const { ContextualIdentityService } = ChromeUtils.importESModule(
        "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
      );
      const experimentDescriptor = Object.getOwnPropertyDescriptor(
        PwaContainerExperiment,
        "isEnabled",
      );
      const identitiesDescriptor = Object.getOwnPropertyDescriptor(
        ContextualIdentityService,
        "getPublicIdentities",
      );
      const host = document.createXULElement("vbox");
      document.documentElement.appendChild(host);
      const selected = signal(0);
      let dispose = () => {};
      try {
        // Supply deterministic options without creating real containers or
        // persisting an experiment enrollment in the test profile.
        Object.defineProperty(PwaContainerExperiment, "isEnabled", {
          configurable: true,
          writable: true,
          value: () => true,
        });
        Object.defineProperty(
          ContextualIdentityService,
          "getPublicIdentities",
          {
            configurable: true,
            writable: true,
            value: () => [{ userContextId: 9060, name: "Fixture one" }, {
              userContextId: 9061,
              name: "Fixture two",
            }],
          },
        );
        await act(() => {
          dispose = safeRender(() =>
            h(SsbContainerSelect, {
              selectedId: () => selected.value,
              onSelect: (id: number) => {
                selected.value = id;
              },
            }), host);
        });
        const items = [0, 9060, 9061].map((id) => {
          const item = host.querySelector(`menuitem[value="${id}"]`);
          assert(item, `container ${id} renders`);
          assertEquals(
            item.namespaceURI,
            "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
            "container option is native XUL",
          );
          return item;
        });
        const checkSelection = (index: number) => {
          items.forEach((item, itemIndex) => {
            const expected = itemIndex === index;
            // These are normal menuitems, not type=radio/checkbox. Pinned
            // daily-1117 XULButtonElement::GetCheckedStateAttribute() maps
            // their :checked state to `selected`, while the PWA checkmark
            // uses `checked`. Do not equate the attribute with :checked.
            // Runtime commit: 1553b7b550dfe555684628d175e0c1701a785d0b.
            assertEquals(
              item.hasAttribute("checked"),
              expected,
              "only the selected container has the checked attribute",
            );
          });
        };
        checkSelection(0);
        for (const index of [1, 2, 0]) {
          await act(() => {
            items[index].dispatchEvent(new Event("command"));
          });
          assertEquals(
            selected.value,
            [0, 9060, 9061][index],
            "command selects the intended container",
          );
          checkSelection(index);
          assertEquals(
            host.querySelector(`menuitem[value="${selected.value}"]`),
            items[index],
            "selection changes retain native option identity",
          );
        }
        await act(() => {
          selected.value = -1;
        });
        checkSelection(-1);
        await act(() => {
          items[0].dispatchEvent(new Event("command"));
        });
        assertEquals(selected.value, 0, "command restores a valid selection");
        checkSelection(0);
        assertEquals(
          host.querySelector('menuitem[value="0"]'),
          items[0],
          "clearing and restoring selection retains the native option",
        );
      } finally {
        dispose();
        host.remove();
        if (experimentDescriptor) {
          Object.defineProperty(
            PwaContainerExperiment,
            "isEnabled",
            experimentDescriptor,
          );
        } else Reflect.deleteProperty(PwaContainerExperiment, "isEnabled");
        if (identitiesDescriptor) {
          Object.defineProperty(
            ContextualIdentityService,
            "getPublicIdentities",
            identitiesDescriptor,
          );
        } else {Reflect.deleteProperty(
            ContextualIdentityService,
            "getPublicIdentities",
          );}
      }
    },
  },
  {
    name: "getPublicContainerOptions includes no-container entry first",
    fn() {
      const options = getPublicContainerOptions();
      assert(
        options.length >= 1,
        "must return at least the no-container option",
      );
      assertEquals(
        options[0].userContextId,
        0,
        "first option must be no container",
      );
      assert(
        options[0].label.length > 0,
        "no-container label must not be empty",
      );
    },
  },
  {
    name: "getPublicContainerOptions uses unique userContextId values",
    fn() {
      const options = getPublicContainerOptions();
      const ids = options.map((option) => option.userContextId);
      const uniqueIds = new Set(ids);
      assertEquals(
        ids.length,
        uniqueIds.size,
        "container option ids must be unique",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("containerUtils.test.ts", tests);
}
