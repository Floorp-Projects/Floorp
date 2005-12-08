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
    var str = 
        "<html>\n" +
        "<head>\n" + 
        "<title>HTMLTitle</title>\n" +
        "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n" +
        "</head>\n"+ "<body bgcolor=\"#FFFFFF\" text=\"#000000\">\n";
    aStream.write(str, str.length);
   
    for each (item in aItems) {
        try {
            item = item.QueryInterface(Components.interfaces.calIEvent);
        } catch(e) {
            continue;
        }
        var start = item.startDate.jsDate;
        var end = item.endDate.jsDate;
        //var when = dateFormat.formatInterval(start, end, calendarEvent.allDay);
        var when = item.startDate + " -- " + item.endDate;
        dump(when+"\n");
        var desc = item.description;
        if (desc == null)
          desc = "";
        if (desc.length > 0) { 
          if (desc.indexOf("\n ") >= 0 || desc.indexOf("\n\t") >= 0 ||
              desc.indexOf(" ") == 0 || desc.indexOf("\t") == 0)
            // (RegExp /^[ \t]/ doesn't work.)
            // contains indented preformatted text after beginning or newline
            // so preserve indentation with PRE.
            desc = "<PRE>"+desc+"</PRE>\n";
          else
            // no indentation, so preserve text line breaks in html with BR
            desc = "<P>"+desc.replace(/\n/g, "<BR>\n")+"</P>\n";
        }
        // use div around each event so events are navigable via DOM.
        str  = "<div><p>";
        str += "<B>Title: </B>\t" + item.title + "<BR>\n";
        str += "<B>When: </B>\t" + when.replace("--", "&ndash;") + "<BR>\n";
        str += "<B>Where: </B>\t" + item.location + "<BR>\n";
        // str += "<B>Organiser: </B>\t" + Event.???
        str += "</p>\n";
        str += desc; // may be empty
        str += "</div>\n";
        aStream.write(str, str.length);
    }

    str = "\n</body>\n</html>\n";
    aStream.write(str, str.length);
    return;
};
