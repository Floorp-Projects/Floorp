/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Garth Smedley <garths@oeone.com>
 *                 Mike Potter <mikep@oeone.com>
 *                 Colin Phillips <colinp@oeone.com> 
 *                 Chris Charabaruk <ccharabaruk@meldstar.com>
 *                 ArentJan Banck <ajbanck@planet.nl>
 *                 Stuart Parmenter <pavlov@pavlov.net>
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

/* dialog stuff */
function loadCalendarPublishDialog()
{
    var calendar = window.arguments[0];

    window.calendar = calendar;

    /* set the path based on what was passed in */
    var pathTextbox = document.getElementById("publish-remotePath-textbox");
    pathTextbox.value = getCalendarManager().getCalendarPref(calendar, "remote-ics-path");;

    /* check the URL field */
    checkURL();

    /* set focus to the path text box*/
    pathTextbox.focus();
}


function onOKCommand()
{
    var progressMeter = document.getElementById("publish-progressmeter");
    progressMeter.setAttribute("mode", "undetermined");

    var pathTextbox = document.getElementById("publish-remotePath-textbox");
    var remotePath = pathTextbox.value;

    try {
        publishCalendar(window.calendar, remotePath); 
    } catch(ex) {
        dump("error publishing!\n");
        dump(ex + "\n");

        progressMeter.setAttribute("mode", "determined");
    }

    return false;
}

function checkURL()
{
    var pathTextbox = document.getElementById("publish-remotePath-textbox");
    var publishWindow = document.getElementById("calendar-publishwindow");

    if (pathTextbox.value.length > 0)
        publishWindow.getButton("accept").removeAttribute("disabled");
    else
        publishWindow.getButton("accept").setAttribute("disabled", "true");
}

function closeDialog()
{
   self.close();
}



function publishCalendar(calendar, remotePath)
{
    var icsURL = makeURL(remotePath);

    var calManager = getCalendarManager();

    // create an ICS calendar, but don't register it
    var newCalendar = calManager.createCalendar("ics", icsURL);

    var getListener = {
        onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail)
        {
            var calManager = getCalendarManager();

            // delete the new calendar now that we're done with it
            calManager.deleteCalendar(newCalendar);

            /* update the remote ics path */
            var calManager = getCalendarManager();
            calManager.setCalendarPref(window.calendar, "remote-ics-path", remotePath);

            /* set the dialog up for closing */
            var progressMeter = document.getElementById("publish-progressmeter");
            progressMeter.setAttribute("mode", "determined");
            
            var publishWindow = document.getElementById("calendar-publishwindow");
            publishWindow.getButton("accept").setAttribute("label", closeButtonLabel);
            publishWindow.getButton("accept").setAttribute("oncommand", "closeDialog()");
        }
    };

    appendCalendars(newCalendar, [calendar], getListener);
}

function appendCalendars(to, froms, listener)
{
    var getListener = {
        onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail)
        {
            if (listener)
                listener.onOperationComplete(aCalendar, aStatus, aOperationType,
                                             aId, aDetail);
        },
        onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems)
        {
            if (!Components.isSuccessCode(aStatus)) {
                aborted = true;
                return;
            }
            if (aCount) {
                for (var i=0; i<aCount; ++i) {
                    // Store a (short living) reference to the item.
                    dump("adding item.. " + aItems[i] + "\n");
                    var itemCopy = aItems[i].clone();
                    to.addItem(itemCopy, null);
                }  
            }
        }
    };


    for each(var from in froms) {
        from.getItems(Components.interfaces.calICalendar.ITEM_FILTER_TYPE_ALL,
                      0, null, null, getListener);
    }
}




