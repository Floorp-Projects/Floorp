// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";
import { NRPwaManagerParent } from "../NRPwaManagerParent.sys.mts";

interface IdentityService {
  getPublicIdentities(): {
    userContextId: number;
    color: string;
    name?: string;
    l10nId?: string;
    l10nID?: string;
  }[];
  getUserContextLabel(userContextId: number): string;
}

async function readContainers(): Promise<string> {
  const parent = Object.create(
    NRPwaManagerParent.prototype,
  ) as NRPwaManagerParent;
  const replies: { name: string; data: unknown }[] = [];
  Object.defineProperty(parent, "sendAsyncMessage", {
    value: (name: string, data: unknown) => replies.push({ name, data }),
  });

  await parent.receiveMessage({
    name: "PwaManager:GetContainers",
  } as ReceiveMessageArgument);

  assertEquals(replies.length, 1, "the actor must send exactly one reply");
  assertEquals(
    replies[0].name,
    "PwaManager:GetContainers",
    "the actor must use the requested response topic",
  );
  assert(typeof replies[0].data === "string", "containers must be serialized");
  return replies[0].data;
}

export async function runAllTests(): Promise<void> {
  const { ContextualIdentityService: service } = ChromeUtils.importESModule(
    "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
  ) as { ContextualIdentityService: IdentityService };
  const { PwaContainerExperiment: experiment } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/PwaContainerExperiment.sys.mjs",
  ) as { PwaContainerExperiment: { isEnabled(): boolean } };

  await runTests("NRPwaManagerContainers.test.mts", [
    {
      name:
        "manager resolves legacy, current and custom container labels through the service",
      async fn() {
        const originalEnabled = experiment.isEnabled;
        const originalIdentities = service.getPublicIdentities;
        const originalLabel = service.getUserContextLabel;
        try {
          experiment.isEnabled = () => true;
          service.getPublicIdentities = () => [
            {
              userContextId: 1,
              color: "blue",
              l10nID: "userContextPersonal.label",
            },
            { userContextId: 2, color: "orange", l10nId: "user-context-work" },
            { userContextId: 7, color: "purple", name: "Private Container" },
          ];
          const labels = new Map([
            [1, "個人用"],
            [2, "仕事用"],
            [7, "Private Container"],
          ]);
          service.getUserContextLabel = (id) => labels.get(id) ?? "";

          assertEquals(
            await readContainers(),
            JSON.stringify([
              { userContextId: 1, name: "個人用", color: "#37adff" },
              { userContextId: 2, name: "仕事用", color: "#ff9f00" },
              { userContextId: 7, name: "Private Container", color: "#af51f5" },
            ]),
            "manager responses must preserve localized names, custom names and colors",
          );
        } finally {
          experiment.isEnabled = originalEnabled;
          service.getPublicIdentities = originalIdentities;
          service.getUserContextLabel = originalLabel;
        }
      },
    },
    {
      name: "manager returns no containers when the experiment is disabled",
      async fn() {
        const originalEnabled = experiment.isEnabled;
        const originalIdentities = service.getPublicIdentities;
        let identityReads = 0;
        try {
          experiment.isEnabled = () => false;
          service.getPublicIdentities = () => {
            identityReads++;
            return [];
          };

          assertEquals(
            await readContainers(),
            "[]",
            "the gate must remain active",
          );
          assertEquals(
            identityReads,
            0,
            "disabled support must not read identities",
          );
        } finally {
          experiment.isEnabled = originalEnabled;
          service.getPublicIdentities = originalIdentities;
        }
      },
    },
  ]);
}
