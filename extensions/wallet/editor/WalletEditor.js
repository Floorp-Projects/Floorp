/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* universal global variables */

var walleteditor = null; // walleteditor interface
var walletList = []; // input stream
var bundle = null; // string bundle
var JS_STRINGS_FILE = "chrome://wallet/locale/WalletEditor.properties";

/* wallet-specific global variables */
    
var schemas = [];
var schemasLength;
var entries = [];
var entriesLength;
var strings = [];
var stringsLength;
var BREAK;

/*** =================== ?? =================== ***/

function ViewEntriesFromXul(){
  ViewEntries();
}
function ViewSynonymsFromXul(){
  ViewSynonyms();
}

/*** =================== STARTING AND STOPPING =================== ***/

/* initializes the wallet editor dialog */
function Startup()
{
  walleteditor = Components.classes["component://netscape/walleteditor/walleteditor-world"].createInstance();
  walleteditor = walleteditor.QueryInterface(Components.interfaces.nsIWalletEditor);

  bundle = srGetStrBundle(JS_STRINGS_FILE); /* initialize string bundle */
  if (!FetchInput()) {
    return; /* user failed to unlock the database */
  }
  ViewSchema(); /* create the display of schemas */
  doSetOKCancel(onOK, null); /* register the onOK method */
  window.sizeToContent();
}

/* routine that executes when OK button is pressed */
function onOK(){
  var i, j, k;
  var output = "OK" + BREAK;
  for (i=0; i<schemasLength; i++) {
    for (j=schemas[i]; j<schemas[i+1]; j++) {
      for (k=entries[j]; k<entries[j+1]; k++) {
        output += strings[k] + BREAK;
      }
    }
  }
  walleteditor.SetValue(output, window);
  return true;
}

/* get the wallet input data */
function FetchInput()
{
  /*  get wallet data into a list */
  list = walleteditor.GetValue();

  /* format of this list is as follows:
   *
   *    BREAK-CHARACTER
   *    schema-name BREAK-CHARACTER
   *    value BREAK-CHARACTER
   *    synonymous-value-1 BREAK-CHARACTER
   *    ...
   *    synonymous-value-n BREAK-CHARACTER
   *
   * and repeat above pattern for each schema name.  Note that if there are more than
   * one distinct values for a particular schema name, the above pattern is repeated
   * for each such distinct value
   */

  /* check for database being unlocked */
  if (list.length == 0) {
    /* user supplied invalid database key */
    window.close();
    return false;
  }

  /* parse the wallet data list into the stings, entries, and schemas arrays */
  BREAK = list[0];
  FlushTables();
  strings = list.split(BREAK);
  stringsLength = strings.length;
  var i, j;
  j = 0;
  for (i=1; i<stringsLength; i++) {
    if (strings[i] != "") {
      if(strings[i-1] == "") {
        entries[j++] = i;
        entriesLength++;
      }
    }
  }
  entries[j] = stringsLength;
  j = 0;
  for (i=0; i<entriesLength; i++) {
    if (i == 0 || (strings[entries[i]] != strings[entries[i-1]])) {
      schemas[j++] = i;
      schemasLength++;
    }
  }
  schemas[j] = entriesLength;

  return true;
}


/*** =================== SELECTING AND DESELECTING ITEMS =================== ***/

/* clear the list of selected schemas */
var selectedSchemas = [];
function ClearSelectedSchemas() {
  selectedSchemas = [];
  SetCurrentSchema(0);
}

/* clear the list of selected entries */
var selectedEntries = [];
function ClearSelectedEntries() {
  selectedEntries = [];
  SetCurrentEntry(0);
}

/* clear the list of selected synonyms */
var selectedSynonyms = [];
function ClearSelectedSynonyms() {
  selectedSynonyms = [];
  SetCurrentSynonym(0);
}

/* initialize the list of selected schemas */
function InitSelectedSchemas() {
  ClearSelectedSchemas();
  var schematree = document.getElementById("schematree");
  var selitems = schematree.selectedItems;
  for(var i = 0; i < selitems.length; i++) {
    selectedSchemas[i] = schematree.selectedItems[i];
  }
  var sorted = false;
  while (!sorted) {
    sorted = true;
    for (var i=0; i < selectedSchemas.length-1; i++) {
      if (selectedSchemas[i].getAttribute("id") <
          selectedSchemas[i+1].getAttribute("id")) {
        temp = selectedSchemas[i+1];
        selectedSchemas[i+1] = selectedSchemas[i];
        selectedSchemas[i] = temp;
        sorted = false;
      }
    }
  }
  return selitems.length;
}

/* initialize the list of selected entries */
function InitSelectedEntries() {
  ClearSelectedEntries();
  var entrytree = document.getElementById("entrytree");
  var selitems = entrytree.selectedItems;
  for(var i = 0; i < selitems.length; i++) { 
    selectedEntries[i] = entrytree.selectedItems[i];
  }
  var sorted = false;
  while (!sorted) {
    sorted = true;
    for (var i=0; i < selectedEntries.length-1; i++) {
      if (selectedEntries[i].getAttribute("id") <
          selectedEntries[i+1].getAttribute("id")) {
        temp = selectedEntries[i+1];
        selectedEntries[i+1] = selectedEntries[i];
        selectedEntries[i] = temp;
        sorted = false;
      }
    }
  }
  return selitems.length;
}

/* initialize the list of selected synonyms */
function InitSelectedSynonyms() {
  ClearSelectedSynonyms();
  var synonymtree = document.getElementById("synonymtree");
  var selitems = synonymtree.selectedItems;
  for(var i = 0; i < selitems.length; i++) { 
    selectedSynonyms[i] = synonymtree.selectedItems[i];
  }
  var sorted = false;
  while (!sorted) {
    sorted = true;
    for (var i=0; i < selectedSynonyms.length-1; i++) {
      if (selectedSynonyms[i].getAttribute("id") <
          selectedSynonyms[i+1].getAttribute("id")) {
        temp = selectedSynonyms[i+1];
        selectedSynonyms[i+1] = selectedSynonyms[i];
        selectedSynonyms[i] = temp;
        sorted = false;
      }
    }
  }
  return selitems.length;
}

/* get the indicated selected schema */
function SelectedSchema(i) {
  if (selectedSchemas.length <= i) {
    return 0;
  }
  var id = selectedSchemas[i].getAttribute("id");
  return parseInt(id.substring(5, id.length));
}

/* get the indicated selected entry */
function SelectedEntry(i) {
  if (selectedEntries.length <= i) {
    return 0;
  }
  var id = selectedEntries[i].getAttribute("id");
  return parseInt(id.substring(5, id.length));
}

/* get the indicated selected synomym */
function SelectedSynonym(i) {
  if (selectedSynonyms.length <= i) {
    return 0;
  }
  var id = selectedSynonyms[i].getAttribute("id");
  return parseInt(id.substring(5, id.length));
}

/* get the first selected schema */
function FirstSelectedSchema() {
  return SelectedSchema(0);
}

/* get the first selected entry */
function FirstSelectedEntry() {
  return SelectedEntry(0);
}

/* get the first selected synonym */
function FirstSelectedSynonym() {
  return SelectedSynonym(0);
}

/* set current schema */
var currentSchema = 0
function SetCurrentSchema(schema) {
  currentSchema = schema;
}

/* set current entry */
var currentEntry = 0;
function SetCurrentEntry(entry) {
  currentEntry = entry;
}

/* set current synonym */
var currentSynonym = 0;
function SetCurrentSynonym(synonym) {
  currentSynonym = synonym;
}

/* get current schema */
function CurrentSchema() {
  return currentSchema;
}

/* get current entry */
function CurrentEntry() {
  return currentEntry;
}

/* get current synonym */
function CurrentSynonym() {
  return currentSynonym;
}

/* a schema has just been selected */
function SchemaSelected()
{
  document.getElementById("removeSchema").setAttribute("disabled", "false")
  document.getElementById("addEntry").setAttribute("disabled", "false")
}

/* an entry has just been selected */
function EntrySelected()
{
  document.getElementById("removeEntry").setAttribute("disabled", "false")
  document.getElementById("addSynonym").setAttribute("disabled", "false")
}

/* a synonym has just been selected */
function SynonymSelected()
{
  var synonymtree = document.getElementById("synonymtree");
  if(synonymtree.selectedItems.length) {
    document.getElementById("removeSynonym").setAttribute("disabled", "false")
  }
}

/* a schema has just been deselected */
function SchemaDeselected()
{
  document.getElementById("removeSchema").setAttribute("disabled", "true")
  document.getElementById("addEntry").setAttribute("disabled", "true")
}

/* an entry has just been deselected */
function EntryDeselected()
{
  document.getElementById("removeEntry").setAttribute("disabled", "true")
  document.getElementById("addSynonym").setAttribute("disabled", "true")
}

/* a synonym has just been deselected */
function SynonymDeselected()
{
  document.getElementById("removeSynonym").setAttribute("disabled", "true")
}

/*** =================== VIEW VARIOUS LISTS =================== ***/

/* display the schemas */
function ViewSchema()
{
  ClearList("schemalist");
  SchemaDeselected();
  ClearList("entrieslist");
  EntryDeselected();
  ClearList("synonymslist");
  SynonymDeselected();
  var i;
  for(i = 0; i < schemasLength; i++)
  {
    AddItem("schemalist", [strings[entries[schemas[i]]]], "tree_", i); 
  }
}

/* display the entries when a schema has been selected */
function ViewEntries()
{
  ClearList("entrieslist");
  EntryDeselected();
  ClearList("synonymslist");
  SynonymDeselected();
  var i;
  var schematree = document.getElementById("schematree");
  InitSelectedSchemas();
  if(schematree.selectedItems.length) {
    var first = schemas[FirstSelectedSchema()];
    var lastPlusOne = schemas[FirstSelectedSchema()+1];
    for (i=first; i<lastPlusOne; i++) {
      if (strings[entries[i]+1] != "") {
        AddItem("entrieslist", [strings[entries[i]+1]], "tree_", i-first); 
      }
    }
    SchemaSelected();
  }
}

/* display the synonyms when an entry has been selected */
function ViewSynonyms()
{
  ClearList("synonymslist");
  SynonymDeselected();
  var i;
  var entrytree = document.getElementById("entrytree");
  InitSelectedSchemas();
  InitSelectedEntries();
  if(entrytree.selectedItems.length) {
    var first = entries[schemas[FirstSelectedSchema()]+FirstSelectedEntry()]+2;
    var lastPlusOne = entries[schemas[FirstSelectedSchema()]+FirstSelectedEntry()+1]-1;
    for (i=first; i<lastPlusOne; i++) {
      AddItem("synonymslist", [strings[i]], "tree_", i-first);
    }
    EntrySelected();
  }
}

/*** =================== ADDING AND DELETING ITEMS =================== ***/

/* clear all wallet tables */
function FlushTables() {
  strings[0] = "";
  strings[1] = "";
  entries[0] = 0;
  entries[1] = 2;
  schemas[0] = 0;
  schemas[1] = 0;
  schemasLength = 0;
  entriesLength = 0;
  stringsLength = 0;
}

/* low-level delete-schema routine */
function DeleteSchema0() {
  var i;
  if (schemasLength == 1) {
    FlushTables();
    return;
  }

  numberOfEntries = schemas[CurrentSchema()+1] - schemas[CurrentSchema()];
  for (i=0; i<numberOfEntries; i++) {
    DeleteEntry0();
  }

  deleteString(entries[schemas[CurrentSchema()]]+1); /* delete blank line */
  deleteString(entries[schemas[CurrentSchema()]]); /* delete name of schema */
  entriesLength--;
  for (i=schemas[CurrentSchema()]; i<=entriesLength; i++) {
    entries[i] = entries[i+1];
  }
  for (i=0; i<=schemasLength; i++) {
    if (schemas[i] > entryToDelete) {
      schemas[i]--;
    }
  }

  schemasLength--;
  for (i=CurrentSchema(); i<=schemasLength; i++) {
    schemas[i] = schemas[i+1];
  }
}

/* low-level delete-entry routine */
function DeleteEntry0() {
  var i;
  entryToDelete = schemas[CurrentSchema()]+CurrentEntry();
  while (entries[entryToDelete]+2 < entries[entryToDelete+1]-1) {
    DeleteSynonym0();
  }

  if ((schemas[CurrentSchema()+1] - schemas[CurrentSchema()]) == 1) {
    if(strings[entries[entryToDelete]+1] != "") {
      deleteString(entries[entryToDelete]+1);
    }
    return;
  }

  while(strings[entries[entryToDelete]] != "") {
    deleteString(entries[entryToDelete]);
  }
  deleteString(entries[entryToDelete]);

  entriesLength--;
  for (i=entryToDelete; i<=entriesLength; i++) {
    entries[i] = entries[i+1];
  }

  for (i=0; i<=schemasLength; i++) {
    if (schemas[i] > entryToDelete) {
      schemas[i]--;
    }
  }
}

/* low-level delete-synonym routine */
function DeleteSynonym0() {
  stringToDelete = entries[schemas[CurrentSchema()]+CurrentEntry()]+2+CurrentSynonym();
  deleteString(stringToDelete);
}

/* low-level add-schema routine */
function AddSchema0() {
  var i;
  var text = prompt(bundle.GetStringFromName("EnterNewSchema"), "" );
  if (text == "") {
    return;
  }
  schemaIndex = 0;
  while ((schemaIndex<schemasLength) &&strings[entries[schemas[schemaIndex]]] < text) {
    schemaIndex++;
  }

  schemasLength++;
  for (i=schemasLength; i>schemaIndex; i--) {
    schemas[i] = schemas[i-1]+1;
  }

  entryIndex = schemas[schemaIndex];
  entriesLength++;
  for (i=entriesLength; i>entryIndex; i--) {
    entries[i] = entries[i-1];
  }
  
  stringIndex = entries[entryIndex];
  if (stringIndex == stringsLength) {
    stringIndex--;
  }

  addString(stringIndex, text);
  addString(stringIndex+1, "");
  schemas[schemaIndex] = entryIndex;
  entries[entryIndex] = stringIndex;
}

/* low-level add-entry routine */ 
function AddEntry0() {
  var i;
  var text = prompt(bundle.GetStringFromName("EnterNewEntry"), "" );
  if (text == "") {
    return;
  }
  stringIndex = entries[schemas[FirstSelectedSchema()]+FirstSelectedEntry()];
  if(strings[entries[schemas[FirstSelectedSchema()]+FirstSelectedEntry()]+1]=="") {
    addString(entries[schemas[FirstSelectedSchema()]+FirstSelectedEntry()]+1, text);
    return;
  }

  addString(stringIndex, strings[entries[schemas[FirstSelectedSchema()]]]);
  addString(stringIndex+1, text);
  addString(stringIndex+2, "");

  entriesLength++;
  for (i=entriesLength; i>schemas[FirstSelectedSchema()]+FirstSelectedEntry(); i--) {
    entries[i] = entries[i-1];
  }
  entries[schemas[FirstSelectedSchema()]+FirstSelectedEntry()] = stringIndex;

  for (i=FirstSelectedSchema()+1; i<=schemasLength; i++) {
    schemas[i]++;
  }
}

/* low-level add-synonym routine */
function AddSynonym0() {
  var text = prompt(bundle.GetStringFromName("EnterNewSynonym"), "" );
  if (text == "") {
    return;
  }
  addString(entries[schemas[FirstSelectedSchema()]+FirstSelectedEntry()]+2, text);
}

function deleteString(stringToDelete) {
  var i;
  stringsLength--;
  for (i=stringToDelete; i<stringsLength; i++) {
    strings[i] = strings[i+1];
  }
  for (i=0; i<=entriesLength; i++) {
    if (entries[i] > stringToDelete) {
      entries[i]--;
    }
  }
}

function addString(stringToAdd, text) {
  var i;
  stringsLength++;
  for (i=stringsLength; i>stringToAdd; i--) {
    strings[i] = strings[i-1];
  }
  strings[stringToAdd] = text;
  for (i=0; i<=entriesLength; i++) {
    if (entries[i] >= stringToAdd) {
      entries[i]++;
    }
  }
}

/* high-level add-schema routine */
function AddSchema() {
  var button = document.getElementById("addSchema");
  if( button.getAttribute("disabled") == "true" ) {
    return;
  }

  InitSelectedSchemas();
  ClearSelectedEntries();
  ClearSelectedSynonyms();
  AddSchema0();
  ViewSchema(); //?? otherwise schema list doesn't get redrawn
}

/* high-level add-entry routine */
function AddEntry() {
  var button = document.getElementById("addEntry");
  if( button.getAttribute("disabled") == "true" ) {
    return;
  }

  InitSelectedSchemas();
  InitSelectedEntries();
  ClearSelectedSynonyms();
  AddEntry0();
  ViewEntries(); //?? otherwise entry list doesn't get redrawn
}

/* high-level add-synonym routine */
function AddSynonym() {
  var button = document.getElementById("addSynonym");
  if( button.getAttribute("disabled") == "true" ) {
    return;
  }

  InitSelectedSchemas();
  InitSelectedEntries();
  InitSelectedSynonyms();
  AddSynonym0();
  ViewSynonyms(); //?? otherwise synonym list doesn't get redrawn
  ViewSynonyms(); //?? otherwise entry list doesn't get redrawn (??even needed twice)
}

/* high-level delete-schema routine */
function DeleteSchema() {
  var button = document.getElementById("removeSchema");
  if( button.getAttribute("disabled") == "true" ) {
    return;
  }
  var count = InitSelectedSchemas();
  ClearSelectedEntries();
  ClearSelectedSynonyms();
  for (var i=0; i<count; i++) {
    SetCurrentSchema(SelectedSchema(i));
    DeleteSchema0();
  }
  ClearList("entrieslist");
  ClearList("synonymslist");
  ViewSchema();
}

/* high-level delete-entry routine */
function DeleteEntry() {
  var button = document.getElementById("removeEntry");
  if( button.getAttribute("disabled") == "true" ) {
    return;
  }
  InitSelectedSchemas();
  SetCurrentSchema(SelectedSchema(0));
  var count = InitSelectedEntries();
  ClearSelectedSynonyms();
  for (var i=0; i<count; i++) {
    SetCurrentEntry(SelectedEntry(i));
    DeleteEntry0();
  }
  ClearList("synonymslist");
  ViewEntries();
}

/* high-level delete-synonym routine */
function DeleteSynonym() {
  var button = document.getElementById("removeSynonym");
  if( button.getAttribute("disabled") == "true" ) {
    return;
  }
  InitSelectedSchemas();
  SetCurrentSchema(SelectedSchema(0));
  InitSelectedEntries();
  SetCurrentEntry(SelectedEntry(0));
  var count = InitSelectedSynonyms();
  for (var i=0; i<count; i++) {
    SetCurrentSynonym(SelectedSynonym(i));
    DeleteSynonym0();
  }
  ViewSynonyms();
}

/*** =================== DEBUGGING CODE =================== ***/

/* debugging routine to dump formatted strings */
function DumpStrings() {
  var i, j, k;
  for (i=0; i<schemasLength; i++) {
    dump("<<"+i+" "+schemas[i]+" "+entries[schemas[i]]+" "+strings[entries[schemas[i]]] +">>\n");
    for (j=schemas[i]; j<schemas[i+1]; j++) {
      dump("<<     " + strings[entries[j]+1] +">>\n");
      for (k=entries[j]+2; k<entries[j+1]-1; k++) {
        dump("<<....." + strings[k] +">>\n");
      }
    }
  }
  dump("\n");
}

/* debugging routine to dump raw strings */
function DumpRawStrings() {
  var i;
  dump("Schemas follow\n");
  for (i=0; i<schemasLength; i++) {
    dump("{" + schemas[i] + "}");
  }
  dump("[" + schemas[schemasLength] + "]");
  dump("\nEntries follow\n");
  for (i=0; i<entriesLength; i++) {
    dump("{" + entries[i] + "}");
  }
  dump("[" + entries[entriesLength] + "]");
  dump("\nStrings follow\n");
  for (i=0; i<stringsLength; i++) {
    dump("{" + strings[i] + "}");
  }
  dump("\n");
}

/*** =================== TREE MANAGEMENT CODE =================== ***/

/* add item to a tree */
function AddItem(children,cells,prefix,idfier)
{
  var kids = document.getElementById(children);
  var item  = document.createElement("treeitem");
  var row   = document.createElement("treerow");
  for(var i = 0; i < cells.length; i++)
  {
    var cell  = document.createElement("treecell");
    cell.setAttribute("class", "propertylist");
    cell.setAttribute("value", cells[i])
    row.appendChild(cell);
  }
  item.appendChild(row);
  item.setAttribute("id",prefix + idfier);
  kids.appendChild(item);
}

/* clear out a tree */
function ClearList(kids) {
  var list = document.getElementById(kids);
  while (list.firstChild) {
    list.removeChild(list.firstChild);
  }
}