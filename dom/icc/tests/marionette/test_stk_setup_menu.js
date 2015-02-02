/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "d081fc810301250082028182850a4c617267654d656e75318f05505a65726f8f044f4f6e658f044e54776f8f064d54687265658f054c466f75728f054b466976658f044a5369788f0649536576656e8f064845696768748f05474e696e658f0646416c7068618f0645427261766f8f0844436861726c69658f064344656c74618f05424563686f8f0941466f782d74726f748f0640426c61636b8f063f42726f776e8f043e5265648f073d4f72616e67658f073c59656c6c6f778f063b477265656e8f053a426c75658f073956696f6c65748f0538477265798f063757686974658f06366d696c6c698f06356d6963726f8f05346e616e6f8f05337069636f",
   expect: {commandQualifier: 0x00,
            title: "LargeMenu1",
            items: [{identifier: 80, text: "Zero"}, {identifier: 79, text: "One"}, {identifier: 78, text: "Two"}, {identifier: 77, text: "Three"}, {identifier: 76, text: "Four"}, {identifier: 75, text: "Five"}, {identifier: 74, text: "Six"}, {identifier: 73, text: "Seven"}, {identifier: 72, text: "Eight"}, {identifier: 71, text: "Nine"}, {identifier: 70, text: "Alpha"}, {identifier: 69, text: "Bravo"}, {identifier: 68, text: "Charlie"}, {identifier: 67, text: "Delta"}, {identifier: 66, text: "Echo"}, {identifier: 65, text: "Fox-trot"}, {identifier: 64, text: "Black"}, {identifier: 63, text: "Brown"}, {identifier: 62, text: "Red"}, {identifier: 61, text: "Orange"}, {identifier: 60, text: "Yellow"}, {identifier: 59, text: "Green"}, {identifier: 58, text: "Blue"}, {identifier: 57, text: "Violet"}, {identifier: 56, text: "Grey"}, {identifier: 55, text: "White"}, {identifier: 54, text: "milli"}, {identifier: 53, text: "micro"}, {identifier: 52, text: "nano"}, {identifier: 51, text: "pico"}]}},
  {command: "d081f3810301250082028182850a4c617267654d656e75328f1dff312043616c6c20466f727761726420556e636f6e646974696f6e616c8f1cfe322043616c6c20466f7277617264204f6e205573657220427573798f1bfd332043616c6c20466f7277617264204f6e204e6f205265706c798f25fc342043616c6c20466f7277617264204f6e2055736572204e6f7420526561636861626c658f20fb352042617272696e67204f6620416c6c204f7574676f696e672043616c6c738f24fa362042617272696e67204f6620416c6c204f7574676f696e6720496e742043616c6c738f13f93720434c492050726573656e746174696f6e",
   expect: {commandQualifier: 0x00,
            title: "LargeMenu2",
            items: [{identifier: 255, text: "1 Call Forward Unconditional"}, {identifier: 254, text: "2 Call Forward On User Busy"}, {identifier: 253, text: "3 Call Forward On No Reply"}, {identifier: 252, text: "4 Call Forward On User Not Reachable"}, {identifier: 251, text: "5 Barring Of All Outgoing Calls"}, {identifier: 250, text: "6 Barring Of All Outgoing Int Calls"}, {identifier: 249, text: "7 CLI Presentation"}]}},
  {command: "d081fc8103012500820281828581ec5468652053494d207368616c6c20737570706c79206120736574206f66206d656e75206974656d732c207768696368207368616c6c20626520696e7465677261746564207769746820746865206d656e752073797374656d20286f72206f74686572204d4d4920666163696c6974792920696e206f7264657220746f206769766520746865207573657220746865206f70706f7274756e69747920746f2063686f6f7365206f6e65206f66207468657365206d656e75206974656d7320617420686973206f776e2064697363726574696f6e2e2045616368206974656d20636f6d70726973657320612073688f020159",
   expect: {commandQualifier: 0x00,
            title: "The SIM shall supply a set of menu items, which shall be integrated with the menu system (or other MMI facility) in order to give the user the opportunity to choose one of these menu items at his own discretion. Each item comprises a sh",
            items: [{identifier: 1, text: "Y"}]}},
  {command: "d03b810301258082028182850c546f6f6c6b6974204d656e758f07014974656d20318f07024974656d20328f07034974656d20338f07044974656d2034",
   expect: {commandQualifier: 0x80,
            title: "Toolkit Menu",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}, {identifier: 3, text: "Item 3"}, {identifier: 4, text: "Item 4"}]}},
  {command: "d041810301250082028182850c546f6f6c6b6974204d656e758f07014974656d20318f07024974656d20328f07034974656d20338f07044974656d2034180413101526",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Menu",
            items: [{identifier: 1, text: "Item 1"}, {identifier: 2, text: "Item 2"}, {identifier: 3, text: "Item 3"}, {identifier: 4, text: "Item 4"}],
            nextActionList: [MozIccManager.STK_CMD_SEND_SMS, MozIccManager.STK_CMD_SET_UP_CALL, MozIccManager.STK_CMD_LAUNCH_BROWSER, MozIccManager.STK_CMD_PROVIDE_LOCAL_INFO]}},
  {command: "d03c810301250082028182850c546f6f6c6b6974204d656e758f07014974656d20318f07024974656d20328f07034974656d20339e0200019f0400050505",
   expect: {commandQualifier: 0x00,
            title: "Toolkit Menu",
            iconSelfExplanatory: true,
            icons: [BASIC_ICON],
            items: [{identifier: 1, text: "Item 1", iconSelfExplanatory: true, icons: [COLOR_TRANSPARENCY_ICON]},
                    {identifier: 2, text: "Item 2", iconSelfExplanatory: true, icons: [COLOR_TRANSPARENCY_ICON]},
                    {identifier: 3, text: "Item 3", iconSelfExplanatory: true, icons: [COLOR_TRANSPARENCY_ICON]}]}},
  {command: "d0819c8103012500820281828519800417041404200410041204210422041204230419042204158f1c018004170414042004100412042104220412042304190422041500318f1c028004170414042004100412042104220412042304190422041500328f1c038004170414042004100412042104220412042304190422041500338f1c04800417041404200410041204210422041204230419042204150034",
   expect: {commandQualifier: 0x00,
            title: "ЗДРАВСТВУЙТЕ",
            items: [{identifier: 1, text: "ЗДРАВСТВУЙТЕ1"}, {identifier: 2, text: "ЗДРАВСТВУЙТЕ2"}, {identifier: 3, text: "ЗДРАВСТВУЙТЕ3"}, {identifier: 4, text: "ЗДРАВСТВУЙТЕ4"}]}},
  {command: "d03c8103012500820281828509805de551777bb153558f080180987976ee4e008f080280987976ee4e8c8f080380987976ee4e098f080480987976ee56db",
   expect: {commandQualifier: 0x00,
            title: "工具箱单",
            items: [{identifier: 1, text: "项目一"}, {identifier: 2, text: "项目二"}, {identifier: 3, text: "项目三"}, {identifier: 4, text: "项目四"}]}},
  {command: "d0208103012500820281828509805de551777bb153558f0411804e008f0412804e8c",
  // Test remove setup menu.
  {command: "D00D81030125008202818285008F00",
   expect: {commandQualifier: 0x00,
            title: "",
            items: [null]}},
];

function testSetupMenu(aCommand, aExpect) {
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SET_UP_MENU, "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");
  is(aCommand.options.title, aExpect.title, "options.title");
  // To ensure that 'presentationType' will only be available in SELECT_ITEM.
  is(aCommand.options.presentationType, undefined, "presentationType");

  for (let index in aExpect.items) {
    let item = aCommand.options.items[index];
    let itemExpect = aExpect.items[index];

    if (!itemExpect) {
      is(item, itemExpect, "options.items[" + index + "]");
    } else {
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
      log("setup_menu_cmd: " + data.command);

      let promises = [];
      // Wait onstkcommand event.
      promises.push(waitForTargetEvent(icc, "stkcommand")
        .then((aEvent) => testSetupMenu(aEvent.command, data.expect)));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
