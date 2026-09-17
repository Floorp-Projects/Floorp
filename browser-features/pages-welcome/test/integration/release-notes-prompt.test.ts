// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import {
  button,
  calls,
  click,
  element,
  executeCases,
  faults,
  pause,
  prefs,
  runFixture,
  until,
  writes,
} from "../../../../libs/ui/test/page-fixture.ts";

const MODE = "floorp.releaseNotes.mode";
const CONFIRMED = "floorp.releaseNotes.choiceConfirmed";
const selector = (mode: string) => `input[type=radio][value="${mode}"]`;
const savedChoices = () =>
  writes.filter((write) => [MODE, CONFIRMED].includes(write.method));
const closed = () =>
  calls.some((call) => call.method === "NRDismissWelcomePage");

export async function runPageTests() {
  await until(
    () => document.querySelector("#release-notes-content"),
    "Prompt not rendered",
  );
  const parameters = new URLSearchParams(location.search);
  const tests: TestCase[] = [];
  if (parameters.has("readFailure")) {
    tests.push({
      name: "Failed loading preserves settings and still permits dismissal",
      fn: async () => {
        await until(
          () => document.querySelector('[role="alert"]'),
          "Load error missing",
        );
        assert(
          [...document.querySelectorAll<HTMLInputElement>(
            'input[type="radio"]',
          )].every((input) => input.disabled),
          "Choice enabled after load failure",
        );
        const save = [...document.querySelectorAll<HTMLButtonElement>("button")]
          .find((item) => item.textContent === "Save selection");
        assert(save?.disabled, "Save enabled after load failure");
        await button("Not now");
        assert(closed(), "Cannot dismiss failed load");
        assertEquals(savedChoices().length, 0, "Dismissal wrote settings");
        assert(
          !document.querySelector("#release-notes-current-setting"),
          "Load failure displayed an unverified current setting",
        );
      },
    });
  } else {
    await until(
      () => !element<HTMLInputElement>(selector("blocking")).disabled,
      "Choices not ready",
    );
    if (parameters.has("confirmed")) {
      tests.push({
        name: "Confirmed existing choice loads without rewriting it",
        fn: () => {
          assert(
            element<HTMLInputElement>(selector("support")).checked,
            "Confirmed choice lost",
          );
          assertEquals(savedChoices().length, 0, "Initial render saved choice");
          assertEquals(
            element("#release-notes-current-setting").textContent,
            "Current setting: Allow ads to support Floorp",
            "Confirmed current setting not displayed",
          );
        },
      });
    } else {
      tests.push({
        name:
          "Existing user starts with blocker settings preserved and a loaded logo",
        fn: async () => {
          assert(
            element<HTMLInputElement>(selector("blocking")).checked,
            "Unconfirmed support was applied",
          );
          assertEquals(
            document.querySelectorAll('input[type="radio"]').length,
            3,
            "Choices missing",
          );
          await until(
            () => [...document.images].some((img) => img.naturalWidth > 0),
            "Logo missing",
          );
          assertEquals(
            savedChoices().length,
            0,
            "Initial render wrote choices",
          );
          assert(
            document.querySelector('[role="note"]')?.textContent?.includes(
              "tracking protection",
            ),
            "Impact explanation missing",
          );
        },
      });
      tests.push({
        name: "All choices remain drafts until confirmed",
        fn: async () => {
          for (const mode of ["disabled", "blocking", "support"]) {
            await click(selector(mode));
            assert(
              element<HTMLInputElement>(selector(mode)).checked,
              `${mode} not selected`,
            );
            assertEquals(savedChoices().length, 0, "Draft wrote settings");
            assertEquals(
              element("#release-notes-current-setting").textContent,
              "Current setting: Open with my blocker settings",
              "Draft replaced the effective current setting",
            );
            assertEquals(
              prefs.get(CONFIRMED),
              false,
              "Draft confirmed preference",
            );
          }
        },
      });
      tests.push({
        name: "Not now closes without applying the draft",
        fn: async () => {
          await click(selector("disabled"));
          calls.length = 0;
          await button("Not now");
          assert(closed(), "Dismiss did not close");
          assertEquals(savedChoices().length, 0, "Dismiss saved draft");
          assertEquals(
            prefs.get(MODE),
            "support",
            "Dismiss changed stored value",
          );
          assertEquals(prefs.get(CONFIRMED), false, "Dismiss confirmed choice");
        },
      });
      tests.push({
        name: "Failed save retains selection and keeps prompt open for retry",
        fn: async () => {
          calls.length = 0;
          faults.write = MODE;
          try {
            await button("Save selection");
            await until(
              () => document.querySelector('[role="alert"]'),
              "Save error missing",
            );
            await pause();
            assert(!closed(), "Closed after failed save");
            assert(
              element<HTMLInputElement>(selector("disabled")).checked,
              "Failed draft lost",
            );
            assertEquals(
              prefs.get(CONFIRMED),
              false,
              "Failure confirmed choice",
            );
          } finally {
            faults.write = "";
          }
        },
      });
      tests.push({
        name: "Retry saves both preferences before closing",
        fn: async () => {
          await button("Save selection");
          await until(closed, "Successful save did not close");
          assertEquals(prefs.get(MODE), "disabled", "Choice not saved");
          assertEquals(prefs.get(CONFIRMED), true, "Choice not confirmed");
        },
      });
    }
  }
  await executeCases(tests);
}

export async function runAllTests() {
  const url =
    "http://localhost:5197/test/integration/index.html?releaseNotes=1";
  await runFixture(url);
  await runFixture(`${url}&confirmed=1`);
  await runFixture(`${url}&readFailure=1`);
}
