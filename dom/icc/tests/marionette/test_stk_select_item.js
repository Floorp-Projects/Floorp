/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

const TEST_DATA = [
  {command: "D03D" + // Length
            "8103012400" + // Command details
            "82028182" + // Device identities
            "850E546F6F6C6B69742053656C656374" + // Alpha identifier
            "8F07014974656D2031" + // Item
            "8F07024974656D2032" + // Item
            "8F07034974656D2033" + // Item
            "8F07044974656D2034", // Item
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "Item 1"},
                    {identifier: 2, text: "Item 2"},
                    {identifier: 3, text: "Item 3"},
                    {identifier: 4, text: "Item 4"}]}},
  {command: "D081FC" + // Length
            "8103012400" + // Command details
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
  {command: "D081FB" + // Length
            "8103012400" + // Command details
            "82028182" + // Device identities
            "850A4C617267654D656E7532" + // Alpha identifier
            "8F1EFF43616C6C20466F7277617264696E6720556E636F6E6469" + // Item
            "74696F6E616C" +
            "8F1DFE43616C6C20466F7277617264696E67204F6E2055736572" + // Item
            "2042757379" +
            "8F1CFD43616C6C20466F7277617264696E67204F6E204E6F2052" + // Item
            "65706C79" +
            "8F26FC43616C6C20466F7277617264696E67204F6E2055736572" + // Item
            "204E6F7420526561636861626C65" +
            "8F1EFB42617272696E67204F6620416C6C204F7574676F696E67" + // Item
            "2043616C6C73" +
            "8F2CFA42617272696E67204F6620416C6C204F7574676F696E67" + // Item
            "20496E7465726E6174696F6E616C2043616C6C73" +
            "8F11F9434C492050726573656E746174696F6E", // Item
   expect: {commandQualifier: 0x00,
            title: "LargeMenu2",
            items: [{identifier: 255, text: "Call Forwarding Unconditional"},
                    {identifier: 254, text: "Call Forwarding On User Busy"},
                    {identifier: 253, text: "Call Forwarding On No Reply"},
                    {identifier: 252, text: "Call Forwarding On User Not Reachable"},
                    {identifier: 251, text: "Barring Of All Outgoing Calls"},
                    {identifier: 250, text: "Barring Of All Outgoing International Calls"},
                    {identifier: 249, text: "CLI Presentation"}]}},
  {command: "D081FD" + // Length
            "8103012400" + // Command details
            "82028182" + // Device identities
            "8581ED5468652053494D207368616C6C20737570" + // Alpha identifier
            "706C79206120736574206F66206974656D732066" +
            "726F6D207768696368207468652075736572206D" +
            "61792063686F6F7365206F6E652E204561636820" +
            "6974656D20636F6D70726973657320612073686F" +
            "7274206964656E74696669657220287573656420" +
            "746F20696E646963617465207468652073656C65" +
            "6374696F6E2920616E6420612074657874207374" +
            "72696E672E204F7074696F6E616C6C7920746865" +
            "2053494D206D617920696E636C75646520616E20" +
            "616C706861206964656E7469666965722E205468" +
            "6520616C706861206964656E7469666965722069" +
            "8F020159", // Item
   expect: {commandQualifier: 0x00,
            title: "The SIM shall supply a set of items from which the user " +
                   "may choose one. Each item comprises a short identifier " +
                   "(used to indicate the selection) and a text string. " +
                   "Optionally the SIM may include an alpha identifier. " +
                   "The alpha identifier i",
            items: [{identifier: 1, text: "Y"}]}},
  {command: "D039" + // Length
            "8103012400" + // Command details
            "82028182" + // Device identities
            "850E546F6F6C6B69742053656C656374" + // Alpha identifier
            "8F07014974656D2031" + // Item
            "8F07024974656D2032" + // Item
            "8F07034974656D2033" + // Item
            "1803131026", // Items next action indicator
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "Item 1"},
                    {identifier: 2, text: "Item 2"},
                    {identifier: 3, text: "Item 3"}],
            nextActionList: [MozIccManager.STK_CMD_SEND_SMS,
                             MozIccManager.STK_CMD_SET_UP_CALL,
                             MozIccManager.STK_CMD_PROVIDE_LOCAL_INFO]}},
  {command: "D03E" + // Length
            "8103012400" + // Command details
            "82028182" + // Device identities
            "850E546F6F6C6B69742053656C656374" + // Alpha identifier
            "8F07014974656D2031" + // Item
            "8F07024974656D2032" + // Item
            "8F07034974656D2033" + // Item
            "9E020101" + // Icon identifier
            "9F0401030303", // Item icon identifier list
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select",
            iconSelfExplanatory: false,
            icons: [BASIC_ICON],
            items: [{identifier: 1, text: "Item 1", iconSelfExplanatory: false, icons: [COLOR_ICON]},
                    {identifier: 2, text: "Item 2", iconSelfExplanatory: false, icons: [COLOR_ICON]},
                    {identifier: 3, text: "Item 3", iconSelfExplanatory: false, icons: [COLOR_ICON]}]}},
  {command: "D028" + // Length
            "8103012400" + // Command details
            "82028182" + // Device identities
            "850E546F6F6C6B69742053656C656374" + // Alpha identifier
            "8F0101" + // Item
            "8F0102" + // Item
            "8F0103" + // Item
            "9F0400050505", // Item icon identifier list
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "", iconSelfExplanatory: true, icons: [COLOR_TRANSPARENCY_ICON]},
                    {identifier: 2, text: "", iconSelfExplanatory: true, icons: [COLOR_TRANSPARENCY_ICON]},
                    {identifier: 3, text: "", iconSelfExplanatory: true, icons: [COLOR_TRANSPARENCY_ICON]}]}},
  {command: "D034" + // Length
            "8103012483" + // Command details
            "82028182" + // Device identities
            "850E546F6F6C6B69742053656C656374" + // Alpha identifier
            "8F07014974656D2031" + // Item
            "8F07024974656D2032" + // Item
            "8F07034974656D2033", // Item
   expect: {commandQualifier: 0x83,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "Item 1"},
                    {identifier: 2, text: "Item 2"},
                    {identifier: 3, text: "Item 3"}]}},
  {command: "D03D" + // Length
            "8103012400" + // Command details
            "82028182" + // Device identities
            "8510546F6F6C6B69742053656C6563742031" + // Alpha identifier
            "8F07014974656D2031" + // Item
            "8F07024974656D2032" + // Item
            "D004001000B4" + // Text attribute
            "D108000600B4000600B4", // Item text attribute list
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select 1",
            items: [{identifier: 1, text: "Item 1"},
                    {identifier: 2, text: "Item 2"}]}},
  {command: "D069" + // Length
            "8103012400" + // Command details
            "82028182" + // Device identities
            "851980041704140420041004120421042204120423041904220415" + // Alpha identifier
            "8F1C01800417041404200410041204210422041204230419042204150031" + // Item
            "8F1102810D089794A09092A1A292A399A29532" + // Item
            "8F1203820D041087849080829192829389928533", // Item
   expect: {commandQualifier: 0x00,
            title: "ЗДРАВСТВУЙТЕ",
            items: [{identifier: 1, text: "ЗДРАВСТВУЙТЕ1"},
                    {identifier: 2, text: "ЗДРАВСТВУЙТЕ2"},
                    {identifier: 3, text: "ЗДРАВСТВУЙТЕ3"}]}},
  {command: "D038" + // Length
            "8103012400" + // Command details
            "82028182" + // Device identities
            "8509800038003030EB0030" + // Alpha identifier
            "8F0A01800038003030EB0031" + // Item
            "8F0A02800038003030EB0032" + // Item
            "8F0A03800038003030EB0033", // Item
   expect: {commandQualifier: 0x00,
            title: "80ル0",
            items: [{identifier: 1, text: "80ル1"},
                    {identifier: 2, text: "80ル2"},
                    {identifier: 3, text: "80ル3"}]}},
  {command: "D030" + // Length
            "8103012400" + // Command details
            "82028182" + // Device identities
            "85078104613831EB30" + // Alpha identifier
            "8F08018104613831EB31" + // Item
            "8F08028104613831EB32" + // Item
            "8F08038104613831EB33", // Item
   expect: {commandQualifier: 0x00,
            title: "81ル0",
            items: [{identifier: 1, text: "81ル1"},
                    {identifier: 2, text: "81ル2"},
                    {identifier: 3, text: "81ル3"}]}},
  {command: "D034" + // Length
            "8103012400" + // Command details
            "82028182" + // Device identities
            "8508820430A03832CB30" + // Alpha identifier
            "8F0901820430A03832CB31" + // Item
            "8F0902820430A03832CB32" + // Item
            "8F0903820430A03832CB33", // Item
   expect: {commandQualifier: 0x00,
            title: "82ル0",
            items: [{identifier: 1, text: "82ル1"},
                    {identifier: 2, text: "82ル2"},
                    {identifier: 3, text: "82ル3"}]}},
  {command: "D03C" + // Length
            "8103012400" + // Command details
            "82028182" + // Device identities
            "850E546F6F6C6B69742053656C656374" + // Alpha identifier
            "8F07014974656D2031" + // Item
            "8F07024974656D2032" + // Item
            "8F07034974656D2033" + // Item
            "1803000081" + // Items next action indicator
            "100102", // Item identifier
   expect: {commandQualifier: 0x00,
            title: "Toolkit Select",
            items: [{identifier: 1, text: "Item 1"},
                    {identifier: 2, text: "Item 2"},
                    {identifier: 3, text: "Item 3"}],
            nextActionList: [MozIccManager.STK_NEXT_ACTION_NULL,
                             MozIccManager.STK_NEXT_ACTION_NULL,
                             MozIccManager.STK_NEXT_ACTION_END_PROACTIVE_SESSION],
            defaultItem: 1}},
];

function testSelectItem(aCommand, aExpect) {
  is(aCommand.commandNumber, 0x01, "commandNumber");
  is(aCommand.typeOfCommand, MozIccManager.STK_CMD_SELECT_ITEM, "typeOfCommand");
  is(aCommand.commandQualifier, aExpect.commandQualifier, "commandQualifier");

  is(aCommand.options.presentationType, aCommand.commandQualifier & 0x03,
     "presentationType");
  is(aCommand.options.isHelpAvailable, !!(aCommand.commandQualifier & 0x80),
     "isHelpAvailable");
  is(aCommand.options.title, aExpect.title, "options.title");

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

  // defaultItem is optional.
  if ("defaultItem" in aExpect) {
    is(aCommand.options.defaultItem, aExpect.defaultItem, "options.defaultItem");
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
      // Wait icc-stkcommand system message.
      promises.push(waitForSystemMessage("icc-stkcommand")
        .then((aMessage) => {
          is(aMessage.iccId, icc.iccInfo.iccid, "iccId");
          testSelectItem(aMessage.command, data.expect);
        }));
      // Send emulator command to generate stk unsolicited event.
      promises.push(sendEmulatorStkPdu(data.command));

      return Promise.all(promises);
    });
  }
  return promise;
});
