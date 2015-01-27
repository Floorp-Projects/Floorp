/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "d03d810301240082028182850e546f6f6c6b69742053656c6563748f07014974656d20318f07024974656d20328f07034974656d20338f07044974656d2034",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}, {identifier: 3, text: "Item 3"}, {identifier: 4, text: "Item 4"}]}},
  {command: "d081fc810301240082028182850a4c617267654d656e75318f05505a65726f8f044f4f6e658f044e54776f8f064d54687265658f054c466f75728f054b466976658f044a5369788f0649536576656e8f064845696768748f05474e696e658f0646416c7068618f0645427261766f8f0844436861726c69658f064344656c74618f05424563686f8f0941466f782d74726f748f0640426c61636b8f063f42726f776e8f043e5265648f073d4f72616e67658f073c59656c6c6f778f063b477265656e8f053a426c75658f073956696f6c65748f0538477265798f063757686974658f06366d696c6c698f06356d6963726f8f05346e616e6f8f05337069636f",
   expect: {commandQualifier: 0x00,
            title: "LargeMenu1",
            items: [{identifier: 80, text: "Zero"}, {identifier: 79, text: "One"}, {identifier: 78, text: "Two"}, {identifier: 77, text: "Three"}, {identifier: 76, text: "Four"}, {identifier: 75, text: "Five"}, {identifier: 74, text: "Six"}, {identifier: 73, text: "Seven"}, {identifier: 72, text: "Eight"}, {identifier: 71, text: "Nine"}, {identifier: 70, text: "Alpha"}, {identifier: 69, text: "Bravo"}, {identifier: 68, text: "Charlie"}, {identifier: 67, text: "Delta"}, {identifier: 66, text: "Echo"}, {identifier: 65, text: "Fox-trot"}, {identifier: 64, text: "Black"}, {identifier: 63, text: "Brown"}, {identifier: 62, text: "Red"}, {identifier: 61, text: "Orange"}, {identifier: 60, text: "Yellow"}, {identifier: 59, text: "Green"}, {identifier: 58, text: "Blue"}, {identifier: 57, text: "Violet"}, {identifier: 56, text: "Grey"}, {identifier: 55, text: "White"}, {identifier: 54, text: "milli"}, {identifier: 53, text: "micro"}, {identifier: 52, text: "nano"}, {identifier: 51, text: "pico"}]}},
  {command: "d081fb810301240082028182850a4c617267654d656e75328f1eff43616c6c20466f7277617264696e6720556e636f6e646974696f6e616c8f1dfe43616c6c20466f7277617264696e67204f6e205573657220427573798f1cfd43616c6c20466f7277617264696e67204f6e204e6f205265706c798f26fc43616c6c20466f7277617264696e67204f6e2055736572204e6f7420526561636861626c658f1efb42617272696e67204f6620416c6c204f7574676f696e672043616c6c738f2cfa42617272696e67204f6620416c6c204f7574676f696e6720496e7465726e6174696f6e616c2043616c6c738f11f9434c492050726573656e746174696f6e",
   expect: {commandQualifier: 0x00,
            title: "LargeMenu2",
            items: [{identifier: 255, text: "Call Forwarding Unconditional"}, {identifier: 254, text: "Call Forwarding On User Busy"}, {identifier: 253, text: "Call Forwarding On No Reply"}, {identifier: 252, text: "Call Forwarding On User Not Reachable"}, {identifier: 251, text: "Barring Of All Outgoing Calls"}, {identifier: 250, text: "Barring Of All Outgoing International Calls"}, {identifier: 249, text: "CLI Presentation"}]}},
  {command: "d022810301240082028182850b53656c656374204974656d8f04114f6e658f041254776f",
   expect: {commandQualifier: 0x00,
            title: "Select Item",
            items: [{identifier: 17, text: "One"}, {identifier: 18, text: "Two"}]}},
  {command: "d081fd8103012400820281828581ed5468652053494d207368616c6c20737570706c79206120736574206f66206974656d732066726f6d207768696368207468652075736572206d61792063686f6f7365206f6e652e2045616368206974656d20636f6d70726973657320612073686f7274206964656e74696669657220287573656420746f20696e646963617465207468652073656c656374696f6e2920616e642061207465787420737472696e672e204f7074696f6e616c6c79207468652053494d206d617920696e636c75646520616e20616c706861206964656e7469666965722e2054686520616c706861206964656e74696669657220698f020159",
   expect: {commandQualifier: 0x00,
            title: "The SIM shall supply a set of items from which the user may choose one. Each item comprises a short identifier (used to indicate the selection) and a text string. Optionally the SIM may include an alpha identifier. The alpha identifier i",
            items: [{identifier: 1, text: "Y"}]}},
  {command: "d081f3810301240082028182850a304c617267654d656e758f1dff312043616c6c20466f727761726420556e636f6e646974696f6e616c8f1cfe322043616c6c20466f7277617264204f6e205573657220427573798f1bfd332043616c6c20466f7277617264204f6e204e6f205265706c798f25fc342043616c6c20466f7277617264204f6e2055736572204e6f7420526561636861626c658f20fb352042617272696e67204f6620416c6c204f7574676f696e672043616c6c738f24fa362042617272696e67204f6620416c6c204f7574676f696e6720496e742043616c6c738f13f93720434c492050726573656e746174696f6e",
   expect: {commandQualifier: 0x00,
            title: "0LargeMenu",
            items: [{identifier: 255, text: "1 Call Forward Unconditional"}, {identifier: 254, text: "2 Call Forward On User Busy"}, {identifier: 253, text: "3 Call Forward On No Reply"}, {identifier: 252, text: "4 Call Forward On User Not Reachable"}, {identifier: 251, text: "5 Barring Of All Outgoing Calls"}, {identifier: 250, text: "6 Barring Of All Outgoing Int Calls"}, {identifier: 249, text: "7 CLI Presentation"}]}},
  {command: "d039810301240082028182850e546f6f6c6b69742053656c6563748f07014974656d20318f07024974656d20328f07034974656d20331803131026",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}, {identifier: 3, text: "Item 3"}],
            nextActionList: [MozIccManager.STK_CMD_SEND_SMS, MozIccManager.STK_CMD_SET_UP_CALL, MozIccManager.STK_CMD_PROVIDE_LOCAL_INFO]}},
  {command: "d037810301240082028182850e546f6f6c6b69742053656c6563748f07014974656d20318f07024974656d20328f07034974656d2033900102",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}, {identifier: 3, text: "Item 3"}]}},
  {command: "d034810301248082028182850e546f6f6c6b69742053656c6563748f07014974656d20318f07024974656d20328f07034974656d2033",
   expect: {commandQualifier: 0x80,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}, {identifier: 3, text: "Item 3"}]}},
  {command: "d03e810301240082028182850e546f6f6c6b69742053656c6563748f07014974656d20318f07024974656d20328f07034974656d20339e0201019f0401030303",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select",
            iconSelfExplanatory: false,
            icons: [BASIC_ICON],
            items: [{identifier: 1, text: "Item 1", iconSelfExplanatory: false, icons: [COLOR_ICON]},
                    {identifier: 2, text: "Item 2", iconSelfExplanatory: false, icons: [COLOR_ICON]},
                    {identifier: 3, text: "Item 3", iconSelfExplanatory: false, icons: [COLOR_ICON]}]}},
  {command: "d03e810301240082028182850e546f6f6c6b69742053656c6563748f07014974656d20318f07024974656d20328f07034974656d20339e0200019f0400050505",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select",
            iconSelfExplanatory: true,
            icons: [BASIC_ICON],
            items: [{identifier: 1, text: "Item 1", iconSelfExplanatory: true, icons: [COLOR_TRANSPARENCY_ICON]},
                    {identifier: 2, text: "Item 2", iconSelfExplanatory: true, icons: [COLOR_TRANSPARENCY_ICON]},
                    {identifier: 3, text: "Item 3", iconSelfExplanatory: true, icons: [COLOR_TRANSPARENCY_ICON]}]}},
  {command: "d034810301240382028182850e546f6f6c6b69742053656c6563748f07014974656d20318f07024974656d20328f07034974656d2033",
   expect: {commandQualifier: 0x03,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}, {identifier: 3, text: "Item 3"}]}},
  {command: "d034810301240182028182850e546f6f6c6b69742053656c6563748f07014974656d20318f07024974656d20328f07034974656d2033",
   expect: {commandQualifier: 0x01,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}, {identifier: 3, text: "Item 3"}]}},
  {command: "d02b810301240482028182850e546f6f6c6b69742053656c6563748f07014974656d20318f07024974656d2032",
   expect: {commandQualifier: 0x04,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}]}},
  {command: "d030810301240082028182850a3c54494d452d4f55543e8f07014974656d20318f07024974656d20328f07034974656d2033",
   expect: {commandQualifier: 0x00,
            title: "<TIME-OUT>",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}, {identifier: 3, text: "Item 3"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420318f07014974656d20318f07024974656d2032d004001000b4d108000600b4000600b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 1",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}]}},
  {command: "d02d8103012400820281828510546f6f6c6b69742053656c65637420328f07014974656d20338f07024974656d2034",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 2",
            items: [{identifier: 1, text: "Item 3"}, {identifier: 2, text: "Item 4"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420318f07014974656d20318f07024974656d2032d004001001b4d108000601b4000601b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 1",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}]}},
  {command: "d02d8103012400820281828510546f6f6c6b69742053656c65637420328f07014974656d20338f07024974656d2034",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 2",
            items: [{identifier: 1, text: "Item 3"}, {identifier: 2, text: "Item 4"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420318f07014974656d20318f07024974656d2032d004001002b4d108000602b4000602b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 1",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}]}},
  {command: "d02d8103012400820281828510546f6f6c6b69742053656c65637420328f07014974656d20338f07024974656d2034",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 2",
            items: [{identifier: 1, text: "Item 3"}, {identifier: 2, text: "Item 4"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420318f07014974656d20318f07024974656d2032d004001004b4d108000604b4000604b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 1",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420328f07014974656d20338f07024974656d2034d004001000b4d108000600b4000600b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 2",
            items: [{identifier: 1, text: "Item 3"}, {identifier: 2, text: "Item 4"}]}},
  {command: "d02d8103012400820281828510546f6f6c6b69742053656c65637420338f07014974656d20358f07024974656d2036",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 3",
            items: [{identifier: 1, text: "Item 5"}, {identifier: 2, text: "Item 6"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420318f07014974656d20318f07024974656d2032d004001008b4d108000608b4000608b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 1",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420328f07014974656d20338f07024974656d2034d004001000b4d108000600b4000600b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 2",
            items: [{identifier: 1, text: "Item 3"}, {identifier: 2, text: "Item 4"}]}},
  {command: "d02d8103012400820281828510546f6f6c6b69742053656c65637420338f07014974656d20358f07024974656d2036",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 3",
            items: [{identifier: 1, text: "Item 5"}, {identifier: 2, text: "Item 6"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420318f07014974656d20318f07024974656d2032d004001010b4d108000610b4000610b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 1",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420328f07014974656d20338f07024974656d2034d004001000b4d108000600b4000600b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 2",
            items: [{identifier: 1, text: "Item 3"}, {identifier: 2, text: "Item 4"}]}},
  {command: "d02d8103012400820281828510546f6f6c6b69742053656c65637420338f07014974656d20358f07024974656d2036",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 3",
            items: [{identifier: 1, text: "Item 5"}, {identifier: 2, text: "Item 6"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420318f07014974656d20318f07024974656d2032d004001020b4d108000620b4000620b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 1",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420328f07014974656d20338f07024974656d2034d004001000b4d108000600b4000600b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 2",
            items: [{identifier: 1, text: "Item 3"}, {identifier: 2, text: "Item 4"}]}},
  {command: "d02d8103012400820281828510546f6f6c6b69742053656c65637420338f07014974656d20358f07024974656d2036",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 3",
            items: [{identifier: 1, text: "Item 5"}, {identifier: 2, text: "Item 6"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420318f07014974656d20318f07024974656d2032d004001040b4d108000640b4000640b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 1",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420328f07014974656d20338f07024974656d2034d004001000b4d108000600b4000600b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 2",
            items: [{identifier: 1, text: "Item 3"}, {identifier: 2, text: "Item 4"}]}},
  {command: "d02d8103012400820281828510546f6f6c6b69742053656c65637420338f07014974656d20358f07024974656d2036",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 3",
            items: [{identifier: 1, text: "Item 5"}, {identifier: 2, text: "Item 6"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420318f07014974656d20318f07024974656d2032d004001080b4d108000680b4000680b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 1",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420328f07014974656d20338f07024974656d2034d004001000b4d108000600b4000600b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 2",
            items: [{identifier: 1, text: "Item 3"}, {identifier: 2, text: "Item 4"}]}},
  {command: "d02d8103012400820281828510546f6f6c6b69742053656c65637420338f07014974656d20358f07024974656d2036",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 3",
            items: [{identifier: 1, text: "Item 5"}, {identifier: 2, text: "Item 6"}]}},
  {command: "d03d8103012400820281828510546f6f6c6b69742053656c65637420318f07014974656d20318f07024974656d2032d004001000b4d108000600b4000600b4",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 1",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}]}},
  {command: "d02d8103012400820281828510546f6f6c6b69742053656c65637420328f07014974656d20338f07024974656d2034",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 2",
            items: [{identifier: 1, text: "Item 3"}, {identifier: 2, text: "Item 4"}]}},
  {command: "d07e8103012400820281828519800417041404200410041204210422041204230419042204158f1c018004170414042004100412042104220412042304190422041500318f1c028004170414042004100412042104220412042304190422041500328f1c03800417041404200410041204210422041204230419042204150033",
   expect: {commandQualifier: 0x00,
            title: "ЗДРАВСТВУЙТЕ",
            items: [{identifier: 1, text: "ЗДРАВСТВУЙТЕ1"}, {identifier: 2, text: "ЗДРАВСТВУЙТЕ2"}, {identifier: 3, text: "ЗДРАВСТВУЙТЕ3"}]}},
  {command: "d053810301240082028182850f810c089794a09092a1a292a399a2958f1101810d089794a09092a1a292a399a295318f1102810d089794a09092a1a292a399a295328f1103810d089794a09092a1a292a399a29533",
   expect: {commandQualifier: 0x00,
            title: "ЗДРАВСТВУЙТЕ",
            items: [{identifier: 1, text: "ЗДРАВСТВУЙТЕ1"}, {identifier: 2, text: "ЗДРАВСТВУЙТЕ2"}, {identifier: 3, text: "ЗДРАВСТВУЙТЕ3"}]}},
  {command: "d0578103012400820281828510820c04108784908082919282938992858f1201820d0410878490808291928293899285318f1202820d0410878490808291928293899285328f1203820d041087849080829192829389928533",
   expect: {commandQualifier: 0x00,
            title: "ЗДРАВСТВУЙТЕ",
            items: [{identifier: 1, text: "ЗДРАВСТВУЙТЕ1"}, {identifier: 2, text: "ЗДРАВСТВУЙТЕ2"}, {identifier: 3, text: "ЗДРАВСТВУЙТЕ3"}]}},
  {command: "d03e810301240082028182850b805de551777bb1900962e98f080180987976ee4e008f080280987976ee4e8c8f080380987976ee4e098f080480987976ee56db",
   expect: {commandQualifier: 0x00,
            title: "工具箱选择",
            items: [{identifier: 1, text: "项目一"}, {identifier: 2, text: "项目二"}, {identifier: 3, text: "项目三"}, {identifier: 4, text: "项目四"}]}},
  {command: "d0388103012400820281828509800038003030eb00308f0a01800038003030eb00318f0a02800038003030eb00328f0a03800038003030eb0033",
   expect: {commandQualifier: 0x00,
            title: "80ル0",
            items: [{identifier: 1, text: "80ル1"}, {identifier: 2, text: "80ル2"}, {identifier: 3, text: "80ル3"}]}},
  {command: "d03081030124008202818285078104613831eb308f08018104613831eb318f08028104613831eb328f08038104613831eb33",
   expect: {commandQualifier: 0x00,
            title: "81ル0",
            items: [{identifier: 1, text: "81ル1"}, {identifier: 2, text: "81ル2"}, {identifier: 3, text: "81ル3"}]}},
  {command: "d0348103012400820281828508820430a03832cb308f0901820430a03832cb318f0902820430a03832cb328f0903820430a03832cb33",
   expect: {commandQualifier: 0x00,
            title: "82ル0",
            items: [{identifier: 1, text: "82ル1"}, {identifier: 2, text: "82ル2"}, {identifier: 3, text: "82ル3"}]}},
  {command: "d039810301240082028182850e546f6f6c6b69742053656c6563748f07014974656d20318f07024974656d20328f07034974656d20331803000081",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}, {identifier: 3, text: "Item 3"}],
            nextActionList: [MozIccManager.STK_NEXT_ACTION_NULL, MozIccManager.STK_NEXT_ACTION_NULL, MozIccManager.STK_NEXT_ACTION_END_PROACTIVE_SESSION]}},
];

function testSelectItem(aCommand, aExpect) {
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SELECT_ITEM, "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");
  is(aCommand.options.title, aExpect.title, "options.title");
  is(aCommand.options.presentationType, aCommand.commandQualifier & 0x03, "presentationType");

  for (let index in aExpect.items) {
    let item = aCommand.options.items[index];
    let itemExpect = aExpect.items[index];
    is(item.identifier, itemExpect.identifier,
       "options.items[" + index + "].identifier");
    is(item.text, itemExpect.text,
       "options.items[" + index + "].text");

    if (itemExpect.icons) {
      isIcons(item.icons, itemExpect.icons);
      is(item.iconSelfExplanatory, itemExpect.iconSelfExplanatory,
         "options.items[" + index + "].iconSelfExplanatory");
    }
  }

  if (aExpect.icons) {
    isIcons(aCommand.options.icons, aExpect.icons);
    is(aCommand.options.iconSelfExplanatory, aExpect.iconSelfExplanatory,
       "options.iconSelfExplanatory");
  }

  if (aExpect.nextActionList) {
    for (let index in aExpect.nextActionList) {
      is(aCommand.options.nextActionList[index], aExpect.nextActionList[index],
         "options.nextActionList[" + index + "]");
    }
  }
}

// Start tests
startTestCommon(function() {
  let icc = getMozIcc();
  let promise = Promise.resolve();
  for (let i = 0; i < TEST_DATA.length; i++) {
    let data = TEST_DATA[i];
    promise = promise.then(() => {
      log("select_item_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testSelectItem(aEvent.command, data.expect)));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
