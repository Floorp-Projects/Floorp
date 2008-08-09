////////////////////////////////////////////////////////////////////////////////
// General

const nsIAccessibleRetrieval = Components.interfaces.nsIAccessibleRetrieval;
const nsIAccessibleRole = Components.interfaces.nsIAccessibleRole;

var gAccRetrieval = null;

function initialize()
{
  gAccRetrieval = Components.classes["@mozilla.org/accessibleRetrieval;1"].
  getService(nsIAccessibleRetrieval);
}

addLoadEvent(initialize);

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

  var ID = aArray[aIndex].ID;
  var actionName = aArray[aIndex].actionName;
  var events = aArray[aIndex].events;

  var elm = document.getElementById(ID);
  if (!elm) {
    ok(false, "There is no element with ID " + ID);
    SimpleTest.finish();
    return null;
  }

  var acc = null;
  try {
    acc = gAccRetrieval.getAccessibleFor(elm);
  } catch(e) {
  }

  if (!acc) {
    ok(false, "There is no accessible for " + ID);
    SimpleTest.finish();
    return null;
  }

  is(acc.getActionName(0), actionName,
     "Wrong action name of the accessible for " + ID);

  gEventHandler.initialize(ID, elm, events);

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
