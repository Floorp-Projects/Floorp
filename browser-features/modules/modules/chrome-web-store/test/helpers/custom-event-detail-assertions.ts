// SPDX-License-Identifier: MPL-2.0
import {
  assert,
  assertEquals,
} from "../../../../../chrome/test/utils/test_harness.ts";
import type {
  Caught,
  DocumentContract,
} from "./custom-event-detail-test-types.ts";

function value<T>(result: Caught<T>, label: string): T {
  assert(result.ok && "value" in result, label + " succeeds");
  return result.value as T;
}

/** Expectations measured with the same public program in Chrome 155. */
export function assertDetailContract(
  document: DocumentContract,
  chrome: boolean,
): void {
  for (const kind of ["data", "accessor", "functions"]) {
    const payload = document.content.payloads.find((item) =>
      item.kind === kind
    );
    assert(payload, "actual source payload " + kind);
    const events = document.page.events.filter((item) => item.kind === kind);
    assertEquals(events.length, 2, "two real events reuse the original object");
    for (const source of payload.sourceEvents) {
      assert(source.sameOriginalDetail, "source world reads original detail");
      assert(
        source.repeatedGetterIdentity,
        "source world retains detail identity",
      );
      assertEquals(
        value(source.originalAlias, "source alias"),
        true,
        "source alias preserved",
      );
      assertEquals(
        value(source.originalCycle, "source cycle"),
        true,
        "source cycle preserved",
      );
    }
    for (const snapshot of payload.snapshots) {
      assertEquals(
        snapshot.sharedCount,
        snapshot.phase === 1 ? 1 : 11,
        "page writes never mutate source data",
      );
      assertEquals(
        value(snapshot.pageAddedTag, "source page-added lookup"),
        null,
        "page-added objects stay in page",
      );
      assertEquals(
        snapshot.deletedByPage,
        false,
        "page deletion never deletes source property",
      );
      const getterCalls = chrome && kind === "accessor"
        ? snapshot.phase * 2
        : 0;
      assertEquals(
        snapshot.getterCalls,
        getterCalls,
        "one source accessor evaluation per cross-world detail getter",
      );
      assertEquals(
        snapshot.getterReceivers.length,
        getterCalls,
        "all source accessor invocations are accounted for",
      );
      assert(
        snapshot.getterReceivers.every((receiver) => receiver.isDetail),
        "source getter receives original detail",
      );
      assertEquals(snapshot.setterCalls, 0, "no source setter crosses worlds");
      assertEquals(
        snapshot.setterReceivers.length,
        0,
        "no source setter receiver exists",
      );
      assertEquals(snapshot.throwCalls, 0, "source thrower is never exposed");
      assertEquals(
        snapshot.calls.length,
        0,
        "source functions are never exposed",
      );
    }
    for (const event of events) {
      assertEquals(
        value(event.detailRead, "public detail getter").type,
        "object",
        "object or null detail result",
      );
      if (!chrome) {
        assertEquals(
          event.detailIsNull,
          false,
          "ordinary Firefox retains opaque detail wrapper",
        );
        assertEquals(
          value(event.repeatedDetailGetterIdentity, "Firefox repeated getter"),
          true,
          "ordinary Firefox wrapper identity preserved",
        );
        if (event.phase === 2) {
          assertEquals(
            event.repeatedEventDetailIdentity,
            true,
            "ordinary Firefox original identity across events",
          );
        }
        for (
          const denied of [
            event.token,
            event.aliases,
            event.ownKeys,
            event.call,
            event.explicitReturnedGlobal,
          ]
        ) {
          assert(
            !denied.ok && denied.error?.name === "Error" &&
              denied.error.message.includes("Permission denied"),
            "ordinary Firefox cross-principal object access remains denied",
          );
        }
        continue;
      }
      if (kind === "functions") {
        assertEquals(
          event.detailIsNull,
          true,
          "uncloneable function payload becomes null as in Chrome",
        );
        assertEquals(
          value(event.repeatedDetailGetterIdentity, "null repeated getter"),
          true,
          "null repeated result",
        );
        for (
          const failed of [
            event.token,
            event.aliases,
            event.call,
            event.explicitReturnedGlobal,
          ]
        ) {
          assert(
            !failed.ok && failed.error?.name === "TypeError",
            "null detail grants no function or global capability",
          );
        }
        continue;
      }
      assertEquals(
        event.detailIsNull,
        false,
        "cloneable detail is a real object",
      );
      assertEquals(
        value(event.repeatedDetailGetterIdentity, "Chrome repeated getter"),
        false,
        "every getter creates a fresh snapshot",
      );
      assertEquals(
        value(event.token, "cloned token"),
        document.token,
        "cloned data belongs to owned document",
      );
      assertEquals(
        value(event.observedKind, "cloned payload kind"),
        kind,
        "cloned payload kind preserved",
      );
      const aliases = value(event.aliases, "cloned aliases and cycle");
      assert(
        aliases.aliasIsShared && aliases.arrayElementIsShared &&
          aliases.selfIsDetail,
        "aliases and cycles survive inside each snapshot",
      );
      const before = value(event.beforePageWrite, "data before local mutation");
      assertEquals(
        before.sharedCount,
        event.phase === 1 ? 1 : 11,
        "fresh snapshot observes current source data",
      );
      assertEquals(
        before.contentAddedTag,
        event.phase === 1 ? null : "content-added",
        "source additions appear in later snapshot",
      );
      assertEquals(
        before.deletedByContent,
        event.phase === 2,
        "source deletions appear in later snapshot",
      );
      const keys = value(event.ownKeys, "real clone own keys");
      assert(
        keys.includes("token") && keys.includes("shared") &&
          keys.includes("self"),
        "real data keys cloned",
      );
      const prototype = value(event.objectPrototype, "clone prototype");
      assert(
        prototype.hasPageObjectPrototype && prototype.constructorIsPageObject,
        "clone uses receiving page intrinsics",
      );
      assertEquals(
        value(event.pageMutation.shared, "page local write"),
        event.phase === 1 ? 7 : 13,
        "page can mutate its local clone",
      );
      assertEquals(
        value(event.pageMutation.added, "page local addition"),
        true,
        "page can add local property",
      );
      assertEquals(
        value(event.pageMutation.deleted, "page local deletion"),
        true,
        "page can delete local property",
      );
      if (event.phase === 2) {
        assertEquals(
          event.repeatedEventDetailIdentity,
          false,
          "reused source object is not cached across events",
        );
        assert(
          event.livePriorReference,
          "first snapshot reference retained by page",
        );
        const prior = value(
          event.livePriorReference,
          "retained prior snapshot",
        );
        assertEquals(
          prior.priorSharedIsCurrentShared,
          false,
          "snapshots do not share cross-read objects",
        );
        assertEquals(
          prior.priorSharedCount,
          7,
          "source update leaves prior page snapshot intact",
        );
      }
      if (kind === "accessor") {
        const descriptor = value(
          event.accessor.descriptor,
          "cloned accessor descriptor",
        );
        assert(
          descriptor.configurable && descriptor.enumerable &&
            descriptor.isData && descriptor.writable &&
            descriptor.valueIsShared,
          "source accessor becomes writable data preserving alias",
        );
        assertEquals(
          descriptor.getterType,
          "undefined",
          "source getter function is not exposed",
        );
        assertEquals(
          descriptor.setterType,
          "undefined",
          "source setter function is not exposed",
        );
        assertEquals(
          value(event.accessor.read, "cloned accessor value"),
          true,
          "accessor value aliases cloned shared data",
        );
      }
      for (const failed of [event.call, event.explicitReturnedGlobal]) {
        assert(
          !failed.ok && failed.error?.name === "TypeError",
          "clone exposes no source functions or globals",
        );
      }
    }
  }
  for (const type of Object.values(document.page.unexposedGlobals)) {
    assertEquals(
      type,
      "undefined",
      "unexported content globals and extension APIs stay private",
    );
  }
}
