/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT Processor.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Axel Hecht <axel@pike.org>
 *  Peter Van der Beken <peterv@netscape.com>
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

const kFTSCID = "@mozilla.org/network/file-transport-service;1";
const nsIFileTransportService = Components.interfaces.nsIFileTransportService;
const kFileTransportService = doCreate(kFTSCID, nsIFileTransportService);

var cmdFileController = 
{
    supportsCommand: function(aCommand)
    {
        switch(aCommand) {
            case 'cmd_fl_save':
            case 'cmd_fl_import':
                return true;
            default:
        }
        return false;
    },
    isCommandEnabled: function(aCommand)
    {
        return this.supportsCommand(aCommand);
    },
    doCommand: function(aCommand)
    {
        switch(aCommand) {
            case 'cmd_fl_save':
                var serial = doCreate(kRDFXMLSerializerID,
                                      nsIRDFXMLSerializer);
                var sink = new Object;
                sink.content = '';
                sink.write = function(aContent, aCount)
                {
                    this.content += aContent;
                    return aCount;
                };
                serial.init(view.mResultDS);
                serial.QueryInterface(nsIRDFXMLSource);
                serial.Serialize(sink);
                if (!sink.content.length) {
                    return;
                }
                // replace NC:succ with NC:orig_succ, so the rdf stuff differs
                var content = sink.content.replace(/NC:succ/g,"NC:orig_succ");
                content = content.replace(/NC:failCount/g,"NC:orig_failCount");
                var fp = doCreateRDFFP('Xalan results',
                                       nsIFilePicker.modeSave);
                var res = fp.show();

                if (res == nsIFilePicker.returnOK ||
                    res == nsIFilePicker.returnReplace) {
                    var fl = fp.file;
                    var trans = kFileTransportService.createTransport
                        (fl, 26, // NS_WRONLY + NS_CREATE_FILE + NS_TRUNCATE
                         'w', true);
                    var out = trans.openOutputStream(0, -1, 0);
                    out.write(content, content.length);
                    out.close();
                    delete out;
                    delete trans;
                    fp.file.permissions = 420;
                }
                break;
            case 'cmd_fl_import':
                var fp = doCreateRDFFP('Previous Xalan results',
                                       nsIFilePicker.modeLoad);
                var res = fp.show();

                if (res == nsIFilePicker.returnOK) {
                    var fl = fp.file;
                    if (view.mPreviousResultDS) {
                        view.database.RemoveDataSource(view.mPreviousResultDS);
                        view.mPreviousResultDS = null;
                    }
                    view.mPreviousResultDS = kRDFSvc.GetDataSource(fp.fileURL.spec);
                    view.database.AddDataSource(view.mPreviousResultDS);
                }

                document.getElementById('obs_orig_success')
                    .setAttribute('hidden','false');
                break;
            default:
                alert('Unknown Command'+aCommand);
        }
    }
};

registerController(cmdFileController);
