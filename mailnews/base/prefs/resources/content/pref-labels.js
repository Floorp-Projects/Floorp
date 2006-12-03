/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SeaMonkey Internet Suite Code.
 *
 * The Initial Developer of the Original Code is the SeaMonkey project.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Karsten Düsterloh <mnyromyr@tprac.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Each tag entry in our list looks like this, where <key> is tag's unique key:
//    <listitem>
//      <listcell>
//        <textbox/>
//      </listcell>
//      <listcell>
//        <colorpicker type='button'/>
//      </listcell>
//    </listitem>
const TAGPANEL_URI    = 'chrome://messenger/content/pref-labels.xul';
const TAGLIST_ID      = 'tagList';                // UI element
const ACTIVE_TAGS_ID  = TAGLIST_ID + '.active';   // wsm element
const DELETED_TAGS_ID = TAGLIST_ID + '.deleted';  // wsm element

var gTagList      = null;  // tagList root element
var gAddButton    = null;
var gDeleteButton = null;
var gRaiseButton  = null;
var gLowerButton  = null;

var gDeletedTags  = null;  // tags to be deleted by the tagService

// init global stuff before the wsm is used
function InitTagPanel()
{
  gTagList      = document.getElementById(TAGLIST_ID);
  gAddButton    = document.getElementById('addTagButton');
  gDeleteButton = document.getElementById('deleteTagButton');
  gRaiseButton  = document.getElementById('raiseTagButton');
  gLowerButton  = document.getElementById('lowerTagButton');
  UpdateButtonStates();
  parent.initPanel(TAGPANEL_URI);
}

function Startup()
{
  parent.hPrefWindow.registerOKCallbackFunc(OnOK);
}

// store pref values in the wsm
function GetFields(aPageData)
{
  // collect the tag definitions from the UI and store them in the wsm
  var tags = [];
  for (var entry = gTagList.firstChild; entry; entry = entry.nextSibling)
    if (entry.localName == 'listitem')
    {
      // update taginfo with current values from textbox and colorpicker
      UpdateTagInfo(entry.taginfo, entry);
      tags.push(entry.taginfo);
    }
  aPageData[ACTIVE_TAGS_ID] = tags;

  // store the list of tags to be deleted in the OKHandler
  aPageData[DELETED_TAGS_ID] = gDeletedTags;

  return aPageData;
}

// read pref values stored in the wsm
function SetFields(aPageData)
{
  var i, tags;
  // If the wsm has no tag data yet, get the list from the tag service.
  if (!(ACTIVE_TAGS_ID in aPageData))
  {
    var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                               .getService(Components.interfaces.nsIMsgTagService);
    var tagArray = tagService.getAllTags({});
    tags = aPageData[ACTIVE_TAGS_ID] = [];
    for (i = 0; i < tagArray.length; ++i)
    {
      // The nsMsgTag items are readonly, but we may need to change them.
      // And we don't care for the current ordinal strings, we'll create new
      // ones in the OKHandler if necessary
      var t = tagArray[i];
      tags.push({tag: t.tag, key: t.key, color: t.color});
    }
  }

  // now create the dynamic elements
  tags = aPageData[ACTIVE_TAGS_ID];

  // Listitems we append to the "end" of the listbox and would be rendered
  // outside the clipping area don't get their text and color set!
  // (See also 354065.)
  // So we stuff them in bottom-up... :-|
  var beforeTag = null;
  for (i = tags.length - 1; i >= 0; --i)
    beforeTag = AppendTagEntry(tags[i], beforeTag);

  // grab the list of tags to be deleted in the OKHandler
  gDeletedTags = (DELETED_TAGS_ID in aPageData) ? aPageData[DELETED_TAGS_ID] : {};
}

// read text and color from the listitem
function UpdateTagInfo(aTagInfo, aEntry)
{
  aTagInfo.tag   = aEntry.firstChild.firstChild.value;
  aTagInfo.color = aEntry.lastChild.lastChild.color;
}

// set text and color of the listitem
function UpdateTagEntry(aTagInfo, aEntry)
{
  aEntry.firstChild.firstChild.value = aTagInfo.tag;
  aEntry.lastChild.lastChild.color   = aTagInfo.color;
}

function AppendTagEntry(aTagInfo, aRefChild)
{
  // Creating a colorpicker dynamically in an onload handler is really sucky.
  // You MUST first set its type attribute (to select the correct binding), then
  // add the element to the DOM (to bind the binding) and finally set the color
  // property(!) afterwards. Try in any other order and fail... :-(
  var key = aTagInfo.key;

  var tagCell = document.createElement('listcell');
  var textbox = document.createElement('textbox');
  tagCell.appendChild(textbox);

  var colorCell = document.createElement('listcell');
  var colorpicker = document.createElement('colorpicker');
  colorpicker.setAttribute('type', 'button');
  colorCell.appendChild(colorpicker);

  var entry = document.createElement('listitem');
  entry.addEventListener('focus', OnFocus, true);
  entry.setAttribute('allowevents', 'true');  // activate textbox and colorpicker
  entry.taginfo = aTagInfo;
  entry.appendChild(tagCell);
  entry.appendChild(colorCell);

  gTagList.insertBefore(entry, aRefChild);
  UpdateTagEntry(aTagInfo, entry);
  return entry;
}

function OnFocus(aEvent)
{
  gTagList.selectedItem = this;
  UpdateButtonStates();
}

function FocusTagEntry(aEntry)
{
  // focus the entry's textbox
  gTagList.ensureElementIsVisible(aEntry);
  aEntry.firstChild.firstChild.focus();
}

function UpdateButtonStates()
{
  var entry = gTagList.selectedItem;
  // disable Delete if no selection
  gDeleteButton.disabled = !entry;
  // disable Raise if no selection or first entry
  gRaiseButton.disabled = !entry || !gTagList.getPreviousItem(entry, 1);
  // disable Lower if no selection or last entry
  gLowerButton.disabled = !entry || !gTagList.getNextItem(entry, 1);
}

function DisambiguateTag(aTag, aTagList)
{
  if (aTag in aTagList)
  {
    var suffix = 2;
    while (aTag + ' ' + suffix in aTagList)
      ++suffix;
    aTag += ' ' + suffix;
  }
  return aTag;
}

function AddTag()
{
  // Add a new tag to the UI here. It will be only be written to the
  // preference system only if the OKHandler is executed!

  // create unique tag name
  var dupeList = {}; // indexed by tag
  for (var entry = gTagList.firstChild; entry; entry = entry.nextSibling)
    if (entry.localName == 'listitem')
      dupeList[entry.firstChild.firstChild.value] = true;
  var tag = DisambiguateTag(gAddButton.getAttribute('defaulttagname'), dupeList);

  // create new tag list entry
  var tagInfo = {tag: tag, key: '', color: '', ordinal: ''};
  var refChild = gTagList.getNextItem(gTagList.selectedItem, 1);
  var newEntry = AppendTagEntry(tagInfo, refChild);
  FocusTagEntry(newEntry);
}

function DeleteTag()
{
  // Delete the selected tag from the UI here. If it was added during this
  // preference dialog session, we can drop it at once; if it was read from
  // the preferences system, we need to remember killing it in the OKHandler.
  var entry = gTagList.selectedItem;
  var key = entry.taginfo.key;
  if (key)
    gDeletedTags[key] = true; // dummy value
  // after removing, move focus to next entry, if it exist, else try previous
  var newFocusItem = gTagList.getNextItem(entry, 1) ||
                     gTagList.getPreviousItem(entry, 1);
  gTagList.removeItemAt(gTagList.getIndexOfItem(entry));
  if (newFocusItem)
    FocusTagEntry(newFocusItem);
  else
    UpdateButtonStates();
}

function MoveTag(aMoveUp)
{
  // Move the selected tag one position up or down in the tagList's child order.
  // This reordering may require changing ordinal strings, which will happen
  // when we write tag data to the preferences system in the OKHandler.
  var entry = gTagList.selectedItem;
  UpdateTagInfo(entry.taginfo, entry); // remember changed values
  var successor = aMoveUp ? gTagList.getPreviousItem(entry, 1)
                          : gTagList.getNextItem(entry, 2);
  entry.parentNode.insertBefore(entry, successor);
  FocusTagEntry(entry);
  UpdateTagEntry(entry.taginfo, entry); // needs to be visible
}

function Restore()
{
  // clear pref panel tag list
  // Remember any known keys for deletion in the OKHandler.
  while (gTagList.getRowCount())
  {
    var key = gTagList.removeItemAt(0).taginfo.key;
    if (key)
      gDeletedTags[key] = true; // dummy value
  }
  // add default items (no ordinal strings for those)
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefDescription = prefService.getDefaultBranch("mailnews.labels.description.");
  var prefColor       = prefService.getDefaultBranch("mailnews.labels.color.");
  const kLocalizedString = Components.interfaces.nsIPrefLocalizedString;
  for (var i = 1; i <= 5; ++i)
  {
    // create default tags from the former label defaults
    var key   = "$label" + i;
    var tag   = prefDescription.getComplexValue(i, kLocalizedString).data;
    var color = prefColor.getCharPref(i);
    var tagInfo = {tag: tag, key: key, color: color};
    AppendTagEntry(tagInfo, null);
  }
  FocusTagEntry(gTagList.getItemAtIndex(0));
}

function OnOK()
{
  var i;
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                             .getService(Components.interfaces.nsIMsgTagService);
  // we may be called in another page's context, so get the stored data from the
  // wsm the hard way
  var pageData    = parent.hPrefWindow.wsm.dataManager.pageData[TAGPANEL_URI];
  var activeTags  = pageData[ACTIVE_TAGS_ID];
  var deletedTags = pageData[DELETED_TAGS_ID];

  // remove all deleted tags from the preferences system
  for (var key in deletedTags)
    tagService.deleteKey(key);

  // count dupes so that we can eliminate them later
  var dupeCounts = {}; // indexed by tag
  for (i = 0; i < activeTags.length; ++i)
  {
    var tag = activeTags[i].tag;
    if (tag in dupeCounts)
      ++dupeCounts[tag];
    else
      dupeCounts[tag] = 0; // no dupes found yet
  }

  // Now write tags to the preferences system, create keys and ordinal strings.
  // Manually set ordinal strings are NOT retained!
  var lastTagInfo = null;
  for (i = 0; i < activeTags.length; ++i)
  {
    var tagInfo = activeTags[i];
    if (tagInfo)
    {
      var dupeCount = dupeCounts[tagInfo.tag];
      if (dupeCount > 0)
      {
        // ignore the first dupe, but set mark for further processing
        dupeCounts[tagInfo.tag] = -1;
      }
      else if (dupeCount < 0)
      {
        tagInfo.tag = DisambiguateTag(tagInfo.tag, dupeCounts);
        dupeCounts[tagInfo.tag] = 0; // new tag name is unique
      }

      if (!tagInfo.key)
      {
        // newly added tag, need to create a key and read it
        tagService.addTag(tagInfo.tag, '', '');
        tagInfo.key = tagService.getKeyForTag(tagInfo.tag);
      }

      if (tagInfo.key)
      {
        if (!lastTagInfo)
        {
          // the first tag list entry needs no ordinal string
          lastTagInfo = tagInfo;
          tagInfo.ordinal = '';
        }
        else
        {
          // if tagInfo's key is lower than that of its predecessor,
          // it needs an ordinal string
          var lastOrdinal = lastTagInfo.ordinal || lastTagInfo.key;
          if (lastOrdinal >= tagInfo.key)
          {
            // create new ordinal
            var tail = lastOrdinal.length - 1;
            if (('a' <= lastOrdinal[tail]) && (lastOrdinal[tail] < 'z'))
            {
              // increment last character
              lastOrdinal = lastOrdinal.substr(0, tail) +
                            String.fromCharCode(lastOrdinal.charCodeAt(tail) + 1);
            }
            else
            {
              // just begin a new increment position
              lastOrdinal += 'a';
            }
            tagInfo.ordinal = lastOrdinal;
          }
          else
          {
            // no ordinal necessary
            tagInfo.ordinal = '';
          }
        }

        // Update the tag definition
        try
        {
          tagService.addTagForKey(tagInfo.key,
                                  tagInfo.tag,
                                  tagInfo.color,
                                  tagInfo.ordinal);
        }
        catch (e)
        {
          dump('Could not update tag:\n' + e);
        }
        lastTagInfo = tagInfo;
      } // have key
    } // have tagInfo
  } // for all active tags
}
