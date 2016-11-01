/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "D081FC" + // Length
            "8103012500" + // Command details
            "82028182" + // Device identities
            "850A4C617267654D656E7531" + // Alpha identifier
            "8F05505A65726F" + // Item
            "8F044F4F6E65" + // Item
            "8F044E54776F" + // Item
            "8F064D5468726565" + // Item
            "8F054C466F7572" + // Item
            "8F054B46697665" + // Item
            "8F044A536978" + // Item
            "8F0649536576656E" + // Item
            "8F06484569676874" + // Item
            "8F05474E696E65" + // Item
            "8F0646416C706861" + // Item
            "8F0645427261766F" + // Item
            "8F0844436861726C6965" + // Item
            "8F064344656C7461" + // Item
            "8F05424563686F" + // Item
            "8F0941466F782D74726F74" + // Item
            "8F0640426C61636B" + // Item
            "8F063F42726F776E" + // Item
            "8F043E526564" + // Item
            "8F073D4F72616E6765" + // Item
            "8F073C59656C6C6F77" + // Item
            "8F063B477265656E" + // Item
            "8F053A426C7565" + // Item
            "8F073956696F6C6574" + // Item
            "8F053847726579" + // Item
            "8F06375768697465" + // Item
            "8F06366D696C6C69" + // Item
            "8F06356D6963726F" + // Item
            "8F05346E616E6F" + // Item
            "8F05337069636F", // Item
   expect: {commandQualifier: 0x00,
            title: "LargeMenu1",
            items: [{identifier: 80, text: "Zero"},
                    {identifier: 79, text: "One"},
                    {identifier: 78, text: "Two"},
                    {identifier: 77, text: "Three"},
                    {identifier: 76, text: "Four"},
                    {identifier: 75, text: "Five"},
                    {identifier: 74, text: "Six"},
                    {identifier: 73, text: "Seven"},
                    {identifier: 72, text: "Eight"},
                    {identifier: 71, text: "Nine"},
                    {identifier: 70, text: "Alpha"},
                    {identifier: 69, text: "Bravo"},
                    {identifier: 68, text: "Charlie"},
                    {identifier: 67, text: "Delta"},
                    {identifier: 66, text: "Echo"},
                    {identifier: 65, text: "Fox-trot"},
                    {identifier: 64, text: "Black"},
                    {identifier: 63, text: "Brown"},
                    {identifier: 62, text: "Red"},
                    {identifier: 61, text: "Orange"},
                    {identifier: 60, text: "Yellow"},
                    {identifier: 59, text: "Green"},
                    {identifier: 58, text: "Blue"},
                    {identifier: 57, text: "Violet"},
                    {identifier: 56, text: "Grey"},
                    {identifier: 55, text: "White"},
                    {identifier: 54, text: "milli"},
                    {identifier: 53, text: "micro"},
                    {identifier: 52, text: "nano"},
                    {identifier: 51, text: "pico"}]}},
  {command: "D081F3" + // Length
            "8103012500" + // Command details
            "82028182" + // Device identities
            "850A4C617267654D656E7532" + // Alpha identifier
            "8F1DFF312043616C6C20466F727761726420556E636F6E646974" + // Item
            "696F6E616C" +
            "8F1CFE322043616C6C20466F7277617264204F6E205573657220" + // Item
            "42757379" +
            "8F1BFD332043616C6C20466F7277617264204F6E204E6F205265" + // Item
            "706C79" +
            "8F25FC342043616C6C20466F7277617264204F6E205573657220" + // Item
            "4E6F7420526561636861626C65" +
            "8F20FB352042617272696E67204F6620416C6C204F7574676F69" + // Item
            "6E672043616C6C73" +
            "8F24FA362042617272696E67204F6620416C6C204F7574676F69" + // Item
            "6E6720496E742043616C6C73" +
            "8F13F93720434C492050726573656E746174696F6E", // Item
   expect: {commandQualifier: 0x00,
            title: "LargeMenu2",
            items: [{identifier: 255, text: "1 Call Forward Unconditional"},
                    {identifier: 254, text: "2 Call Forward On User Busy"},
                    {identifier: 253, text: "3 Call Forward On No Reply"},
                    {identifier: 252, text: "4 Call Forward On User Not Reachable"},
                    {identifier: 251, text: "5 Barring Of All Outgoing Calls"},
                    {identifier: 250, text: "6 Barring Of All Outgoing Int Calls"},
                    {identifier: 249, text: "7 CLI Presentation"}]}},
  {command: "D081FC" + // Length
            "8103012500" + // Command details
            "82028182" + // Device identities
            "8581EC5468652053494D207368616C6C20737570" + // Alpha identifier
            "706C79206120736574206F66206D656E75206974" +
            "656D732C207768696368207368616C6C20626520" +
            "696E746567726174656420776974682074686520" +
            "6D656E752073797374656D20286F72206F746865" +
            "72204D4D4920666163696C6974792920696E206F" +
            "7264657220746F20676976652074686520757365" +
            "7220746865206F70706F7274756E69747920746F" +
            "2063686F6F7365206F6E65206F66207468657365" +
            "206D656E75206974656D7320617420686973206F" +
            "776E2064697363726574696F6E2E204561636820" +
            "6974656D20636F6D7072697365732061207368" +
            "8F020159", // Item
   expect: {commandQualifier: 0x00,
            title: "The SIM shall supply a set of menu items, which shall " +
                   "be integrated with the menu system (or other MMI " +
                   "facility) in order to give the user the opportunity to " +
                   "choose one of these menu items at his own discretion. " +
                   "Each item comprises a sh",
            items: [{identifier: 1, text: "Y"}]}},
  {command: "D03B" + // Length
            "8103012580" + // Command details
            "82028182" + // Device identities
            "850C546F6F6C6B6974204D656E75" + // Alpha identifier
            "8F07014974656D2031" + // Item
            "8F07024974656D2032" + // Item
            "8F07034974656D2033" + // Item
            "8F07044974656D2034", // Item
   expect: {commandQualifier: 0x80,
            title: "Toolkit Menu",
            items: [{identifier: 1, text: "Item 1"},
                    {identifier: 2, text: "Item 2"},
                    {identifier: 3, text: "Item 3"},
                    {identifier: 4, text: "Item 4"}]}},
  {command: "D041" + // Length
            "8103012500" + // Command details
            "82028182" + // Device identities
            "850C546F6F6C6B6974204D656E75" + // Alpha identifier
            "8F07014974656D2031" + // Item
            "8F07024974656D2032" + // Item
            "8F07034974656D2033" + // Item
            "8F07044974656D2034" + // Item
            "180413101526", // Items next action indicator
   expect: {commandQualifier: 0x00,
            title: "Toolkit Menu",
            items: [{identifier: 1, text: "Item 1"},
                    {identifier: 2, text: "Item 2"},
                    {identifier: 3, text: "Item 3"},
                    {identifier: 4, text: "Item 4"}],
            nextActionList: [MozIccManager.STK_CMD_SEND_SMS,
                             MozIccManager.STK_CMD_SET_UP_CALL,
                             MozIccManager.STK_CMD_LAUNCH_BROWSER,
                             MozIccManager.STK_CMD_PROVIDE_LOCAL_INFO]}},
  {command: "D03C" + // Length
            "8103012500" + // Command details
            "82028182" + // Device identities
            "850C546F6F6C6B6974204D656E75" + // Alpha identifier
            "8F07014974656D2031" + // Item
            "8F07024974656D2032" + // Item
            "8F07034974656D2033" + // Item
            "9E020001" + // Icon identifier
            "9F0400050505", // Item icon identifier list
   expect: {commandQualifier: 0x00,
            title: "Toolkit Menu",
            iconSelfExplanatory: true,
            icons: [BASIC_ICON],
            items: [{identifier: 1, text: "Item 1", iconSelfExplanatory: true, icons: [COLOR_TRANSPARENCY_ICON]},
                    {identifier: 2, text: "Item 2", iconSelfExplanatory: true, icons: [COLOR_TRANSPARENCY_ICON]},
                    {identifier: 3, text: "Item 3", iconSelfExplanatory: true, icons: [COLOR_TRANSPARENCY_ICON]}]}},
  {command: "D036" + // Length
            "8103012500" + // Command details
            "82028182" + // Device identities
            "850C546F6F6C6B6974204D656E75" + // Alpha identifier
            "8F07014974656D2031" + // Item
            "8F07024974656D2032" + // Item
            "8F07034974656D2033" + // Item
            "9E020001", // Icon identifier
   expect: {commandQualifier: 0x00,
            title: "Toolkit Menu",
            iconSelfExplanatory: true,
            icons: [BASIC_ICON],
            items: [{identifier: 1, text: "Item 1"},
                    {identifier: 2, text: "Item 2"},
                    {identifier: 3, text: "Item 3"}]}},
  {command: "D038" + // Length
            "8103012500" + // Command details
            "82028182" + // Device identities
            "850C546F6F6C6B6974204D656E75" + // Alpha identifier
            "8F07014974656D2031" + // Item
            "8F07024974656D2032" + // Item
            "8F07034974656D2033" + // Item
            "9F0400030303", // Item icon identifier list
   expect: {commandQualifier: 0x00,
            title: "Toolkit Menu",
            items: [{identifier: 1, text: "Item 1", iconSelfExplanatory: true, icons: [COLOR_ICON]},
                    {identifier: 2, text: "Item 2", iconSelfExplanatory: true, icons: [COLOR_ICON]},
                    {identifier: 3, text: "Item 3", iconSelfExplanatory: true, icons: [COLOR_ICON]}]}},
  {command: "D0819C" + // Length
            "8103012500" + // Command details
            "82028182" + // Device identities
            "851980041704140420041004120421042204120423041904220415" + // Alpha identifier
            "8F1C01800417041404200410041204210422041204230419042204150031" + // Item
            "8F1C02800417041404200410041204210422041204230419042204150032" + // Item
            "8F1C03800417041404200410041204210422041204230419042204150033" + // Item
            "8F1C04800417041404200410041204210422041204230419042204150034", // Item
   expect: {commandQualifier: 0x00,
            title: "ЗДРАВСТВУЙТЕ",
            items: [{identifier: 1, text: "ЗДРАВСТВУЙТЕ1"},
                    {identifier: 2, text: "ЗДРАВСТВУЙТЕ2"},
                    {identifier: 3, text: "ЗДРАВСТВУЙТЕ3"},
                    {identifier: 4, text: "ЗДРАВСТВУЙТЕ4"}]}},
  {command: "D03C" + // Length
            "8103012500" + // Command details
            "82028182" + // Device identities
            "8509805DE551777BB15355" + // Alpha identifier
            "8F080180987976EE4E00" + // Item
            "8F080280987976EE4E8C" + // Item
            "8F080380987976EE4E09" + // Item
            "8F080480987976EE56DB", // Item
   expect: {commandQualifier: 0x00,
            title: "工具箱单",
            items: [{identifier: 1, text: "项目一"},
                    {identifier: 2, text: "项目二"},
                    {identifier: 3, text: "项目三"},
                    {identifier: 4, text: "项目四"}]}},
  // Test remove setup menu.
  {command: "D00D" + // Length
            "8103012500" + // Command details
            "82028182" + // Device identities
            "8500" + // Alpha identifier
            "8F00", // Item
   expect: {commandQualifier: 0x00,
            title: "",
            items: [null]}},
];

function testSetupMenu(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SET_UP_MENU, "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");

  // To ensure that 'presentationType' will only be available in SELECT_ITEM.
  is(aCommand.options.presentationType, undefined, "presentationType");
  is(aCommand.options.isHelpAvailable, !!(aCommand.commandQualifier & 0x80),
     "isHelpAvailable");
  is(aCommand.options.title, aExpect.title, "options.title");

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

  // icons is optional.
  if ("icons" in aExpect) {
    isIcons(aCommand.options.icons, aExpect.icons);
    is(aCommand.options.iconSelfExplanatory, aExpect.iconSelfExplanatory,
       "options.iconSelfExplanatory");
  }

  // nextActionList is optional.
  if ("nextActionList" in aExpect) {
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
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          testSetupMenu(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
