// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { CommandPaletteController } from "../controller.ts";
import type {
  CommandStep,
  PaletteCommand,
} from "#features-chrome/common/command-palette/types.ts";

function makeStepCommand(
  steps: CommandStep[],
  fn?: PaletteCommand["fn"],
): PaletteCommand {
  return {
    id: "__test-step-command__",
    label: "Test Command",
    description: "Test command for controller",
    category: "test",
    keywords: [],
    fn: fn ?? ((_win: Window, _args?: Record<string, string>) => {}),
    steps,
  };
}

const STEP_COMMAND_NO_VALIDATE_2: PaletteCommand = makeStepCommand(
  [
    { id: "step1", label: "Step 1", placeholder: "Enter step 1" },
    { id: "step2", label: "Step 2", placeholder: "Enter step 2" },
  ],
);

const STEP_COMMAND_WITH_VALIDATE: PaletteCommand = makeStepCommand([
  {
    id: "validated",
    label: "Validated",
    placeholder: "Enter value",
    validate: (input: string): boolean | string =>
      input.trim() ? true : "Input required",
  },
]);

const STEP_COMMAND_WITH_CHOICES: PaletteCommand = makeStepCommand([
  {
    id: "choiceStep",
    label: "Choose",
    placeholder: "Pick one",
    choices: [
      { label: "Alpha", value: "a" },
      { label: "Beta", value: "b" },
      { label: "Gamma", value: "g" },
    ],
  },
]);

const STEP_COMMAND_WITH_LOADER: PaletteCommand = makeStepCommand([
  {
    id: "loaderStep",
    label: "Load",
    placeholder: "Loading...",
    choicesLoader: () =>
      Promise.resolve([
        { label: "Loaded A", value: "la" },
        { label: "Loaded B", value: "lb" },
      ]),
  },
]);

let capturedArgs: Record<string, string> | undefined;
const STEP_COMMAND_CAPTURE_ARGS: PaletteCommand = makeStepCommand(
  [
    { id: "input", label: "Input", placeholder: "Type" },
  ],
  (_win: Window, args?: Record<string, string>) => {
    capturedArgs = args;
  },
);

function createController(): CommandPaletteController {
  return new CommandPaletteController(window);
}

function createDeferred<T>(): {
  promise: Promise<T>;
  resolve: (value: T) => void;
} {
  let resolve!: (value: T) => void;
  const promise = new Promise<T>((resolvePromise) => {
    resolve = resolvePromise;
  });
  return { promise, resolve };
}

const rawTests: TestCase[] = [
  // --- Controller instantiation ---
  {
    name: "controller constructs with window",
    fn() {
      const ctrl = createController();
      assert(ctrl !== null, "controller should be created");
      assertEquals(
        ctrl.state.mode(),
        "command",
        "initial mode should be command",
      );
      assertEquals(
        ctrl.state.isVisible(),
        false,
        "initial visibility should be false",
      );
    },
  },

  // --- enterInputMode via executeCommand ---
  {
    name: "executeCommand with steps enters input mode",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_NO_VALIDATE_2);
      assertEquals(ctrl.state.mode(), "input", "mode should be input");
      assertEquals(
        ctrl.state.activeCommand()?.id,
        "__test-step-command__",
        "active command should be set",
      );
      assertEquals(ctrl.state.currentStepIndex(), 0, "step index should be 0");
      assertEquals(
        Object.keys(ctrl.state.stepInputs()).length,
        0,
        "step inputs should be empty",
      );
      assertEquals(ctrl.state.stepError(), null, "step error should be null");
    },
  },

  // --- advanceStep progression ---
  {
    name: "advanceStep progresses through steps",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_NO_VALIDATE_2);
      ctrl.updateSearch("value1");
      ctrl.advanceStep();
      assertEquals(
        ctrl.state.currentStepIndex(),
        1,
        "should advance to step 1",
      );
      assertEquals(
        ctrl.state.stepInputs().step1,
        "value1",
        "step1 input should be saved",
      );
    },
  },
  {
    name: "advanceStep at last step executes fn with collected args",
    fn() {
      capturedArgs = undefined;
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_CAPTURE_ARGS);
      ctrl.updateSearch("final value");
      ctrl.advanceStep();
      assertEquals(
        (capturedArgs as unknown as Record<string, string>)?.input,
        "final value",
        "fn should receive collected args",
      );
    },
  },

  // --- advanceStep with validation ---
  {
    name: "advanceStep with validation failure sets error and stays",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_VALIDATE);
      ctrl.updateSearch("   ");
      ctrl.advanceStep();
      assertEquals(
        typeof ctrl.state.stepError(),
        "string",
        "should have error string",
      );
      assertEquals(ctrl.state.currentStepIndex(), 0, "should stay on step 0");
    },
  },
  {
    name: "advanceStep with validation pass clears error",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_VALIDATE);
      ctrl.updateSearch("   ");
      ctrl.advanceStep();
      assert(
        ctrl.state.stepError() !== null,
        "should have error after empty input",
      );
      ctrl.updateSearch("valid");
      ctrl.advanceStep();
      assertEquals(
        ctrl.state.stepError(),
        null,
        "error should be cleared after valid input",
      );
    },
  },

  // --- goBackStep ---
  {
    name: "goBackStep returns to previous step",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_NO_VALIDATE_2);
      ctrl.updateSearch("value1");
      ctrl.advanceStep();
      assertEquals(ctrl.state.currentStepIndex(), 1, "should be on step 1");
      ctrl.goBackStep();
      assertEquals(ctrl.state.currentStepIndex(), 0, "should return to step 0");
    },
  },
  {
    name: "goBackStep at step 0 exits to command mode",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_NO_VALIDATE_2);
      assertEquals(ctrl.state.mode(), "input", "should be in input mode");
      ctrl.goBackStep();
      assertEquals(
        ctrl.state.mode(),
        "command",
        "should return to command mode",
      );
      assertEquals(
        ctrl.state.activeCommand(),
        null,
        "active command should be null",
      );
    },
  },

  // --- loadStepChoices with static choices ---
  {
    name: "enterInputMode with choices populates filteredStepChoices",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_CHOICES);
      assertEquals(
        ctrl.state.filteredStepChoices().length,
        3,
        "should have 3 choices",
      );
      assertEquals(
        ctrl.state.filteredStepChoices()[0].value,
        "a",
        "first choice should be Alpha",
      );
    },
  },

  // --- loadStepChoices with choicesLoader ---
  {
    name: "enterInputMode with choicesLoader loads async choices",
    async fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_LOADER);
      assertEquals(ctrl.state.stepChoicesLoading(), true, "should be loading");
      // Wait for choicesLoader promise to resolve
      await new Promise((resolve) => setTimeout(resolve, 100));
      assertEquals(
        ctrl.state.stepChoicesLoading(),
        false,
        "loading should be done",
      );
      assertEquals(
        ctrl.state.filteredStepChoices().length,
        2,
        "should have 2 loaded choices",
      );
      assertEquals(
        ctrl.state.filteredStepChoices()[0].value,
        "la",
        "first loaded choice value",
      );
    },
  },
  {
    name: "choicesLoader receives target window and saved inputs",
    async fn() {
      let loaderWindow: Window | undefined;
      let loaderArgs: Readonly<Record<string, string>> | undefined;
      const command = makeStepCommand([
        { id: "context", label: "Context", placeholder: "Type context" },
        {
          id: "loaded",
          label: "Loaded",
          placeholder: "Pick one",
          choicesLoader: (win, args) => {
            loaderWindow = win;
            loaderArgs = args;
            return Promise.resolve([{ label: "Choice", value: "choice" }]);
          },
        },
      ]);
      const ctrl = createController();

      ctrl.executeCommand(command);
      ctrl.updateSearch("workspace-a");
      ctrl.advanceStep();

      assertEquals(
        loaderWindow,
        window,
        "loader should receive controller target window",
      );
      assertEquals(
        loaderArgs?.context,
        "workspace-a",
        "loader should receive saved inputs",
      );
      await Promise.resolve();
      assertEquals(
        ctrl.state.filteredStepChoices()[0]?.value,
        "choice",
        "loader result should apply",
      );
    },
  },

  // --- conditional step traversal ---
  {
    name:
      "conditional steps are skipped forward and backward with input restored",
    fn() {
      let predicateWindow: Window | undefined;
      const command = makeStepCommand([
        {
          id: "destination",
          label: "Destination",
          placeholder: "Type destination",
        },
        {
          id: "container",
          label: "Container",
          placeholder: "Type container",
          shouldInclude: (args, win) => {
            predicateWindow = win;
            return args.destination !== "current-tab";
          },
        },
        { id: "url", label: "URL", placeholder: "Type URL" },
      ]);
      const ctrl = createController();

      ctrl.executeCommand(command);
      ctrl.updateSearch("current-tab");
      ctrl.advanceStep();

      assertEquals(
        ctrl.state.currentStepIndex(),
        2,
        "forward traversal should skip container",
      );
      assertEquals(
        predicateWindow,
        window,
        "predicate should receive controller target window",
      );
      assertEquals(
        ctrl.state.stepInputs().container,
        undefined,
        "skipped step should not have input",
      );
      const progress = ctrl.getStepProgress();
      assertEquals(
        progress.current,
        2,
        "URL should be the second visible step",
      );
      assertEquals(progress.total, 2, "only two steps should be visible");

      ctrl.goBackStep();
      assertEquals(
        ctrl.state.currentStepIndex(),
        0,
        "back traversal should skip container",
      );
      assertEquals(
        ctrl.state.query(),
        "current-tab",
        "previous input should be restored",
      );
    },
  },
  {
    name: "inputs from newly excluded steps are pruned before execution",
    fn() {
      let executedArgs: Record<string, string> | undefined;
      const command = makeStepCommand(
        [
          {
            id: "destination",
            label: "Destination",
            placeholder: "Type destination",
          },
          {
            id: "container",
            label: "Container",
            placeholder: "Type container",
            shouldInclude: (args) => args.destination !== "current-tab",
          },
          { id: "url", label: "URL", placeholder: "Type URL" },
        ],
        (_win, args) => {
          executedArgs = args;
        },
      );
      const ctrl = createController();

      ctrl.executeCommand(command);
      ctrl.updateSearch("new-tab");
      ctrl.advanceStep();
      ctrl.updateSearch("3");
      ctrl.advanceStep();
      ctrl.goBackStep();
      ctrl.goBackStep();

      ctrl.updateSearch("current-tab");
      ctrl.advanceStep();
      assertEquals(
        ctrl.state.currentStepIndex(),
        2,
        "container should now be skipped",
      );
      assertEquals(
        ctrl.state.stepInputs().container,
        undefined,
        "stale container input should be pruned",
      );

      ctrl.updateSearch("https://example.com");
      ctrl.advanceStep();
      assertEquals(
        executedArgs?.container,
        undefined,
        "excluded input must not reach command fn",
      );
      assertEquals(
        executedArgs?.destination,
        "current-tab",
        "updated destination should execute",
      );
    },
  },
  {
    name:
      "stale choices loader cannot overwrite another command at the same step index",
    async fn() {
      const stale = createDeferred<Array<{ label: string; value: string }>>();
      const staleCommand = makeStepCommand([
        {
          id: "shared-step-id",
          label: "Stale",
          placeholder: "Wait",
          choicesLoader: () => stale.promise,
        },
      ]);
      const currentCommand = makeStepCommand([
        {
          id: "shared-step-id",
          label: "Current",
          placeholder: "Wait",
          choicesLoader: () =>
            Promise.resolve([{ label: "Current", value: "current" }]),
        },
      ]);
      const ctrl = createController();

      ctrl.executeCommand(staleCommand);
      ctrl.executeCommand(currentCommand);
      await Promise.resolve();
      assertEquals(
        ctrl.state.filteredStepChoices()[0]?.value,
        "current",
        "current loader should apply",
      );

      stale.resolve([{ label: "Stale", value: "stale" }]);
      await Promise.resolve();
      assertEquals(
        ctrl.state.filteredStepChoices()[0]?.value,
        "current",
        "stale loader should be ignored",
      );
    },
  },

  // --- updateStepChoices filtering ---
  {
    name: "updateSearch filters choices in input mode",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_CHOICES);
      assertEquals(
        ctrl.state.filteredStepChoices().length,
        3,
        "should start with 3 choices",
      );
      ctrl.updateSearch("alp");
      assertEquals(
        ctrl.state.filteredStepChoices().length,
        1,
        "should filter to 1 choice",
      );
      assertEquals(
        ctrl.state.filteredStepChoices()[0].value,
        "a",
        "filtered choice should be Alpha",
      );
    },
  },
  {
    name: "empty query restores all choices",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_CHOICES);
      ctrl.updateSearch("alp");
      assertEquals(
        ctrl.state.filteredStepChoices().length,
        1,
        "should be filtered",
      );
      ctrl.updateSearch("");
      assertEquals(
        ctrl.state.filteredStepChoices().length,
        3,
        "should restore all choices",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("commandPaletteController.test.ts", rawTests);
}
