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
 * The Original Code is bookmark transaction code.
 *
 * The Initial Developer of the Original Code is
 *   Joey Minta <jminta@gmail.com>
 *
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

function bookmarkTransactionManager() {
    this.wrappedJSObject = this;
    this.mTransactionManager = Components.classes["@mozilla.org/transactionmanager;1"]
                                         .createInstance(Components.interfaces.nsITransactionManager);

    this.mBatchCount = 0;

    this.classInfo = {
        getInterfaces: function (count) {
            var ifaces = [
                Components.interfaces.nsISupports,
                Components.interfaces.nsIClassInfo
            ];
            count.value = ifaces.length;
            return ifaces;
        },

        getHelperForLanguage: function (language) {
            return null;
        },

        contractID: "@mozilla.org/bookmarks/transactionManager;1",
        classDescription: "Booksmarks Transaction Manager",
        classID: Components.ID("{62d2f7fb-acd2-4876-aa2d-b607de9329ff}"),
        implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
        flags: 0
    };

    // Define our transactions
    function bkmkTxn() {
        this.item    = null;
        this.parent  = null;
        this.index   = null;
        this.removedProp = null;
    };

    bkmkTxn.prototype = {
        BMDS: null,

        QueryInterface: function bkmkTxnQI(iid) {
            if (!iid.equals(Components.interfaces.nsITransaction) &&
                !iid.equals(Components.interfaces.nsISupports))
            throw Components.results.NS_ERROR_NO_INTERFACE;

            return this;
        },

        merge               : function (aTxn)   {return false},
        getHelperForLanguage: function (aCount) {return null},
        getInterfaces       : function (aCount) {return null},
        canCreateWrapper    : function (aIID)   {return "AllAccess"},

        mAssertProperties: function bkmkTxnAssertProps(aProps) {
            if (!aProps) {
                return;
            }

            for each (var prop in aProps) {
                for (var i = 0; i < this.Properties.length; i++) {
                    var oldValue = this.BMDS.GetTarget(this.item, this.Properties[i], true);
                    // must check, if paste call after copy the oldvalue didn't remove.
                    if (!oldValue) {
                        var newValue = aProps[i+1];
                        if (newValue) {
                            this.BMDS.Assert(this.item, 
                                             this.Properties[i], 
                                             newValue, true);
                        }
                    } else {
                        this.removedProp[i+1] = oldValue;
                    }
                }
            }
        },

        mUnassertProperties: function bkmkTxnUnassertProps(aProps) {
            if (!aProps) {
                return;
            }
            for each (var prop in aProps) {
                for (var i = 0; i < this.Properties.length; i++) {
                    var oldValue = aProps[i+1];
                    if (oldValue) {
                        this.BMDS.Unassert(this.item, this.Properties[i], oldValue);
                    }
                }
            }
        }
    };

    bkmkTxn.prototype.RDFC = 
            Components.classes["@mozilla.org/rdf/container;1"]
                      .createInstance(Components.interfaces.nsIRDFContainer);
    var rdfService = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                               .getService(Components.interfaces.nsIRDFService);
    bkmkTxn.prototype.BMDS = rdfService.GetDataSource("rdf:bookmarks");

    bkmkTxn.prototype.Properties = 
            [rdfService.GetResource("http://home.netscape.com/NC-rdf#Name"),
             rdfService.GetResource("http://home.netscape.com/NC-rdf#URL"),
             rdfService.GetResource("http://home.netscape.com/NC-rdf#ShortcutURL"),
             rdfService.GetResource("http://home.netscape.com/NC-rdf#Description"),
             rdfService.GetResource("http://home.netscape.com/NC-rdf#WebPanel"),
             rdfService.GetResource("http://home.netscape.com/NC-rdf#FeedURL")];

    function bkmkInsertTxn(aAction) {
        this.type    = "insert";
        // move container declaration to here so it can be recognized if
        // undoTransaction is call after the BM manager is close and reopen.
        this.container = Components.classes["@mozilla.org/rdf/container;1"]
                                   .createInstance(Components.interfaces.nsIRDFContainer);
    }

    bkmkInsertTxn.prototype = {
         __proto__: bkmkTxn.prototype,

         isTransient: false,

         doTransaction: function bkmkInsertDoTxn() {
             this.RDFC.Init(this.BMDS, this.parent);
             // if the index is -1, we use appendElement, and then update the
             // index so that undoTransaction can still function
             if (this.index == -1) {
                 this.RDFC.AppendElement(this.item);
                 this.index = this.RDFC.GetCount();
             } else {
/*XXX- broken insert code, see bug 264571
                 try {
                     this.RDFC.InsertElementAt(this.item, this.index, true);
                 } catch (e if e.result == Components.results.NS_ERROR_ILLEGAL_VALUE) {
                     // if this failed, then we assume that we really want to append,
                     // because things are out of whack until we renumber.
                     this.RDFC.AppendElement(this.item);
                     // and then fix up the index so undo works
                     this.index = this.RDFC.GetCount();
                 }
*/
                 this.RDFC.InsertElementAt(this.item, this.index, true);
             }

             // insert back all the properties
             this.mAssertProperties(this.removedProp);
         },

         undoTransaction: function bkmkInsertUndoTxn() {
             // XXXvarga Can't use |RDFC| here because it's being "reused" elsewhere.
             this.container.Init(this.BMDS, this.parent);

             // remove all properties befor we remove the element so
             // nsLocalSearchService doesn't return deleted element in Search
             this.mUnassertProperties(this.removedProp);

             this.container.RemoveElementAt(this.index, true);
         },

         redoTransaction: function bkmkInsertRedoTxn() {
             this.doTransaction();
         }
    };

    function bkmkRemoveTxn() {
        this.type    = "remove";
    }

    bkmkRemoveTxn.prototype = {
        __proto__: bkmkTxn.prototype,

        isTransient: false,

        doTransaction: function bkmkRemoveDoTxn() {
            this.RDFC.Init(this.BMDS, this.parent);

            // remove all properties befor we remove the element so
            // nsLocalSearchService doesn't return deleted element in Search
            this.mUnassertProperties(this.removedProp);

            this.RDFC.RemoveElementAt(this.index, false);
        },

        undoTransaction: function bkmkRemoveUndoTxn() {
            this.RDFC.Init(this.BMDS, this.parent);
            this.RDFC.InsertElementAt(this.item, this.index, false);

            // insert back all the properties
            this.mAssertProperties(this.removedProp);
        },

        redoTransaction: function () {
            this.doTransaction();
        }
    }

    function bkmkImportTxn(aAction) {
        this.type    = "import";
        this.action  = aAction;
    }

    bkmkImportTxn.prototype = {
        __proto__: bkmkTxn.prototype,

        isTransient: false,

        doTransaction: function bkmkImportDoTxn() {},

        undoTransaction: function mkmkImportUndoTxn() {
            this.RDFC.Init(this.BMDS, this.parent);
            this.RDFC.RemoveElementAt(this.index, true);
        },

        redoTransaction: function bkmkImportredoTxn() {
            this.RDFC.Init(this.BMDS, this.parent);
            this.RDFC.InsertElementAt(this.item, this.index, true);
        }
    };

    this.BookmarkRemoveTransaction = bkmkRemoveTxn;
    this.BookmarkInsertTransaction = bkmkInsertTxn;
    this.BookmarkImportTransaction = bkmkImportTxn;
}

bookmarkTransactionManager.prototype.QueryInterface = function bkTxnMgrQI(aIID) {
    if (aIID.equals(Components.interfaces.nsISupports) ||
        aIID.equals(Components.interfaces.nsIBookmarkTransactionManager)) {
        return this;
    } 
    if (aIID.equals(Components.interfaces.nsIClassInfo)) {
        return this.classInfo;
    }

    throw NS_ERROR_NO_INTERFACE;
}

bookmarkTransactionManager.prototype.createAndCommitTxn = 
function bkmkTxnMgrCandC(aType, aAction, aItem, aIndex, aParent, aPropCount, aRemovedProps) {
    var txn;
    var nsIBookmarkTransactionManager = Components.interfaces.nsIBookmarkTransactionManager;
    switch (aType) {
        case nsIBookmarkTransactionManager.IMPORT: 
            txn = new this.BookmarkImportTransaction(aAction);
            break;
        case nsIBookmarkTransactionManager.INSERT: 
            txn = new this.BookmarkInsertTransaction(aAction);
            break;
        case nsIBookmarkTransactionManager.REMOVE: 
            txn = new this.BookmarkRemoveTransaction(aAction);
            break;
        default:
            Components.utils.reportError("Unknown bookmark transaction type:"+aType);
            throw NS_ERROR_FAILURE;
    }
    txn.item = aItem;
    txn.parent = aParent;
    txn.index = aIndex;
    txn.removedProp = aRemovedProps;
    txn.action = aAction;
    txn.wrappedJSObject = txn;
    this.mTransactionManager.doTransaction(txn);
}

bookmarkTransactionManager.prototype.startBatch = function bkmkTxnMgrUndo() {
    if (this.mBatchCount == 0) {
        this.mTransactionManager.beginBatch();
    }
    this.mBatchCount++;
}

bookmarkTransactionManager.prototype.endBatch = function bkmkTxnMgrUndo() {
    this.mBatchCount--;
    if (this.mBatchCount == 0) {
        this.mTransactionManager.endBatch();
    }
}

bookmarkTransactionManager.prototype.undo = function bkmkTxnMgrUndo() {
    this.mTransactionManager.undoTransaction();
}

bookmarkTransactionManager.prototype.redo = function bkmkTxnMgrRedo() {
    this.mTransactionManager.redoTransaction();
}

bookmarkTransactionManager.prototype.canUndo = function bkmkTxnMgrCanUndo() {
    return this.mTransactionManager.numberOfUndoItems > 0;
}

bookmarkTransactionManager.prototype.canRedo = function bkmkTxnMgrCanRedo() {
    return this.mTransactionManager.numberOfRedoItems > 0;
}

bookmarkTransactionManager.prototype.__defineGetter__("transactionManager", 
function bkmkTxnMgrGetter() {  return this.mTransactionManager; });

/****
 **** module registration
 ****/

const kFactory = {
    createInstance: function (outer, iid) {
        if (outer != null)
            throw Components.results.NS_ERROR_NO_AGGREGATION;
        return (new bookmarkTransactionManager()).QueryInterface(iid);
    }
};

var bkmkTxnMgrModule = {
    mCID: Components.ID("{8be133d0-681d-4f0b-972b-6a68e41afb62}"),
    mContractID: "@mozilla.org/bookmarks/transactionmanager;1",
    
    registerSelf: function (compMgr, fileSpec, location, type) {
        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
        compMgr.registerFactoryLocation(this.mCID,
                                        "Bookmark Transaction Manager",
                                        this.mContractID,
                                        fileSpec,
                                        location,
                                        type);
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.mCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return kFactory;
    },

    canUnload: function(compMgr) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return bkmkTxnMgrModule;
}
