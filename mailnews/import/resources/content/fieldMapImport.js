var importService = 0;
var fieldMap = null;
var transferType = null;
var recordNum = 0;
var amAtEnd = false;
var addInterface = null;
var dialogResult = null;
var gListbox;

function OnLoadFieldMapImport()
{
  top.importService = Components.classes["@mozilla.org/import/import-service;1"].getService();
  top.importService = top.importService.QueryInterface(Components.interfaces.nsIImportService);
  top.transferType = "moz/fieldmap";
  top.recordNum = 0;

  // We need a field map object...
  // assume we have one passed in? or just make one?
  if (window.arguments && window.arguments[0]) {
    top.fieldMap = window.arguments[0].fieldMap;
    top.addInterface = window.arguments[0].addInterface;
    top.dialogResult = window.arguments[0].result;
  }
  if (top.fieldMap == null) {
    top.fieldMap = top.importService.CreateNewFieldMap();
    top.fieldMap.DefaultFieldMap( top.fieldMap.numMozFields);
  }

  doSetOKCancel( FieldImportOKButton, 0);

  gListbox = document.getElementById("fieldList");
  ListFields();
  OnNextRecord();

  // childNodes includes the listcols and listhead
  gListbox.selectedItem = gListbox.childNodes[2];
  window.sizeToContent();
}

function IndexInMap( index)
{
  var count = top.fieldMap.mapSize;
  var i;
  for (i = 0; i < count; i++) {
    if (top.fieldMap.GetFieldMap( i) == index)
      return( true);
  }

  return( false);
}

function ListFields() {
  if (top.fieldMap == null)
    return;

  var count = top.fieldMap.mapSize;
  var index;
  var i;
  for (i = 0; i < count; i++) {
    index = top.fieldMap.GetFieldMap( i);
    AddFieldToList( gListbox, top.fieldMap.GetFieldDescription( index), index, top.fieldMap.GetFieldActive( i));
  }

  count = top.fieldMap.numMozFields;
  for (i = 0; i < count; i++) {
    if (!IndexInMap( i))
      AddFieldToList( gListbox, top.fieldMap.GetFieldDescription( i), i, false);
  }
}

function CreateField( name, index, on)
{
  var item = document.createElement('listitem');
  item.setAttribute('field-index', index);
  var cell = document.createElement('listcell');
  var cCell = document.createElement( 'listcell');  
  cCell.setAttribute('type', "checkbox");
  cCell.setAttribute( 'label', name);
  if (on == true)
    cCell.setAttribute( 'checked', "true");
  item.appendChild( cCell);
  cell.setAttribute( "class", "importsampledata");
  cell.setAttribute( 'label', " ");
  item.appendChild( cell);
  return( item);
}

function AddFieldToList(list, name, index, on)
{
  var item = CreateField(name, index, on);
  list.appendChild(item);
}

function itemSelected()
{
  if (gListbox.selectedIndex < 0)
    return;
  var on = gListbox.selectedItem.firstChild.getAttribute('checked');
  if (on == "true")
    gListbox.selectedItem.firstChild.setAttribute('checked', "false");
  else
    gListbox.selectedItem.firstChild.setAttribute('checked', "true");
}

// the "Move Up/Move Down" buttons should move the items in the left column
// up/down but the values in the right column should not change.

function moveUpListItem()
{

  // childNodes and removeItem includes the listcols and listhead
  // while the selectedIndex and ensureIndexIsVisible doesnot.
  var selectedIndex = gListbox.selectedIndex;
  if (selectedIndex < 1)
    return;

  // get the attributes of the selectedItem and remove it.
  var name = gListbox.selectedItem.firstChild.getAttribute('label');
  var selectedItemValue = gListbox.selectedItem.childNodes[1].getAttribute('label');
  var index = gListbox.selectedItem.getAttribute('field-index');
  var on = gListbox.selectedItem.firstChild.getAttribute('checked');
  gListbox.removeItemAt(selectedIndex + 2);

  // create a new item with the left column fields set.
  var item;
  if (on == "true")
    item = CreateField(name, index, true);
  else
    item = CreateField(name, index, false);

  // previousItem is the item above the selectedItem.
  // the index of the previous item is selectedIndex+1 instead of 
  // selectedIndex-1 because, the childNodes include listcols and listhead
  // which makes it selectedIndex -1 + 2 = selectedIndex+1
  // Insert the newly created item before the previousItem.
  var previousItem = gListbox.childNodes[selectedIndex+1];
  var previousItemValue = previousItem.childNodes[1].getAttribute('label');
  gListbox.insertBefore(item, previousItem);

  // set the selectedItem and make sure it is visible.
  gListbox.ensureIndexIsVisible(selectedIndex-1);
  gListbox.selectedItem = gListbox.childNodes[selectedIndex+1];

  // set the values in the right column such that the selectedItem has the value at that index.
  gListbox.childNodes[selectedIndex+2].childNodes[1].setAttribute('label', selectedItemValue);
  gListbox.selectedItem.childNodes[1].setAttribute('label', previousItemValue);
}

function moveDownListItem()
{
  // childNodes and removeItem includes the listcols and listhead
  // while the selectedIndex and ensureIndexIsVisible doesnot.
  var selectedIndex = gListbox.selectedIndex;
  if (selectedIndex < 0 || (selectedIndex > (gListbox.childNodes.length - 4)))
    return;

  // get the attributes of the selectedItem and remove it.
  var name = gListbox.selectedItem.firstChild.getAttribute('label');
  var selectedItemName = gListbox.selectedItem.childNodes[1].getAttribute('label');
  var index = gListbox.selectedItem.getAttribute('field-index');
  var on = gListbox.selectedItem.firstChild.getAttribute('checked');
  gListbox.removeItemAt(selectedIndex + 2);

  // create a new item with the left column fields set.
  var item;
  if (on == "true")
    item = CreateField(name, index, true);
  else
    item = CreateField(name, index, false);

  // nextItemValue is the value of the item below the selectedItem.
  // the index of the nextitem is selectedIndex+2 instead of 
  // selectedIndex+1 because, the childNodes include listcols and listhead 
  // which makes it selectedIndex + 3 and we removed the  selected item
  // which make it selectedIndex + 2
  var nextItemValue = gListbox.childNodes[selectedIndex+2].childNodes[1].getAttribute('label');

  // now we need to insert the newly created item after the selectedIndex+2
  // i.e. before selectedIndex+3
  // Check if the item we are trying to move down is last but one item
  // then append the item at the end.
  if (selectedIndex > gListbox.childNodes.length-4) {
    gListbox.appendChild(item);
  }
  else {
    gListbox.insertBefore(item, gListbox.childNodes[selectedIndex+3]);
  }

  // set the selectedItem and make sure it is visible.
  gListbox.ensureIndexIsVisible(selectedIndex+1);
  gListbox.selectedItem = gListbox.childNodes[selectedIndex+3];

  // set the values in the right column such that the selectedItem has the value at that index.
  gListbox.childNodes[selectedIndex+2].childNodes[1].setAttribute('label', selectedItemName);
  gListbox.selectedItem.childNodes[1].setAttribute('label', nextItemValue);
}

function ShowSampleData( data)
{
  var fields = data.split( "\n");
  // childNodes includes the listcols and listhead
  for (var i = 0; i < gListbox.childNodes.length - 2; i++) {
    if (i < fields.length) {
      gListbox.childNodes[i+2].childNodes[1].setAttribute( 'label', fields[i]);
    }
    else {
      gListbox.childNodes[i+2].childNodes[1].setAttribute( 'label', " ");
    }
  }

}

function FetchSampleData()
{
  if (!top.addInterface)
    return( false);

  var num = top.recordNum - 1;
  if (num < 0)
    num = 0;
  var data = top.addInterface.GetData( "sampleData-"+num);
  var result = false;
  if (data != null) {
    data = data.QueryInterface( Components.interfaces.nsISupportsString);
    if (data != null) {
      ShowSampleData( data.data);
      result = true;
    }
  }

  return( result);
}

function OnPreviousRecord()
{
  if (top.recordNum <= 1)
    return;
  top.recordNum--;
  top.amAtEnd = false;
  if (FetchSampleData()) {
    document.getElementById('recordNumber').setAttribute('value', ("" + top.recordNum));
  }
}

function OnNextRecord()
{
  if (top.amAtEnd)
    return;
  top.recordNum++;
  if (!FetchSampleData()) {
    top.amAtEnd = true;
    top.recordNum--;
  }
  else
    document.getElementById('recordNumber').setAttribute('value', ("" + top.recordNum));
}

function FieldImportOKButton()
{
  // childNodes includes the listcols and listhead
  var max = gListbox.childNodes.length - 2;
  var fIndex;
  var on;
  for (var i = 0; i < max; i++) {
    fIndex = gListbox.childNodes[i+2].getAttribute( 'field-index');
    on = gListbox.childNodes[i+2].firstChild.getAttribute('checked');
    top.fieldMap.SetFieldMap( i, fIndex);
    if (on == "true")
      top.fieldMap.SetFieldActive( i, true);
    else
      top.fieldMap.SetFieldActive( i, false);
  }

  top.dialogResult.ok = true;

  return true;
}
