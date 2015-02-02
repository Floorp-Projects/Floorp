/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "d01a8103012800820281828d0f0449646c65204d6f64652054657874",
   expect: {commandQualifier: 0x00,
            text: "Idle Mode Text"}},
  {command: "d081fd8103012800820281828d81f100547419344d3641737498cd06cdeb70383b0f0a83e8653c1d34a7cbd3ee330b7447a7c768d01c1d66b341e232889c9ec3d9e17c990c12e741747419d42c82c27350d80d4a93d96550fb4d2e83e8653c1d943683e8e832a85904a5e7a0b0985d06d1df20f21b94a6bba8e832082e2fcfcb6e7a989e7ebb41737a9e5d06a5e72076d94c0785e7a0b01b946ec3d9e576d94d0fd3d36f37885c1ea7e7e9b71b447f83e8e832a85904b5c3eeba393ca6d7e565b90b444597416932bb0c6abfc96510bd8ca783e6e8309b0d129741e4f41cce0ee7cb6450da0d0a83da61b7bb2c07d1d1613aa8ec9ed7e5e539888e0ed341ee32",
   expect: {commandQualifier: 0x00,
            text: "The SIM shall supply a text string, which shall be displayed by the ME as an idle mode text if the ME is able to do it.The presentation style is left as an implementation decision to the ME manufacturer. The idle mode text shall be displayed in a manner that ensures that ne"}},
  {command: "d0198103012800820281828d0a0449646c6520746578749e020001",
   expect: {commandQualifier: 0x00,
            text: "Idle text",
            iconSelfExplanatory: true,
            icons: [BASIC_ICON]}},
  {command: "d0198103012800820281828d0a0449646c6520746578749e020007",
   expect: {commandQualifier: 0x00,
            text: "Idle text",
            iconSelfExplanatory: true,
            icons: [COLOR_ICON, COLOR_TRANSPARENCY_ICON]}},
  {command: "d0248103012800820281828d1908041704140420041004120421042204120423041904220415",
   expect: {commandQualifier: 0x00,
            text: "ЗДРАВСТВУЙТЕ"}},
  {command: "d0228103012800820281828d110449646c65204d6f646520546578742031d004001000b4",
   expect: {commandQualifier: 0x00,
            text: "Idle Mode Text 1"}},
  {command: "d0108103012800820281828d05084f60597d",
   expect: {commandQualifier: 0x00,
            text: "你好"}},
];

function testSetupIdleModeText(aCommand, aExpect) {
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SET_UP_IDLE_MODE_TEXT,
     "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");
  is(aCommand.options.text, aExpect.text, "options.text");

  if (aExpect.icons) {
    isIcons(aCommand.options.icons, aExpect.icons);
    is(aCommand.options.iconSelfExplanatory, aExpect.iconSelfExplanatory,
       "options.iconSelfExplanatory");
  }
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => {
      log("setup_idle_mode_text_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testSetupIdleModeText(aEvent.command, data.expect)));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
