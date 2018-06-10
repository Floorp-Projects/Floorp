# vim: set ts=4 sw=4 tw=99 et:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from ipdl.ast import Visitor, ASYNC


class SyncMessageChecker(Visitor):
    syncMsgList = []
    seenProtocols = []
    seenSyncMessages = []

    def __init__(self, syncMsgList):
        SyncMessageChecker.syncMsgList = syncMsgList
        self.errors = []

    def prettyMsgName(self, msg):
        return "%s::%s" % (self.currentProtocol, msg)

    def errorUnknownSyncMessage(self, loc, msg):
        self.errors.append('%s: error: Unknown sync IPC message %s' %
                           (str(loc), msg))

    def errorAsyncMessageCanRemove(self, loc, msg):
        self.errors.append('%s: error: IPC message %s is async, can be delisted' %
                           (str(loc), msg))

    def visitProtocol(self, p):
        self.errors = []
        self.currentProtocol = p.name
        SyncMessageChecker.seenProtocols.append(p.name)
        Visitor.visitProtocol(self, p)

    def visitMessageDecl(self, md):
        pn = self.prettyMsgName(md.name)
        if md.sendSemantics is not ASYNC:
            if pn not in SyncMessageChecker.syncMsgList:
                self.errorUnknownSyncMessage(md.loc, pn)
            SyncMessageChecker.seenSyncMessages.append(pn)
        elif pn in SyncMessageChecker.syncMsgList:
            self.errorAsyncMessageCanRemove(md.loc, pn)

    @staticmethod
    def getFixedSyncMessages():
        return set(SyncMessageChecker.syncMsgList) - set(SyncMessageChecker.seenSyncMessages)


def checkSyncMessage(tu, syncMsgList, errout=sys.stderr):
    checker = SyncMessageChecker(syncMsgList)
    tu.accept(checker)
    if len(checker.errors):
        for error in checker.errors:
            print >>errout, error
        return False
    return True


def checkFixedSyncMessages(config, errout=sys.stderr):
    fixed = SyncMessageChecker.getFixedSyncMessages()
    error_free = True
    for item in fixed:
        protocol = item.split('::')[0]
        # Ignore things like sync messages in test protocols we didn't compile.
        # Also, ignore platform-specific IPC messages.
        if protocol in SyncMessageChecker.seenProtocols and \
           'platform' not in config.options(item):
            print >>errout, 'Error: Sync IPC message %s not found, it appears to be fixed.\n' \
                            'Please remove it from sync-messages.ini.' % item
            error_free = False
    return error_free
