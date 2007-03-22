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
 * The Original Code is Calendar Transaction Manager code.
 *
 * The Initial Developer of the Original Code is
 *   Philipp Kewisch (mozilla@kewis.ch)
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

function calTransactionManager() {
    if (!this.transactionManager) {
        this.transactionManager =
            Components.classes["@mozilla.org/transactionmanager;1"]
                      .createInstance(Components.interfaces.nsITransactionManager);
    }
}

calTransactionManager.prototype = {

    QueryInterface: function cTM_QueryInterface(aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calITransactionManager))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }
        return this;
    },

    transactionManager: null,

    createAndCommitTxn: function cTM_createAndCommitTxn(aAction,
                                                        aItem,
                                                        aCalendar,
                                                        aOldItem,
                                                        aListener) {
        var txn = new calTransaction(aAction,
                                     aItem,
                                     aCalendar,
                                     aOldItem,
                                     aListener);
        this.transactionManager.doTransaction(txn);
    },

    beginBatch: function cTM_beginBatch() {
        this.transactionManager.beginBatch();
    },

    endBatch: function cTM_endBatch() {
        this.transactionManager.endBatch();
    },

    undo: function cTM_undo() {
        this.transactionManager.undoTransaction();
    },

    canUndo: function cTM_canUndo() {
        return (this.transactionManager.numberOfUndoItems > 0);
    },

    redo: function cTM_redo() {
        this.transactionManager.redoTransaction();
    },

    canRedo: function cTM_canRedo() {
        return (this.transactionManager.numberOfRedoItems > 0);
    }
};

function calTransaction(aAction, aItem, aCalendar, aOldItem, aListener) {
    this.mAction = aAction;
    this.mItem = aItem;
    this.mCalendar = aCalendar;
    this.mOldItem = aOldItem;
    this.mListener = aListener;
}

calTransaction.prototype = {

    mAction: null,
    mCalendar: null,
    mItem: null,
    mOldItem: null,
    mOldCalendar: null,
    mListener: null,
    mIsDoTransaction: false,

    QueryInterface: function cT_QueryInterface(aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.nsITransaction) &&
            !aIID.equals(Components.interfaces.calIOperationListener))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }
        return this;
    },

    onOperationComplete: function cT_onOperationComplete(aCalendar,
                                                         aStatus,
                                                         aOperationType,
                                                         aId,
                                                         aDetail) {
        if (aStatus == Components.results.NS_OK &&
            (aOperationType == Components.interfaces.calIOperationListener.ADD ||
             aOperationType == Components.interfaces.calIOperationListener.MODIFY)) {
            if (this.mIsDoTransaction) {
                this.mItem = aDetail;
            } else {
                this.mOldItem = aDetail;
            }
        }
        if (this.mListener) {
            this.mListener.onOperationComplete(aCalendar,
                                              aStatus,
                                              aOperationType,
                                              aId,
                                              aDetail);
        }
    },

    onGetResult: function cT_onGetResult(aCalendar,
                                         aStatus,
                                         aItemType,
                                         aDetail,
                                         aCount,
                                         aItems) {
        if (this.mListener) {
            this.mListener.onGetResult(aCalendar,
                                       aStatus,
                                       aItemType,
                                       aDetail,
                                       aCount,
                                       aItems);
        }
    },

    doTransaction: function cT_doTransaction() {
        this.mIsDoTransaction = true;
        switch (this.mAction) {
            case 'add':
                this.mCalendar.addItem(this.mItem, this);
                break;
            case 'modify':
                this.mCalendar.modifyItem(this.mItem,
                                          this.mOldItem,
                                          this);
                break;
            case 'delete':
                this.mCalendar.deleteItem(this.mItem, this);
                break;
            case 'move':
                this.mOldCalendar = this.mOldItem.calendar;
                this.mOldCalendar.deleteItem(this.mOldItem, this);
                this.mCalendar.addItem(this.mItem, this);
                break;
            default:
                throw new Components.Exception("Invalid action specified",
                                               Components.results.NS_ERROR_ILLEGAL_VALUE);
                break;
        }
    },
    
    undoTransaction: function cT_undoTransaction() {
        this.mIsDoTransaction = false;
        switch (this.mAction) {
            case 'add':
                this.mCalendar.deleteItem(this.mItem, this);
                break;
            case 'modify':
                this.mCalendar.modifyItem(this.mOldItem, this.mItem, this);
                break;
            case 'delete':
                this.mCalendar.addItem(this.mItem, this);
                break;
            case 'move':
                this.mCalendar.deleteItem(this.mItem, this);
                this.mOldCalendar.addItem(this.mOldItem, this);
                break;
            default:
                throw new Components.Exception("Invalid action specified",
                                               Components.results.NS_ERROR_ILLEGAL_VALUE);
                break;
        }
    },
    
    redoTransaction: function cT_redoTransaction() {
        this.doTransaction();
    },

    isTransient: false,

    merge: function cT_merge(aTransaction) {
        // No support for merging
        return false;
    }
};
