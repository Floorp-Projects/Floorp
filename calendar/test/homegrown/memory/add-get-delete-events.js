// create some events
var item1 = createItem();
item1.title = "Test Event 1";
var item2 = createItem();
item2.title = "Test Event 2";

// add them to the calendar
var newItem1 = addItem(item1);
dump("newItem1 = " + newItem1 + "\n");
var newItem2 = addItem(item2);
dump("newItem2 = " + newItem2 + "\n");

// XXX what should happen untyped items?

// XXX compare newItems to items

// get them from the calendar
var gottenItems = getItems(Ci.calICalendar.ITEM_FILTER_COMPLETED_ALL | 
			   Ci.calICalendar.ITEM_FILTER_TYPE_EVENT, 0, null, 
			   null);

dump("gottenItems = " + gottenItems + "\n");

// XXX compare gottenItem to item

// delete it from the calendar
var deletedItem1 = deleteItem(gottenItems[0]);
dump("deletedItem1 = " + deletedItem1 + "\n");
var deletedItem2 = deleteItem(gottenItems[1]);
dump("deletedItem2 = " + deletedItem2 + "\n");

// XXX compare deletedItem to gottenItem

// XXX make sure getting and deleting again both fail



