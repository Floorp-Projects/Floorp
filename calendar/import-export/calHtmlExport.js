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
 * Contributor(s):
 *   Michiel van Leeuwen <mvl@exedo.nl>
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

// Export

function calHtmlExporter() {
    this.wrappedJSObject = this;
}

calHtmlExporter.prototype.QueryInterface =
function QueryInterface(aIID) {
    if (!aIID.equals(Components.interfaces.nsISupports) &&
        !aIID.equals(Components.interfaces.calIExporter)) {
        throw Components.results.NS_ERROR_NO_INTERFACE;
    }

    return this;
};

calHtmlExporter.prototype.getFileTypes =
function getFileTypes(aCount) {
    aCount.value = 1;
    return([{defaultExtension:'html', 
             extensionFilter:'*.html; *.htm', 
             description:'HTML'}]);
};

// not prototype.export. export is reserved.
calHtmlExporter.prototype.exportToStream =
function html_exportToStream(aStream, aCount, aItems) {
    var dateFormatter = 
        Components.classes["@mozilla.org/calendar/datetime-formatter;1"]
                  .getService(Components.interfaces.calIDateTimeFormatter);

    var html =
        <html>
            <head>
                <title>HTMLTitle</title>
                <meta http-equiv='Content-Type' content='text/html; charset=UTF-8'/>
                <style type='text/css'/>
            </head>
            <body>
                <!-- Note on the use of the summarykey class: this is a
                     special class, because in the default style, it is hidden.
                     The div is still included for those that want a different
                     style, where the key is visible -->
            </body>
        </html>;
    // XXX The html comment above won't propagate to the resulting html.
    //     Should fix that, one day.

    // Using this way to create the styles, because { and } are special chars
    // in e4x. They have to be escaped, which doesn't improve readability
    html.head.style = ".vevent {border: 1px solid black; padding: 0px; margin-bottom: 10px;}\n";
    html.head.style += "div.key {font-style: italic; margin-left: 3px;}\n";
    html.head.style += "div.value {margin-left: 20px;}\n";
    html.head.style += "abbr {border: none;}\n";
    html.head.style += ".summarykey {display: none;}\n";
    html.head.style += "div.summary {background: lightgray; font-weight: bold; margin: 0px; padding: 3px;}\n";

    // Sort aItems
    aItems.sort(function (a,b) { return a.startDate.compare(b.startDate); });

    for each (item in aItems) {
        try {
            item = item.QueryInterface(Components.interfaces.calIEvent);
        } catch(e) {
            continue;
        }

        // Put properties of the event in a definition list
        // Use hCalendar classes as bonus
        var ev = <div class='vevent'/>;

        // Title
        ev.appendChild(
            <div>
                <div class='key summarykey'>Title</div>
                <div class='value summary'>{item.title}</div>
            </div>
        );

        // Start and end
        var startstr = new Object();
        var endstr = new Object();
        dateFormatter.formatInterval(item.startDate, item.endDate, startstr, endstr);

        // Include the end date anyway, even when empty, because the dtend
        // class should be there, for hCalendar goodness.
        var seperator = "";
        if (endstr.value) {
            seperator = " - ";
        }

        ev.appendChild(
            <div>
                <div class='key'>When</div>
                <div class='value'>
                    <abbr class='dtstart' title={item.startDate.icalString}>{startstr.value}</abbr>
                    {seperator}
                    <abbr class='dtend' title={item.endDate.icalString}>{endstr.value}</abbr>
                </div>
            </div>
        );


        // Location
        if (item.getProperty('LOCATION')) {
            ev.appendChild(
                <div>
                    <div class='key'>Location</div>
                    <div class='value location'>{item.getProperty('LOCATION')}</div>
                </div>
            );
        }

        // Description, inside a pre to preserve formating when needed.
        var desc = item.getProperty('DESCRIPTION');
        if (desc && desc.length > 0) { 
            var usePre = false;
            if (desc.indexOf("\n ") >= 0 || desc.indexOf("\n\t") >= 0 ||
                desc.indexOf(" ") == 0 || desc.indexOf("\t") == 0)
                // (RegExp /^[ \t]/ doesn't work.)
                // contains indented preformatted text after beginning or newline
                // so preserve indentation with PRE.
                usePre = true;

            var descnode = 
                <div>
                    <div class='key'>Description</div>
                    <div class='value'/>
                </div>;

            if (usePre)
                descnode.div[1] = <pre class='description'>{desc}</pre>;
            else {
                descnode.div[1] = desc.replace(/\n/g, "<BR>\n");
                descnode.div[1].@class += ' description';
            }
            ev.appendChild(descnode);
        }
        html.body.appendChild(ev);
    }

    // Convert the javascript string to an array of bytes, using the
    // utf8 encoder
    var convStream = Components.classes["@mozilla.org/intl/converter-output-stream;1"]
                               .getService(Components.interfaces.nsIConverterOutputStream);
    convStream.init(aStream, 'UTF-8', 0, 0x0000);

    var str = html.toXMLString()
    convStream.writeString(str);
    return;
};
