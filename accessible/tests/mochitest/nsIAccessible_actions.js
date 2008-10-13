////////////////////////////////////////////////////////////////////////////////
// Event constants

const MOUSEDOWN_EVENT = 1;
const MOUSEUP_EVENT = 2;
const CLICK_EVENT = 4;
const COMMAND_EVENT = 8;

const CLICK_EVENTS = MOUSEDOWN_EVENT | MOUSEUP_EVENT | CLICK_EVENT;
const ALL_EVENTS = CLICK_EVENTS | COMMAND_EVENT;

////////////////////////////////////////////////////////////////////////////////
// Public functions

function testActions(aArray, aIndex)
{
  if (!aIndex)
    aIndex = 0;

  if (aIndex == aArray.length) {
    SimpleTest.finish();
    return;
  }

  var accOrElmOrID = aArray[aIndex].ID;
  var actionName = aArray[aIndex].actionName;
  var events = aArray[aIndex].events;

  var elmObj = {};
  var acc = getAccessible(accOrElmOrID, null, elmObj);
  var elm = elmObj.value;

  var isThereActions = acc.numActions > 0;
  ok(isThereActions,
     "No actions on the accessible for " + accOrElmOrID);

  if (!isThereActions) {
    SimpleTest.finish();
    return; // Stop test.
  }

  is(acc.getActionName(0), actionName,
     "Wrong action name of the accessible for " + accOrElmOrID);

  gEventHandler.initialize(accOrElmOrID, elm, events);

  acc.doAction(0);

  window.setTimeout(
    function()
    {
      gEventHandler.checkEvents();
      testActions(aArray, aIndex + 1);
    },
    200
  );
}

////////////////////////////////////////////////////////////////////////////////
// Private

var gEventHandler =
{
  initialize: function(aID, aElm, aExpectedEvents)
  {
    this.ID = aID,
    this.element = aElm;
    this.mExpectedEvents = aExpectedEvents;
    this.mFiredEvents = 0;

    if (this.mExpectedEvents & MOUSEDOWN_EVENT)
      aElm.addEventListener("mousedown", this, false);

    if (this.mExpectedEvents & MOUSEUP_EVENT)
      aElm.addEventListener("mouseup", this, false);

    if (this.mExpectedEvents & CLICK_EVENT)
      aElm.addEventListener("click", this, false);

    if (this.mExpectedEvents & COMMAND_EVENT)
      aElm.addEventListener("command", this, false);
  },

  checkEvents: function()
  {
    if (this.mExpectedEvents & MOUSEDOWN_EVENT) {
      ok(this.mFiredEvents & MOUSEDOWN_EVENT,
         "mousedown hasn't been fired for " + this.ID);
      this.element.removeEventListener("mousedown", this, false);
    }

    if (this.mExpectedEvents & MOUSEUP_EVENT) {
      ok(this.mFiredEvents & MOUSEUP_EVENT,
         "mouseup hasn't been fired for " + this.ID);
      this.element.removeEventListener("mouseup", this, false);
    }

    if (this.mExpectedEvents & CLICK_EVENT) {
      ok(this.mFiredEvents & CLICK_EVENT,
         "click hasn't been fired for " + this.ID);
      this.element.removeEventListener("click", this, false);
    }

    if (this.mExpectedEvents & COMMAND_EVENT) {
      ok(this.mFiredEvents & COMMAND_EVENT,
         "command hasn't been fired for " + this.ID);
      this.element.removeEventListener("command", this, false);
    }
  },

  ID: "",
  element: null,

  handleEvent : function(aEvent)
  {
    if (aEvent.type == "mousedown")
      this.mFiredEvents |= MOUSEDOWN_EVENT;
    else if(aEvent.type == "mouseup")
      this.mFiredEvents |= MOUSEUP_EVENT;
    else if(aEvent.type == "click")
      this.mFiredEvents |= CLICK_EVENT;
    else if(aEvent.type == "command")
      this.mFiredEvents |= COMMAND_EVENT;
  },

  mExpectedEvents: 0,
  mFiredEvents: 0
};
