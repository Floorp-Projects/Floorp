/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_HEAD_JS = "stk_helper.js";

function testDisplayText(command, expect) {
  log("STK CMD " + JSON.stringify(command));
  is(command.typeOfCommand, iccManager.STK_CMD_DISPLAY_TEXT, expect.name);
  is(command.options.text, expect.text, expect.name);
  is(command.commandQualifier, expect.commandQualifier, expect.name);
  is(command.options.userClear, expect.userClear, expect.name);
  is(command.options.isHighPriority, expect.isHighPriority, expect.name);

  let duration = command.options.duration;
  if (duration) {
    is(duration.timeUnit, expect.duration.timeUnit, expect.name);
    is(duration.timeInterval, expect.duration.timeInterval, expect.name);
  }

  let icons = command.options.icons;
  if (icons) {
    isIcons(icons, expect.icons, expect.name);

    let iconSelfExplanatory = command.options.iconSelfExplanatory;
    is(iconSelfExplanatory, expect.iconSelfExplanatory, expect.name);
  }

  runNextTest();
}

let tests = [
  {command: "d01a8103012180820281028d0f04546f6f6c6b697420546573742031",
   func: testDisplayText,
   expect: {name: "display_text_cmd_1",
            commandQualifier: 0x80,
            text: "Toolkit Test 1",
            userClear: true}},
  {command: "d01a8103012181820281028d0f04546f6f6c6b697420546573742032",
   func: testDisplayText,
   expect: {name: "display_text_cmd_2",
            commandQualifier: 0x81,
            text: "Toolkit Test 2",
            isHighPriority: true,
            userClear: true}},
  {command: "d0198103012180820281028d0e00d4f79bbd4ed341d4f29c0e9a01",
   func: testDisplayText,
   expect: {name: "display_text_cmd_3",
            commandQualifier: 0x80,
            text: "Toolkit Test 3",
            userClear: true}},
  {command: "d01a8103012100820281028d0f04546f6f6c6b697420546573742034",
   func: testDisplayText,
   expect: {name: "display_text_cmd_4",
            commandQualifier: 0x00,
            text: "Toolkit Test 4"}},
  {command: "d081ad8103012180820281028d81a1045468697320636f6d6d616e6420696e7374727563747320746865204d4520746f20646973706c617920612074657874206d6573736167652e20497420616c6c6f7773207468652053494d20746f20646566696e6520746865207072696f72697479206f662074686174206d6573736167652c20616e6420746865207465787420737472696e6720666f726d61742e2054776f207479706573206f66207072696f",
   func: testDisplayText,
   expect: {name: "display_text_cmd_5",
            commandQualifier: 0x80,
            text: "This command instructs the ME to display a text message. It allows the SIM to define the priority of that message, and the text string format. Two types of prio",
            userClear: true}},
  {command: "d01a8103012180820281028d0f043c474f2d4241434b57415244533e",
   func: testDisplayText,
   expect: {name: "display_text_cmd_6",
            commandQualifier: 0x80,
            text: "<GO-BACKWARDS>",
            userClear: true}},
  {command: "d0248103012180820281028d1908041704140420041004120421042204120423041904220415",
   func: testDisplayText,
   expect: {name: "display_text_cmd_7",
            commandQualifier: 0x80,
            text: "ЗДРАВСТВУЙТЕ",
            userClear: true}},
  {command: "d0108103012180820281028d05084f60597d",
   func: testDisplayText,
   expect: {name: "display_text_cmd_8",
            commandQualifier: 0x80,
            text: "你好",
            userClear: true}},
  {command: "d0128103012180820281028d07080038003030eb",
   func: testDisplayText,
   expect: {name: "display_text_cmd_9",
            commandQualifier: 0x80,
            text: "80ル",
            userClear: true}},
  {command: "d0288103012180820281020d1d00d3309bfc06c95c301aa8e80259c3ec34b9ac07c9602f58ed159bb940",
   func: testDisplayText,
   expect: {name: "display_text_cmd_10",
            commandQualifier: 0x80,
            text: "Saldo 2.04 E. Validez 20/05/13. ",
            userClear: true}},
  {command: "d0198103012180820281028D0A043130205365636F6E648402010A",
   func: testDisplayText,
   expect: {name: "display_text_cmd_11",
            commandQualifier: 0x80,
            text: "10 Second",
            userClear: true,
            duration: {timeUnit: iccManager.STK_TIME_UNIT_SECOND,
                       timeInterval: 0x0A}}},
  {command: "d01a8103012180820281028d0b0442617369632049636f6e9e020001",
   func: testDisplayText,
   expect: {name: "display_text_cmd_12",
            commandQualifier: 0x80,
            text: "Basic Icon",
            userClear: true,
            iconSelfExplanatory: true,
            icons: [basicIcon]}},
  {command: "D026810301210082028102" +
            "8D" +
            "1B" +
            "00" + // 7BIT
            "D4F79BBD4ED341D4F29C0E3A4A9F55A8" +
            "0E8687C158A09B304905",
   func: testDisplayText,
   expect: {name: "display_text_cmd_13",
            commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0x00, 7BIT"}},
  {command: "D029810301210082028102" +
            "8D" +
            "1E" +
            "04" + // 8BIT
            "546F6F6C6B697420546573742047524F" +
            "55503A307830302C2038424954",
   func: testDisplayText,
   expect: {name: "display_text_cmd_14",
            commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0x00, 8BIT"}},
  {command: "D046810301210082028102" +
            "8D" +
            "3B" +
            "08" + // UCS2
            "0054006F006F006C006B006900740020" +
            "0054006500730074002000470052004F" +
            "00550050003A0030007800300030002C" +
            "00200055004300530032",
   func: testDisplayText,
   expect: {name: "display_text_cmd_15",
            commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0x00, UCS2"}},
  {command: "D026810301210082028102" +
            "8D" +
            "1B" +
            "12" + // 7BIT + Class 2
            "D4F79BBD4ED341D4F29C0E3A4A9F55A8" +
            "0E868FC158A09B304905",
   func: testDisplayText,
   expect: {name: "display_text_cmd_16",
            commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0x10, 7BIT"}},
  {command: "D029810301210082028102" +
            "8D" +
            "1E" +
            "16" + // 8BIT + Class 2
            "546F6F6C6B697420546573742047524F" +
            "55503A307831302C2038424954",
   func: testDisplayText,
   expect: {name: "display_text_cmd_17",
            commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0x10, 8BIT"}},
  {command: "D046810301210082028102" +
            "8D" +
            "3B" +
            "1A" + // UCS2 + Class 2
            "0054006F006F006C006B006900740020" +
            "0054006500730074002000470052004F" +
            "00550050003A0030007800310030002C" +
            "00200055004300530032",
   func: testDisplayText,
   expect: {name: "display_text_cmd_18",
            commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0x10, UCS2"}},
  {command: "D026810301210082028102" +
            "8D" +
            "1B" +
            "F2" + // 7BIT + Class 2
            "D4F79BBD4ED341D4F29C0E3A4A9F55A8" +
            "0E8637C258A09B304905",
   func: testDisplayText,
   expect: {name: "display_text_cmd_19",
            commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0xF0, 7BIT"}},
  {command: "D029810301210082028102" +
            "8D" +
            "1E" +
            "F6" + // 8BIT + Class 2
            "546F6F6C6B697420546573742047524F" +
            "55503A307846302C2038424954",
   func: testDisplayText,
   expect: {name: "display_text_cmd_20",
            commandQualifier: 0x00,
            text: "Toolkit Test GROUP:0xF0, 8BIT"}},
];

runNextTest();
