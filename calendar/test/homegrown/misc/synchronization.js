/*
 * This is the memory hog version. Read all events in memory, then sync
 * from there.
 * Doing it the right async way gives me too many headaches.
 */

const calICalendar = Components.interfaces.calICalendar;

var emptyListener =
{
  onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail) {},
  onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {}
};

var localcal = Components.classes["@mozilla.org/calendar/calendar;1?type=memory"]
                         .createInstance(Components.interfaces.calICalendar);
var remotecal = Components.classes["@mozilla.org/calendar/calendar;1?type=memory"]
                          .createInstance(Components.interfaces.calICalendar);
var changecal = Components.classes["@mozilla.org/calendar/calendar;1?type=memory"]
                          .createInstance(Components.interfaces.calICalendar);


// local: B CC
// change: A, B, C
// remote: A, C
// expected: B, CC
createEvent("i2", "B", localcal);
createEvent("i3", "CC", localcal);
createEvent("i1", "A", remotecal);
createEvent("i3", "C", remotecal);
createEvent("i1", "A", changecal);
createEvent("i2", "B", changecal);
createEvent("i3", "C", changecal);

dump("Before\n");
dumpCals();

// Start by getting all the items into an array
var localItems = [];
var remoteItems = [];
var changeItems = [];

// This will get the items from the calendars into the global arrays,
// then call doSync()
getCalandarItems();

function doSync()
{
  /*
   * forall changeEvents
   *   if (!remoteEvent)
   *     remoteCalendar.add(localEvent);
   *   else if (remoteEvent != changedEvent)
   *     conflict();
   *   else if (!localEvent)
   *     remoteCalendar.deleteEvent(changedEvent.id)
   */

  for (id in changeItems) {
    if (!remoteItems[id])
      remotecal.addItem(localItems[id].clone(), emptyListener);
    else if (!itemsAreEqual(remoteItems[id], changeItems[id]))
      dump("Conflict! wheep! "+remoteItems[id].title+" != "+changeItems[id].title+"\n");
    else if (!localItems[id]) {
      remotecal.deleteItem(remoteItems[id], emptyListener);
      delete remoteItems[id];
    }
  }

  //dump("Halfway\n");
  //dumpCals();

  // now for part two: remote to local

  /*
   * forall remoteEvents
   *   if (!localEvent)
   *     localCalendar.add(remoteEvent);
   *   else if (localEvent != remoteEvent)
   *     if (!changeEvent)
   *       localCalendar.modify(localEvent.id, remoteEvent);
   *     else if (changeEvent == remoteEvent)
   *       remoteCalendar.modify(remoteEvent.id, localEvent);
   *     else
   *       conflict();
   */

  for (id in remoteItems) {
    if (!localItems[id])
      localcal.addItem(remoteItems[i].clone(), emptyListener);
    if (!itemsAreEqual(localItems[id], remoteItems[id])) {
      if (!changeItems[id]) {
        copyItems(remoteItems[id], localItems[id]);
        localcal.modifyItem(localItems[id], emptyListener);
      } else if (itemsAreEqual(remoteItems[id], changeItems[id])) {
        copyItems(localItems[id], remoteItems[id]);
        remotecal.modifyItem(remoteItems[id], emptyListener);
      } else {
        // There is a conflict, but that was already detected before when
        // dealing with locally changed items. No need to do that again.
        //dump("Conflict\n");
      }

    }
  }

  dump("After\n");
  dumpCals();
}


function itemsAreEqual(itemA, itemB)
{
  return (itemA.id == itemB.id && itemA.title == itemB.title)
}

function copyItems(aSource, aDest)
{
  aDest.title = aSource.title;
}

function createEvent(aID, aTitle, aCal)
{
  var event = Components.classes["@mozilla.org/calendar/event;1"]
                        .createInstance(Components.interfaces.calIEvent);
  event.title = aTitle;
  event.id = aID;
  aCal.addItem(event, emptyListener);
}

function getCalandarItems()
{
  var calendarsFinished = 0;

  var getListener =
  {
    onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail)
    {
      calendarsFinished++;
      if (calendarsFinished == 3) {
        doSync();
      }
    },
    onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems)
    {
      if (aCount) {
        var ar;
        if (aCalendar == localcal)
          items = localItems;
        else if (aCalendar == remotecal)
          items = remoteItems;
        else if (aCalendar == changecal)
          items = changeItems;

        for (var i=0; i<aCount; ++i) {
          items[aItems[i].id] = aItems[i];
        }  
      }
    }
  };

  localcal.getItems(calICalendar.ITEM_FILTER_TYPE_ALL | calICalendar.ITEM_FILTER_COMPLETED_ALL,
                    0, null, null, getListener);
  remotecal.getItems(calICalendar.ITEM_FILTER_TYPE_ALL | calICalendar.ITEM_FILTER_COMPLETED_ALL,
                    0, null, null, getListener);
  changecal.getItems(calICalendar.ITEM_FILTER_TYPE_ALL | calICalendar.ITEM_FILTER_COMPLETED_ALL,
                    0, null, null, getListener);
}


function dumpCals()
{
  var dumpListener =
  {
    onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail) {},
    onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems)
    {
      for (var i=0; i<aCount; ++i) {
        dump("  "+i+" "+aItems[i].id+" "+aItems[i].title+"\n");
      }
    }
  };

  dump(" Local: \n");
  localcal.getItems(calICalendar.ITEM_FILTER_TYPE_ALL | calICalendar.ITEM_FILTER_COMPLETED_ALL,
                     0, null, null, dumpListener);
  dump("\n Remote: \n");
  remotecal.getItems(calICalendar.ITEM_FILTER_TYPE_ALL | calICalendar.ITEM_FILTER_COMPLETED_ALL,
                     0, null, null, dumpListener);
  dump("\n");
}

