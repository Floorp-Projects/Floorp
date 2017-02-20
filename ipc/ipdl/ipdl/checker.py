# vim: set ts=4 sw=4 tw=99 et:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from ipdl.ast import Visitor, ASYNC

class SyncMessageChecker(Visitor):
    def __init__(self, syncMsgList):
        self.syncMsgList = syncMsgList
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
        self.currentProtocol = p.name
        Visitor.visitProtocol(self, p)

    def visitMessageDecl(self, md):
        pn = self.prettyMsgName(md.name)
        if md.sendSemantics is not ASYNC and pn not in self.syncMsgList:
            self.errorUnknownSyncMessage(md.loc, pn)
        if md.sendSemantics is ASYNC and pn in self.syncMsgList:
            self.errorAsyncMessageCanRemove(md.loc, pn)

def checkSyncMessage(tu, syncMsgList, errout=sys.stderr):
    checker = SyncMessageChecker(syncMsgList)
    tu.accept(checker)
    if len(checker.errors):
        for error in checker.errors:
            print >>errout, error
        return False
    return True
