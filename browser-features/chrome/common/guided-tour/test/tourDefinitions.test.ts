// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { getAllTours, getTourById } from "../tour-definitions/index.ts";
import { TOUR_REQUEST_PREF } from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function testRegistryIdsMatchDefinitionIds(): void {
  for (const tour of getAllTours()) {
    const found = getTourById(tour.id);
    assert(found !== undefined, `tour not resolvable by id: ${tour.id}`);
    assertEquals(found?.id, tour.id, `registry key mismatch for ${tour.id}`);
  }
}

function testUnknownTourReturnsUndefined(): void {
  assertEquals(
    getTourById("does-not-exist"),
    undefined,
    "unknown tour id should return undefined",
  );
}

function testToursHaveSteps(): void {
  for (const tour of getAllTours()) {
    assert(tour.steps.length > 0, `tour has no steps: ${tour.id}`);
  }
}

function testStepI18nKeysAreNamespaced(): void {
  for (const tour of getAllTours()) {
    tour.steps.forEach((step, i) => {
      assert(
        step.titleKey.startsWith(`guidedTour.${tour.id}.`),
        `invalid titleKey namespace at ${tour.id}[${i}]: ${step.titleKey}`,
      );
      assert(
        step.descriptionKey.startsWith(`guidedTour.${tour.id}.`),
        `invalid descriptionKey namespace at ${tour.id}[${i}]: ${step.descriptionKey}`,
      );
    });
  }
}

function testStepSelectorsAreValidSyntax(): void {
  for (const tour of getAllTours()) {
    tour.steps.forEach((step, i) => {
      const selectors = [
        step.selector,
        step.waitForSelector,
        step.action?.selector,
        step.advanceOn?.selector,
      ].filter((s): s is string => typeof s === "string");
      for (const selector of selectors) {
        try {
          document.querySelector(selector);
        } catch {
          assert(false, `invalid selector at ${tour.id}[${i}]: ${selector}`);
        }
      }
    });
  }
}

function testPassthroughStepsHaveTarget(): void {
  for (const tour of getAllTours()) {
    tour.steps.forEach((step, i) => {
      if (step.passthrough) {
        assert(
          step.selector !== null,
          `passthrough step needs a selector at ${tour.id}[${i}]`,
        );
      }
    });
  }
}

/** パネル表示済みの連続ステップでツールバーをトグルしない */
function testWorkspacesPanelStepsDoNotRetoggleToolbar(): void {
  const tour = getTourById("workspaces");
  assert(tour !== undefined, "workspaces tour must exist");
  const step4 = tour.steps[3];
  assertEquals(
    step4.action,
    undefined,
    "step 4 must not click toolbar (would toggle panel closed)",
  );
  assertEquals(
    step4.waitForSelector,
    undefined,
    "step 4 assumes panel stays open from step 3",
  );
  assert(step4.passthrough === true, "step 4 needs passthrough for right-click");
  assert(
    step4.keepWorkspacePanelOpen === true,
    "step 4 must keep workspace panel open",
  );
  assert(
    tour.steps[2].keepWorkspacePanelOpen === true,
    "step 3 must keep workspace panel open",
  );
}

function testRequestPrefIsNamespaced(): void {
  assert(
    TOUR_REQUEST_PREF.startsWith("floorp.guidedTour."),
    `invalid request pref: ${TOUR_REQUEST_PREF}`,
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "registry ids match definition ids",
      fn: testRegistryIdsMatchDefinitionIds,
    },
    {
      name: "unknown tour returns undefined",
      fn: testUnknownTourReturnsUndefined,
    },
    { name: "tours have steps", fn: testToursHaveSteps },
    {
      name: "step i18n keys are namespaced",
      fn: testStepI18nKeysAreNamespaced,
    },
    {
      name: "step selectors are valid syntax",
      fn: testStepSelectorsAreValidSyntax,
    },
    {
      name: "passthrough steps have a target",
      fn: testPassthroughStepsHaveTarget,
    },
    {
      name: "workspaces panel steps do not retoggle toolbar",
      fn: testWorkspacesPanelStepsDoNotRetoggleToolbar,
    },
    { name: "request pref is namespaced", fn: testRequestPrefIsNamespaced },
  ];

  await runTests("guided-tour/tourDefinitions.test.ts", tests);
}
