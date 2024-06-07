# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import optparse
import os
import sys
from configparser import RawConfigParser
from io import StringIO
from multiprocessing import Manager

import ipdl
from ipdl.ast import SYNC
from ipdl.exporter import JSONExporter


class WorkerPool:
    per_process_context = None

    def __init__(
        self,
        manager,
        files,
        asts,
        headersdir,
        cppdir,
        segmentCapacityDict,
        allmessages,
        allprotocols,
        allmessageprognames,
        allsyncmessages,
        alljsonobjs,
        *,
        processes=None
    ):
        if processes is None:
            processes = os.cpu_count() or 1
        processes = min(processes, 8)
        self.n = len(files)
        self.asts = asts
        self.pool = manager.Pool(
            initializer=WorkerPool._init_worker,
            initargs=(
                files,
                asts,
                headersdir,
                cppdir,
                segmentCapacityDict,
                allmessages,
                allprotocols,
                allmessageprognames,
                allsyncmessages,
                alljsonobjs,
            ),
            processes=processes,
        )

    def run(self):
        self.pool.map(WorkerPool._run_worker, range(self.n))

    @staticmethod
    def _init_worker(*state):
        WorkerPool.per_process_context = state

    @staticmethod
    def _run_worker(index):
        (
            files,
            asts,
            headersdir,
            cppdir,
            segmentCapacityDict,
            allmessages,
            allprotocols,
            allmessageprognames,
            allsyncmessages,
            alljsonobjs,
        ) = WorkerPool.per_process_context
        ast = asts[index]
        ipdl.gencxx(files[index], ast, headersdir, cppdir, segmentCapacityDict)

        if ast.protocol:
            allmessages[ast.protocol.name] = ipdl.genmsgenum(ast)
            allprotocols.append(ast.protocol.name)

            alljsonobjs.append(JSONExporter.protocolToObject(ast.protocol))

            # e.g. PContent::RequestMemoryReport (not prefixed or suffixed.)
            for md in ast.protocol.messageDecls:
                allmessageprognames.append("%s::%s" % (md.namespace, md.decl.progname))

                if md.sendSemantics is SYNC:
                    allsyncmessages.append(
                        "%s__%s" % (ast.protocol.name, md.prettyMsgName())
                    )


def main():
    def log(minv, fmt, *args):
        if _verbosity >= minv:
            print(fmt % args)

    # process command line

    op = optparse.OptionParser(usage="ipdl.py [options] IPDLfiles...")
    op.add_option(
        "-I",
        "--include",
        dest="includedirs",
        default=[],
        action="append",
        help="Additional directory to search for included protocol specifications",
    )
    op.add_option(
        "-s",
        "--sync-msg-list",
        dest="syncMsgList",
        default="sync-messages.ini",
        help="Config file listing allowed sync messages",
    )
    op.add_option(
        "-m",
        "--msg-metadata",
        dest="msgMetadata",
        default="message-metadata.ini",
        help="Predicted message sizes for reducing serialization malloc overhead.",
    )
    op.add_option(
        "-v",
        "--verbose",
        dest="verbosity",
        default=1,
        action="count",
        help="Verbose logging (specify -vv or -vvv for very verbose logging)",
    )
    op.add_option(
        "-q",
        "--quiet",
        dest="verbosity",
        action="store_const",
        const=0,
        help="Suppress logging output",
    )
    op.add_option(
        "-d",
        "--outheaders-dir",
        dest="headersdir",
        default=".",
        help="""Directory into which C++ headers will be generated.
    A protocol Foo in the namespace bar will cause the headers
      dir/bar/Foo.h, dir/bar/FooParent.h, and dir/bar/FooParent.h
    to be generated""",
    )
    op.add_option(
        "-o",
        "--outcpp-dir",
        dest="cppdir",
        default=".",
        help="""Directory into which C++ sources will be generated
    A protocol Foo in the namespace bar will cause the sources
      cppdir/FooParent.cpp, cppdir/FooChild.cpp
    to be generated""",
    )
    op.add_option(
        "-F",
        "--file-list",
        dest="file_list_file",
        default=None,
        help="""A file containing IPDL files to parse. This will be
    merged with files provided on the commandline.""",
    )

    options, cmdline_files = op.parse_args()
    _verbosity = options.verbosity
    syncMsgList = options.syncMsgList
    headersdir = options.headersdir
    cppdir = options.cppdir
    includedirs = [os.path.abspath(incdir) for incdir in options.includedirs]

    files = []

    if options.file_list_file is not None:
        with open(options.file_list_file) as f:
            files.extend(f.read().splitlines())

    files.extend(cmdline_files)

    if not len(files):
        op.error("No IPDL files specified")

    ipcmessagestartpath = os.path.join(headersdir, "IPCMessageStart.h")
    ipc_msgtype_name_path = os.path.join(cppdir, "IPCMessageTypeName.cpp")

    log(2, 'Generated C++ headers will be generated relative to "%s"', headersdir)
    log(2, 'Generated C++ sources will be generated in "%s"', cppdir)

    def normalizedFilename(f):
        if f == "-":
            return "<stdin>"
        return f

    log(2, "Reading sync message list")
    parser = RawConfigParser()
    parser.read_file(open(options.syncMsgList))
    syncMsgList = parser.sections()

    for section in syncMsgList:
        if not parser.get(section, "description"):
            print(
                "Error: Sync message %s lacks a description" % section, file=sys.stderr
            )
            sys.exit(1)

    # Read message metadata. Right now we only have 'segment_capacity'
    # for the standard segment size used for serialization.
    log(2, "Reading message metadata...")
    msgMetadataConfig = RawConfigParser()
    msgMetadataConfig.read_file(open(options.msgMetadata))

    manager = Manager()
    segmentCapacityDict = manager.dict()
    allmessages = manager.dict()
    allsyncmessages = manager.list()
    allmessageprognames = manager.list()
    allprotocols = manager.list()
    alljsonobjs = manager.list()

    for msgName in msgMetadataConfig.sections():
        if msgMetadataConfig.has_option(msgName, "segment_capacity"):
            capacity = msgMetadataConfig.get(msgName, "segment_capacity")
            segmentCapacityDict[msgName] = capacity

    # First pass: parse and type-check all protocols
    asts = []
    for f in files:
        log(2, os.path.basename(f))
        filename = normalizedFilename(f)
        if f == "-":
            fd = sys.stdin
        else:
            fd = open(f)

        specstring = fd.read()
        fd.close()

        ast = ipdl.parse(specstring, filename, includedirs=includedirs)
        if ast is None:
            print("Specification could not be parsed.", file=sys.stderr)
            sys.exit(1)

        log(2, "checking types")
        if not ipdl.typecheck(ast):
            print("Specification is not well typed.", file=sys.stderr)
            sys.exit(1)

        if not ipdl.checkSyncMessage(ast, syncMsgList):
            print(
                "Error: New sync IPC messages must be reviewed by an IPC peer and recorded in %s"
                % options.syncMsgList,
                file=sys.stderr,
            )  # NOQA: E501
            sys.exit(1)

        asts.append(ast)

    if not ipdl.checkFixedSyncMessages(parser):
        # Errors have alraedy been printed to stderr, just exit
        sys.exit(1)

    # Second pass: generate code
    pool = WorkerPool(
        manager,
        files,
        asts,
        headersdir,
        cppdir,
        segmentCapacityDict,
        allmessages,
        allprotocols,
        allmessageprognames,
        allsyncmessages,
        alljsonobjs,
    )
    pool.run()

    if cppdir is not None:
        # Sort to ensure deterministic output
        alljsonobjs = list(alljsonobjs)
        alljsonobjs.sort(key=lambda p: p["name"])
        ipdl.writeifmodified(
            json.dumps({"protocols": alljsonobjs}, indent=2),
            os.path.join(cppdir, "protocols.json"),
        )

    allprotocols.sort()
    allsyncmessages.sort()

    # Check if we have undefined message names in segmentCapacityDict.
    # This is a fool-proof of the 'message-metadata.ini' file.
    undefinedMessages = set(segmentCapacityDict.keys()) - set(allmessageprognames)
    if len(undefinedMessages) > 0:
        print(
            "Error: Undefined message names in message-metadata.ini:", file=sys.stderr
        )
        print(undefinedMessages, file=sys.stderr)
        sys.exit(1)

    ipcmsgstart = StringIO()

    print(
        """
// CODE GENERATED by ipdl.py. Do not edit.

#ifndef IPCMessageStart_h
#define IPCMessageStart_h

enum IPCMessageStart {
""",
        file=ipcmsgstart,
    )

    for name in allprotocols:
        print("  %sMsgStart," % name, file=ipcmsgstart)

    print(
        """
  LastMsgIndex
};

static_assert(LastMsgIndex <= 65536, "need to update IPC_MESSAGE_MACRO");

#endif // ifndef IPCMessageStart_h
""",
        file=ipcmsgstart,
    )

    ipc_msgtype_name = StringIO()
    print(
        """
// CODE GENERATED by ipdl.py. Do not edit.
#include <cstdint>

#include "mozilla/ipc/ProtocolUtils.h"
#include "IPCMessageStart.h"

using std::uint32_t;

namespace {

enum IPCMessages {
    """,
        file=ipc_msgtype_name,
    )

    for protocol in sorted(allmessages.keys()):
        for msg, num in allmessages[protocol].idnums:
            if num:
                print("  %s = %s," % (msg, num), file=ipc_msgtype_name)
            elif not msg.endswith("End"):
                print("  %s__%s," % (protocol, msg), file=ipc_msgtype_name)

    print(
        """
};

} // anonymous namespace

namespace IPC {

bool IPCMessageTypeIsSync(uint32_t aMessageType)
{
  switch (aMessageType) {
""",
        file=ipc_msgtype_name,
    )

    for msg in allsyncmessages:
        print("  case %s:" % msg, file=ipc_msgtype_name)

    print(
        """    return true;
  default:
    return false;
  }
}

const char* StringFromIPCMessageType(uint32_t aMessageType)
{
  switch (aMessageType) {
    """,
        file=ipc_msgtype_name,
    )

    for protocol in sorted(allmessages.keys()):
        for msg, num in allmessages[protocol].idnums:
            if num or msg.endswith("End"):
                continue
            print(
                """
  case %s__%s:
    return "%s::%s";"""
                % (protocol, msg, protocol, msg),
                file=ipc_msgtype_name,
            )

    print(
        """
  case DATA_PIPE_CLOSED_MESSAGE_TYPE:
    return "DATA_PIPE_CLOSED_MESSAGE";
  case DATA_PIPE_BYTES_CONSUMED_MESSAGE_TYPE:
    return "DATA_PIPE_BYTES_CONSUMED_MESSAGE";
  case ACCEPT_INVITE_MESSAGE_TYPE:
    return "ACCEPT_INVITE_MESSAGE";
  case REQUEST_INTRODUCTION_MESSAGE_TYPE:
    return "REQUEST_INTRODUCTION_MESSAGE";
  case INTRODUCE_MESSAGE_TYPE:
    return "INTRODUCE_MESSAGE";
  case BROADCAST_MESSAGE_TYPE:
    return "BROADCAST_MESSAGE";
  case EVENT_MESSAGE_TYPE:
    return "EVENT_MESSAGE";
  case IMPENDING_SHUTDOWN_MESSAGE_TYPE:
    return "IMPENDING_SHUTDOWN";
  case BUILD_IDS_MATCH_MESSAGE_TYPE:
    return "BUILD_IDS_MATCH_MESSAGE";
  case BUILD_ID_MESSAGE_TYPE:
    return "BUILD_ID_MESSAGE";
  case CHANNEL_OPENED_MESSAGE_TYPE:
    return "CHANNEL_OPENED_MESSAGE";
  case SHMEM_DESTROYED_MESSAGE_TYPE:
    return "SHMEM_DESTROYED_MESSAGE";
  case SHMEM_CREATED_MESSAGE_TYPE:
    return "SHMEM_CREATED_MESSAGE";
  case GOODBYE_MESSAGE_TYPE:
    return "GOODBYE_MESSAGE";
  case CANCEL_MESSAGE_TYPE:
    return "CANCEL_MESSAGE";
  default:
    return "<unknown IPC msg name>";
  }
}

} // namespace IPC

namespace mozilla {
namespace ipc {

const char* ProtocolIdToName(IPCMessageStart aId) {
  switch (aId) {
""",
        file=ipc_msgtype_name,
    )

    for name in allprotocols:
        print("    case %sMsgStart:" % name, file=ipc_msgtype_name)
        print('      return "%s";' % name, file=ipc_msgtype_name)

    print(
        """
  default:
    return "<unknown protocol id>";
  }
}

} // namespace ipc
} // namespace mozilla
""",
        file=ipc_msgtype_name,
    )

    ipdl.writeifmodified(ipcmsgstart.getvalue(), ipcmessagestartpath)
    ipdl.writeifmodified(ipc_msgtype_name.getvalue(), ipc_msgtype_name_path)


if __name__ == "__main__":
    main()
