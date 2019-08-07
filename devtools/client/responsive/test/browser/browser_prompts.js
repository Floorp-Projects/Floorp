/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that alert, confirm and prompt are supported in RDM.

const TEST_URL = `data:text/html;charset=utf-8,
  <CENTER>
    <font face='comic sans ms' size='7'>ALERTS!</font>
  </CENTER>`;

addRDMTask(TEST_URL, async function({ ui }) {
  const { toolWindow } = ui;
  const { store } = toolWindow;
  await waitUntilState(store, state => state.viewports.length == 1);

  // Alerts coming from inside the viewport are relayed by RDM to the outer browser.
  // So, in order to test that those relayed prompts occur, let's override the usual
  // alert, prompt and confirm methods there to test that they get called.
  const {
    alert: oldAlert,
    prompt: oldPrompt,
    confirm: oldConfirm,
  } = toolWindow;

  info("Listen for calls on alert, prompt and confirm");
  const prompts = [];
  toolWindow.alert = message => {
    prompts.push({
      promptType: "alert",
      message,
    });
  };
  toolWindow.prompt = (message, initialValue) => {
    prompts.push({
      promptType: "prompt",
      message,
      initialValue,
    });
  };
  toolWindow.confirm = message => {
    prompts.push({
      promptType: "confirm",
      message,
    });
  };

  info("Trigger a few prompts from inside the viewport");
  await spawnViewportTask(ui, {}, function() {
    content.alert("Some simple alert");
    content.prompt("Some simple prompt");
    content.prompt("Some simple prompt with initial value", "initial value");
    content.confirm("Some simple confirm");
  });

  is(prompts.length, 4, "The right number of prompts was detected");

  is(prompts[0].promptType, "alert", "Prompt 1 has the right type");
  is(prompts[0].message, "Some simple alert", "Prompt 1 has the right message");
  ok(!prompts[0].initialValue, "Prompt 1 has the right initialValue");

  is(prompts[1].promptType, "prompt", "Prompt 2 has the right type");
  is(
    prompts[1].message,
    "Some simple prompt",
    "Prompt 2 has the right message"
  );
  ok(!prompts[1].initialValue, "Prompt 2 has the right initialValue");

  is(prompts[2].promptType, "prompt", "Prompt 3 has the right type");
  is(
    prompts[2].message,
    "Some simple prompt with initial value",
    "Prompt 3 has the right message"
  );
  is(
    prompts[2].initialValue,
    "initial value",
    "Prompt 3 has the right initialValue"
  );

  is(prompts[3].promptType, "confirm", "Prompt 4 has the right type");
  is(
    prompts[3].message,
    "Some simple confirm",
    "Prompt 4 has the right message"
  );
  ok(!prompts[3].initialValue, "Prompt 4 has the right initialValue");

  // Revert the old versions of alert, prompt and confirm.
  toolWindow.alert = oldAlert;
  toolWindow.prompt = oldPrompt;
  toolWindow.confirm = oldConfirm;
});
