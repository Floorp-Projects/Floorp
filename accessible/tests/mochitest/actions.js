////////////////////////////////////////////////////////////////////////////////
// Event constants

const MOUSEDOWN_EVENT = 1;
const MOUSEUP_EVENT = 2;
const CLICK_EVENT = 4;
const COMMAND_EVENT = 8;
const FOCUS_EVENT = 16;

const CLICK_EVENTS = MOUSEDOWN_EVENT | MOUSEUP_EVENT | CLICK_EVENT;
const XUL_EVENTS = CLICK_EVENTS | COMMAND_EVENT;

////////////////////////////////////////////////////////////////////////////////
// Public functions

/**
 * Test default accessible actions.
 *
 * Action tester interface is:
 *
 *  var actionObj = {
 *    // identifier of accessible to perform an action on
 *    get ID() {},
 *
 *    // index of the action
 *    get actionIndex() {},
 *
 *    // name of the action
 *    get actionName() {},
 *
 *    // DOM events (see constants defined above)
 *    get events() {},
 *
 *    // [optional] identifier of target DOM events listeners are registered on,
 *    // used with 'events', if missing then 'ID' is used instead.
 *    get targetID() {},
 *
 *    // [optional] perform checks when 'click' event is handled if 'events'
 *    // is used.
 *    checkOnClickEvent: function() {},
 *
 *    // [optional] an array of invoker's checker objects (see eventQueue
 *    // constructor events.js)
 *    get eventSeq() {}
 *  };
 *
 *
 * @param  aArray [in] an array of action cheker objects
 */
function testActions(aArray)
{
  gActionsQueue = new eventQueue();

  for (var idx = 0; idx < aArray.length; idx++) {

    var actionObj = aArray[idx];
    var accOrElmOrID = actionObj.ID;
    var actionIndex = actionObj.actionIndex;
    var actionName = actionObj.actionName;
    var events = actionObj.events;
    var accOrElmOrIDOfTarget = actionObj.targetID ?
      actionObj.targetID : accOrElmOrID;

    var eventSeq = [];
    if (events) {
      var elm = getNode(accOrElmOrIDOfTarget);
      if (events & MOUSEDOWN_EVENT)
        eventSeq.push(new checkerOfActionInvoker("mousedown", elm));

      if (events & MOUSEUP_EVENT)
        eventSeq.push(new checkerOfActionInvoker("mouseup", elm));

      if (events & CLICK_EVENT)
        eventSeq.push(new checkerOfActionInvoker("click", elm, actionObj));

      if (events & COMMAND_EVENT)
        eventSeq.push(new checkerOfActionInvoker("command", elm));

      if (events & FOCUS_EVENT)
        eventSeq.push(new focusChecker(elm));
    }

    if (actionObj.eventSeq)
      eventSeq = eventSeq.concat(actionObj.eventSeq);

    var invoker = new actionInvoker(accOrElmOrID, actionIndex, actionName,
                                    eventSeq);
    gActionsQueue.push(invoker);
  }

  gActionsQueue.invoke();
}

/**
 * Test action names and descriptions.
 */
function testActionNames(aID, aActions)
{
  var actions = (typeof aActions == "string") ?
    [ aActions ] : (aActions || []);

  var acc = getAccessible(aID);
  is(acc.actionCount, actions.length, "Wong number of actions.");
  for (var i = 0; i < actions.length; i++ ) {
    is(acc.getActionName(i), actions[i], "Wrong action name at " + i + " index.");
    is(acc.getActionDescription(0), gActionDescrMap[actions[i]],
       "Wrong action description at " + i + "index.");
  }
}

////////////////////////////////////////////////////////////////////////////////
// Private

var gActionsQueue = null;

function actionInvoker(aAccOrElmOrId, aActionIndex, aActionName, aEventSeq)
{
  this.invoke = function actionInvoker_invoke()
  {
    var acc = getAccessible(aAccOrElmOrId);
    if (!acc)
      return INVOKER_ACTION_FAILED;

    var isThereActions = acc.actionCount > 0;
    ok(isThereActions,
       "No actions on the accessible for " + prettyName(aAccOrElmOrId));

    if (!isThereActions)
      return INVOKER_ACTION_FAILED;

    is(acc.getActionName(aActionIndex), aActionName,
       "Wrong action name of the accessible for " + prettyName(aAccOrElmOrId));

    try {
      acc.doAction(aActionIndex);
    }
    catch (e) {
      ok(false, "doAction(" + aActionIndex + ") failed with: " + e.name);
      return INVOKER_ACTION_FAILED;
    }
  }

  this.eventSeq = aEventSeq;

  this.getID = function actionInvoker_getID()
  {
    return "invoke an action " + aActionName + " at index " + aActionIndex +
      " on " + prettyName(aAccOrElmOrId);
  }
}

function checkerOfActionInvoker(aType, aTarget, aActionObj)
{
  this.type = aType;

  this.target = aTarget;

  this.phase = false;

  this.getID = function getID()
  {
    return aType + " event handling";
  }

  this.check = function check(aEvent)
  {
    if (aActionObj && "checkOnClickEvent" in aActionObj)
      aActionObj.checkOnClickEvent(aEvent);
  }
}

var gActionDescrMap =
{
  jump: "Jump",
  press: "Press",
  check: "Check",
  uncheck: "Uncheck",
  select: "Select",
  open: "Open",
  close: "Close",
  switch: "Switch",
  click: "Click",
  collapse: "Collapse",
  expand: "Expand",
  activate: "Activate",
  cycle: "Cycle"
};
