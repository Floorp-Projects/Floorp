"use strict";

const PPB_TESTCASES = [
  {"__interface":"PPB_KeyboardInputEvent","__version":"1.2","__method":"IsKeyboardInputEvent","resource":0},
  {"__interface":"PPB_KeyboardInputEvent","__version":"1.2","__method":"GetKeyCode","key_event":0},
  {"__interface":"PPB_KeyboardInputEvent","__version":"1.2","__method":"GetCharacterText","character_event":0}
];


class Mock_DomEvent {
  constructor(eventType) {
    this.type = eventType;
    this.timeStamp = 0;
  }
}

class Mock_KeyboardInputEvent extends Mock_DomEvent {
  constructor(eventType, keyCode, charCode) {
    super(eventType);
    this.keyCode = keyCode;
    this.charCode = charCode;
  }
}

function run_test() {
  // We mock a "keydown" event to test "PPB_KeyboardInputEvent".
  let event = new Mock_KeyboardInputEvent("keydown", 65, 0);
  // To test PPB_KeyboardInputEvent we need to invoke event resource constructor
  // in ppapi-runtime.jsm to get a resource id.
  let eventType = EventTypes.get(event.type);
  let resource = new eventType.resourceCtor(instance, event);
  let PP_ResourceID = resource.toJSON();

  PPB_TESTCASES[0].resource = PP_ResourceID;
  PPB_TESTCASES[1].key_event = PP_ResourceID;
  Assert.equal(Call_PpbFunc(PPB_TESTCASES[0]), PP_Bool.PP_TRUE);
  Assert.equal(Call_PpbFunc(PPB_TESTCASES[1]), 65); // 65 is the keyCode when you press 'A'.

  // We mock a "keypress" event to test "PPB_KeyboardInputEvent".
  event = new Mock_KeyboardInputEvent("keypress", 0, 65);
  eventType = EventTypes.get(event.type);
  resource = new eventType.resourceCtor(instance, event);
  PP_ResourceID = resource.toJSON();

  PPB_TESTCASES[0].resource = PP_ResourceID;
  PPB_TESTCASES[2].character_event = PP_ResourceID;
  Assert.equal(Call_PpbFunc(PPB_TESTCASES[0]), PP_Bool.PP_TRUE);
  Assert.equal(Call_PpbFunc(PPB_TESTCASES[2]).type, PP_VarType.PP_VARTYPE_STRING);
}
