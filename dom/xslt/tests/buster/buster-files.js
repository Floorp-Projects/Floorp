/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kFileOutStreamCID = "@mozilla.org/network/file-output-stream;1";
const nsIFileOutputStream = Components.interfaces.nsIFileOutputStream;

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
                var sink = new Object;
                sink.write = function(aContent, aCount)
                {
                    // replace NC:succ with NC:orig_succ,
                    //  so the rdf stuff differs
                    var content = aContent.replace(/NC:succ/g,"NC:orig_succ");
                    content = content.replace(/NC:failCount/g,"NC:orig_failCount");
                    this.mSink.write(content, content.length);
                    return aCount;
                };
                var fp = doCreateRDFFP('Xalan results',
                                       nsIFilePicker.modeSave);
                var res = fp.show();

                if (res == nsIFilePicker.returnOK ||
                    res == nsIFilePicker.returnReplace) {
                    var serial = doCreate(kRDFXMLSerializerID,
                                          nsIRDFXMLSerializer);
                    serial.init(view.mResultDS);
                    serial.QueryInterface(nsIRDFXMLSource);
                    var fl = fp.file;
                    var fstream = doCreate(kFileOutStreamCID,
                                           nsIFileOutputStream);
                    fstream.init(fl, 26, 420, 0);
                    sink.mSink = fstream;
                    serial.Serialize(sink);
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
