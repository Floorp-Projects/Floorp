// create an event
var item = createItem();

item.title = "Test Event";

// add it to the calendar
var newItem = addItem(item);
dump("newItem = " + newItem + "\n");

// XXX what should happen to untyped item?

// XXX compare newItem to item

// get it from the calendar
var gottenItem = getItem(newItem.id);
dump("gottenItem = " + gottenItem + "\n");

// XXX compare gottenItem to item

// delete it from the calendar
var deletedItem = deleteItem(gottenItem);
dump("deletedItem = " + deletedItem + "\n");

// XXX compare deletedItem to gottenItem

// XXX make sure getting and deleting again both fail
