/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_HEAD_JS = "stk_helper.js";

function testBipCommand(command, expect) {
  log("STK CMD " + JSON.stringify(command));

  is(command.typeOfCommand, expect.typeOfCommand, expect.name);
  is(command.options.text, expect.text, expect.name);

  let icons = command.options.icons;
  if (icons) {
    isIcons(icons, expect.icons, expect.name);

    let iconSelfExplanatory = command.options.iconSelfExplanatory;
    is(iconSelfExplanatory, expect.iconSelfExplanatory, expect.name);
  }

  runNextTest();
}

let tests = [
  {command: "d04f81030140018202818205074f70656e204944350702030403041f0239020578470a065465737447700272730d08f4557365724c6f670d08f4557365725077643c0301ad9c3e0521010101019e020007",
   func: testBipCommand,
   expect: {name: "open_channel_1",
            typeOfCommand: iccManager.STK_CMD_OPEN_CHANNEL,
            text: "Open ID",
            iconSelfExplanatory: true,
            icons: [colorIcon, colorTransparencyIcon]}},
  {command: "d0448103014001820281820500350702030403041f0239020578470a065465737447700272730d08f4557365724c6f670d08f4557365725077643c0301ad9c3e052101010101",
   func: testBipCommand,
   expect: {name: "open_channel_2",
            typeOfCommand: iccManager.STK_CMD_OPEN_CHANNEL,
            text: ""}},
  {command: "d05381030140018202818205094f70656e2049442031350702030403041f0239020578470a065465737447700272730d08f4557365724c6f670d08f4557365725077643c0301ad9c3e052101010101d004000900b4",
   func: testBipCommand,
   expect: {name: "open_channel_3",
            typeOfCommand: iccManager.STK_CMD_OPEN_CHANNEL,
            text: "Open ID 1"}},
  {command: "d01b810301410082028121850a436c6f73652049442031d004000a00b4",
   func: testBipCommand,
   expect: {name: "close_channel_1",
            typeOfCommand: iccManager.STK_CMD_CLOSE_CHANNEL,
            text: "Close ID 1"}},
  {command: "d022810301420082028121850e5265636569766520446174612031b701c8d004000e00b4",
   func: testBipCommand,
   expect: {name: "receive_data_1",
            typeOfCommand: iccManager.STK_CMD_RECEIVE_DATA,
            text: "Receive Data 1"}},
  {command: "d026810301430182028121850b53656e6420446174612031b6080001020304050607d004000b00b4",
   func: testBipCommand,
   expect: {name: "send_data_1",
            typeOfCommand: iccManager.STK_CMD_SEND_DATA,
            text: "Send Data 1"}},
];

runNextTest();
