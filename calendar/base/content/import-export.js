/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Calendar code.
 *
 * The Initial Developer of the Original Code is
 * ArentJan Banck <ajbanck@planet.nl>.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): ArentJan Banck <ajbanck@planet.nl>
 *                 Steve Hampton <mvgrad78@yahoo.com>
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
 *                 Jussi Kukkonen <jussi.kukkonen@welho.com>
 *                 Michiel van Leeuwen <mvl@exedo.nl>
 *                 Stefan Sitter <ssitter@googlemail.com>
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
 
// File constants copied from file-utils.js
const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_RDWR     = 0x04;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;
const MODE_SYNC     = 0x40;
const MODE_EXCL     = 0x80;

/**
 * loadEventsFromFile
 * shows a file dialog, reads the selected file(s) and tries to parse events from it.
 *
 * @param aCalendar  (optional) If specified, the items will be imported directly
 *                              into the calendar
 */
function loadEventsFromFile(aCalendar)
{
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
  
    var fp = Components.classes["@mozilla.org/filepicker;1"]
                       .createInstance(nsIFilePicker);
    fp.init(window, calGetString("calendar", "Open"), nsIFilePicker.modeOpen);
    fp.defaultExtension = "ics";

    // Get a list of exporters
    var contractids = new Array();
    var catman = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);
    var catenum = catman.enumerateCategory('cal-importers');
    while (catenum.hasMoreElements()) {
        var entry = catenum.getNext();
        entry = entry.QueryInterface(Components.interfaces.nsISupportsCString);
        var contractid = catman.getCategoryEntry('cal-importers', entry);
        var exporter = Components.classes[contractid]
                                 .getService(Components.interfaces.calIImporter);
        var types = exporter.getFileTypes({});
        var type;
        for each (type in types) {
            fp.appendFilter(type.description, type.extensionFilter);
            contractids.push(contractid);
        }
    }

    fp.show();

    if (fp.file && fp.file.path && fp.file.path.length > 0) {
        var filePath = fp.file.path;
        var importer = Components.classes[contractids[fp.filterIndex]]
                                 .getService(Components.interfaces.calIImporter);

        const nsIFileInputStream = Components.interfaces.nsIFileInputStream;
        const nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;

        var inputStream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                                    .createInstance(nsIFileInputStream);

        try
        {
           inputStream.init( fp.file, MODE_RDONLY, 0444, {} );

           var items = importer.importFromStream(inputStream, {});
           inputStream.close();
        }
        catch(ex)
        {
            switch (ex.result) {
                case Components.interfaces.calIErrors.INVALID_TIMEZONE:
                    showError(calGetString("calendar", "timezoneError", [filePath] , 1));
                    break;
                default:
                    showError(calGetString("calendar", "unableToRead") + filePath + "\n"+ ex);
            }
        }

        if (aCalendar) {
            putItemsIntoCal(aCalendar, items);
            return;
        }

        var count = new Object();
        var calendars = getCalendarManager().getCalendars(count);

        if (count.value == 1) {
            // There's only one calendar, so it's silly to ask what calendar
            // the user wants to import into.
            putItemsIntoCal(calendars[0], items, filePath);
        } else {
            // Ask what calendar to import into
            var args = new Object();
            args.onOk = function putItems(aCal) { putItemsIntoCal(aCal, items, filePath); };
            args.promptText = calGetString("calendar", "importPrompt");
            openDialog("chrome://calendar/content/chooseCalendarDialog.xul", 
                       "_blank", "chrome,titlebar,modal,resizable", args);
        }
    }
}

function putItemsIntoCal(destCal, aItems, aFilePath) {
    // Set batch for the undo/redo transaction manager
    startBatchTransaction();

    // And set batch mode on the calendar, to tell the views to not
    // redraw until all items are imported
    destCal.startBatch();

    // This listener is needed to find out when the last addItem really
    // finished. Using a counter to find the last item (which might not
    // be the last item added)
    var count = 0;
    var failedCount = 0;
    var duplicateCount = 0;
    // Used to store the last error. Only the last error, because we don't
    // wan't to bomb the user with thousands of error messages in case
    // something went really wrong.
    // (example of something very wrong: importing the same file twice.
    //  quite easy to trigger, so we really should do this)
    var lastError;
    var listener = {
        onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail) {
            count++;
            if (!Components.isSuccessCode(aStatus)) {
                if (aStatus == Components.interfaces.calIErrors.DUPLICATE_ID) {
                    duplicateCount++;
                } else {
                    failedCount++;
                    lastError = aStatus;
                }
            }
            // See if it is time to end the calendar's batch.
            if (count == aItems.length) {
                destCal.endBatch();
                if (!failedCount && duplicateCount) {
                    showError(calGetString("calendar", "duplicateError", [duplicateCount, aFilePath] , 2));
                } else if (failedCount) {
                    showError(calGetString("calendar", "importItemsFailed", [failedCount, lastError.toString()] , 2));
                }
            }
        }
    }

    for each (item in aItems) {
        // XXX prompt when finding a duplicate.
        try {
            destCal.addItem(item, listener);
        } catch(e) {
            failedCount++;
            lastError = e;
            // Call the listener's operationComplete, to increase the
            // counter and not miss failed items. Otherwise, endBatch might
            // never be called.
            listener.onOperationComplete(null, null, null, null, null);
            Components.utils.reportError("Import error: "+e);
        }
    }

    // End transmgr batch
    endBatchTransaction();
}

/**
 * saveEventsToFile
 *
 * Save data to a file. Create the file or overwrite an existing file.
 *
 * @param calendarEventArray (required) Array of calendar events that should
 *                                      be saved to file.
 * @param aDefaultFileName   (optional) Initial filename shown in SaveAs dialog.
 */

function saveEventsToFile(calendarEventArray, aDefaultFileName)
{
   if (!calendarEventArray)
       return;

   if (!calendarEventArray.length)
   {
      alert(calGetString("calendar", "noEventsToSave"));
      return;
   }

   // Show the 'Save As' dialog and ask for a filename to save to
   const nsIFilePicker = Components.interfaces.nsIFilePicker;

   var fp = Components.classes["@mozilla.org/filepicker;1"]
                      .createInstance(nsIFilePicker);

   fp.init(window, calGetString("calendar", "SaveAs"), nsIFilePicker.modeSave);

   if (aDefaultFileName && aDefaultFileName.length && aDefaultFileName.length > 0) {
      fp.defaultString = aDefaultFileName;
   } else if (calendarEventArray.length == 1 && calendarEventArray[0].title) {
      fp.defaultString = calendarEventArray[0].title;
   } else {
      fp.defaultString = calGetString("calendar", "defaultFileName");
   }

   fp.defaultExtension = "ics";

   // Get a list of exporters
   var contractids = new Array();
   var catman = Components.classes["@mozilla.org/categorymanager;1"]
                          .getService(Components.interfaces.nsICategoryManager);
   var catenum = catman.enumerateCategory('cal-exporters');
   while (catenum.hasMoreElements()) {
       var entry = catenum.getNext();
       entry = entry.QueryInterface(Components.interfaces.nsISupportsCString);
       var contractid = catman.getCategoryEntry('cal-exporters', entry);
       var exporter = Components.classes[contractid]
                                .getService(Components.interfaces.calIExporter);
       var types = exporter.getFileTypes({});
       var type;
       for each (type in types) {
           fp.appendFilter(type.description, type.extensionFilter);
           contractids.push(contractid);
       }
   }


   fp.show();

   // Now find out as what to save, convert the events and save to file.
   if (fp.file && fp.file.path.length > 0) 
   {
      const UTF8 = "UTF-8";
      var aDataStream;
      var extension;
      var charset;

      var exporter = Components.classes[contractids[fp.filterIndex]]
                               .getService(Components.interfaces.calIExporter);

      var filePath = fp.file.path;
      if(filePath.indexOf(".") == -1 )
          filePath += "."+exporter.getFileTypes({})[0].defaultExtension;

      const LOCALFILE_CTRID = "@mozilla.org/file/local;1";
      const FILEOUT_CTRID = "@mozilla.org/network/file-output-stream;1";
      const nsILocalFile = Components.interfaces.nsILocalFile;
      const nsIFileOutputStream = Components.interfaces.nsIFileOutputStream;

      var outputStream;

      var localFileInstance = Components.classes[LOCALFILE_CTRID]
                                        .createInstance(nsILocalFile);
      localFileInstance.initWithPath(filePath);

      outputStream = Components.classes[FILEOUT_CTRID]
                               .createInstance(nsIFileOutputStream);
      try
      {
         outputStream.init(localFileInstance, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE, 0664, 0);
         // XXX Do the right thing with unicode and stuff. Or, again, should the
         //     exporter handle that?
         exporter.exportToStream(outputStream, calendarEventArray.length, calendarEventArray, null);
         outputStream.close();
      }
      catch(ex)
      {
         showError(calGetString("calendar", "unableToWrite") + filePath);
      }
   }
}

/* Exports all the events and tasks in a calendar.  If aCalendar is not specified,
 * the user will be prompted with a list of calendars to choose which one to export.
 */
function exportEntireCalendar(aCalendar) {
    var itemArray = [];
    var getListener = {
        onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail)
        {
            saveEventsToFile(itemArray, aCalendar.name);
        },
        onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems)
        {
            for each (item in aItems) {
                itemArray.push(item);   
            }
        }
    };

    function getItemsFromCal(aCal) {
        aCal.getItems(Components.interfaces.calICalendar.ITEM_FILTER_ALL_ITEMS,
                      0, null, null, getListener);
    }

    if (!aCalendar) {
        var count = new Object();
        var calendars = getCalendarManager().getCalendars(count);

        if (count.value == 1) {
            // There's only one calendar, so it's silly to ask what calendar
            // the user wants to import into.
            getItemsFromCal(calendars[0]);
        } else {
            // Ask what calendar to import into
            var args = new Object();
            args.onOk = getItemsFromCal;
            args.promptText = calGetString("calendar", "exportPrompt");
            openDialog("chrome://calendar/content/chooseCalendarDialog.xul", 
                       "_blank", "chrome,titlebar,modal,resizable", args);
        }
    } else {
        getItemsFromCal(aCalendar);
    }
}

function showError(aMsg)
{
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                  .getService(Components.interfaces.nsIPromptService);
    promptService.alert(null,
                        calGetString("calendar", "errorTitle"),
                        aMsg);
}
