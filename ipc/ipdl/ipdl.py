# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import print_function

from io import StringIO
import optparse
import os
import sys
from configparser import RawConfigParser

import ipdl


def log(minv, fmt, *args):
    if _verbosity >= minv:
        print(fmt % args)

# process command line


op = optparse.OptionParser(usage='ipdl.py [options] IPDLfiles...')
op.add_option('-I', '--include', dest='includedirs', default=[],
              action='append',
              help='Additional directory to search for included protocol specifications')
op.add_option('-s', '--sync-msg-list', dest='syncMsgList', default='sync-messages.ini',
              help="Config file listing allowed sync messages")
op.add_option('-m', '--msg-metadata', dest='msgMetadata', default='message-metadata.ini',
              help="Predicted message sizes for reducing serialization malloc overhead.")
op.add_option('-v', '--verbose', dest='verbosity', default=1, action='count',
              help='Verbose logging (specify -vv or -vvv for very verbose logging)')
op.add_option('-q', '--quiet', dest='verbosity', action='store_const', const=0,
              help="Suppress logging output")
op.add_option('-d', '--outheaders-dir', dest='headersdir', default='.',
              help="""Directory into which C++ headers will be generated.
A protocol Foo in the namespace bar will cause the headers
  dir/bar/Foo.h, dir/bar/FooParent.h, and dir/bar/FooParent.h
to be generated""")
op.add_option('-o', '--outcpp-dir', dest='cppdir', default='.',
              help="""Directory into which C++ sources will be generated
A protocol Foo in the namespace bar will cause the sources
  cppdir/FooParent.cpp, cppdir/FooChild.cpp
to be generated""")

options, files = op.parse_args()
_verbosity = options.verbosity
syncMsgList = options.syncMsgList
msgMetadata = options.msgMetadata
headersdir = options.headersdir
cppdir = options.cppdir
includedirs = [os.path.abspath(incdir) for incdir in options.includedirs]

if not len(files):
    op.error("No IPDL files specified")

ipcmessagestartpath = os.path.join(headersdir, 'IPCMessageStart.h')
ipc_msgtype_name_path = os.path.join(cppdir, 'IPCMessageTypeName.cpp')

log(2, 'Generated C++ headers will be generated relative to "%s"', headersdir)
log(2, 'Generated C++ sources will be generated in "%s"', cppdir)

allmessages = {}
allmessageprognames = []
allprotocols = []


def normalizedFilename(f):
    if f == '-':
        return '<stdin>'
    return f


log(2, 'Reading sync message list')
parser = RawConfigParser()
parser.readfp(open(options.syncMsgList))
syncMsgList = parser.sections()

for section in syncMsgList:
    if not parser.get(section, "description"):
        print('Error: Sync message %s lacks a description' % section, file=sys.stderr)
        sys.exit(1)

# Read message metadata. Right now we only have 'segment_capacity'
# for the standard segment size used for serialization.
log(2, 'Reading message metadata...')
msgMetadataConfig = RawConfigParser()
msgMetadataConfig.readfp(open(options.msgMetadata))

segmentCapacityDict = {}
for msgName in msgMetadataConfig.sections():
    if msgMetadataConfig.has_option(msgName, 'segment_capacity'):
        capacity = msgMetadataConfig.get(msgName, 'segment_capacity')
        segmentCapacityDict[msgName] = capacity

# First pass: parse and type-check all protocols
for f in files:
    log(2, os.path.basename(f))
    filename = normalizedFilename(f)
    if f == '-':
        fd = sys.stdin
    else:
        fd = open(f)

    specstring = fd.read()
    fd.close()

    ast = ipdl.parse(specstring, filename, includedirs=includedirs)
    if ast is None:
        print('Specification could not be parsed.', file=sys.stderr)
        sys.exit(1)

    log(2, 'checking types')
    if not ipdl.typecheck(ast):
        print('Specification is not well typed.', file=sys.stderr)
        sys.exit(1)

    if not ipdl.checkSyncMessage(ast, syncMsgList):
        print('Error: New sync IPC messages must be reviewed by an IPC peer and recorded in %s' % options.syncMsgList, file=sys.stderr)  # NOQA: E501
        sys.exit(1)

if not ipdl.checkFixedSyncMessages(parser):
    # Errors have alraedy been printed to stderr, just exit
    sys.exit(1)

# Second pass: generate code
for f in files:
    # Read from parser cache
    filename = normalizedFilename(f)
    ast = ipdl.parse(None, filename, includedirs=includedirs)
    ipdl.gencxx(filename, ast, headersdir, cppdir, segmentCapacityDict)

    if ast.protocol:
        allmessages[ast.protocol.name] = ipdl.genmsgenum(ast)
        allprotocols.append(ast.protocol.name)
        # e.g. PContent::RequestMemoryReport (not prefixed or suffixed.)
        for md in ast.protocol.messageDecls:
            allmessageprognames.append('%s::%s' % (md.namespace, md.decl.progname))

allprotocols.sort()

# Check if we have undefined message names in segmentCapacityDict.
# This is a fool-proof of the 'message-metadata.ini' file.
undefinedMessages = set(segmentCapacityDict.keys()) - set(allmessageprognames)
if len(undefinedMessages) > 0:
    print('Error: Undefined message names in message-metadata.ini:', file=sys.stderr)
    print(undefinedMessages, file=sys.stderr)
    sys.exit(1)

ipcmsgstart = StringIO()

print("""
// CODE GENERATED by ipdl.py. Do not edit.

#ifndef IPCMessageStart_h
#define IPCMessageStart_h

enum IPCMessageStart {
""", file=ipcmsgstart)

for name in allprotocols:
    print("  %sMsgStart," % name, file=ipcmsgstart)

print("""
  LastMsgIndex
};

static_assert(LastMsgIndex <= 65536, "need to update IPC_MESSAGE_MACRO");

#endif // ifndef IPCMessageStart_h
""", file=ipcmsgstart)

ipc_msgtype_name = StringIO()
print("""
// CODE GENERATED by ipdl.py. Do not edit.
#include <cstdint>

#include "mozilla/ipc/ProtocolUtils.h"
#include "IPCMessageStart.h"

using std::uint32_t;

namespace {

enum IPCMessages {
""", file=ipc_msgtype_name)

for protocol in sorted(allmessages.keys()):
    for (msg, num) in allmessages[protocol].idnums:
        if num:
            print("  %s = %s," % (msg, num), file=ipc_msgtype_name)
        elif not msg.endswith('End'):
            print("  %s__%s," % (protocol, msg), file=ipc_msgtype_name)

print("""
};

} // anonymous namespace

namespace IPC {

const char* StringFromIPCMessageType(uint32_t aMessageType)
{
  switch (aMessageType) {
""", file=ipc_msgtype_name)

for protocol in sorted(allmessages.keys()):
    for (msg, num) in allmessages[protocol].idnums:
        if num or msg.endswith('End'):
            continue
        print("""
  case %s__%s:
    return "%s::%s";""" % (protocol, msg, protocol, msg), file=ipc_msgtype_name)

print("""
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
""", file=ipc_msgtype_name)

for name in allprotocols:
    print("    case %sMsgStart:" % name, file=ipc_msgtype_name)
    print("      return \"%s\";" % name, file=ipc_msgtype_name)

print("""
  default:
    return "<unknown protocol id>";
  }
}

} // namespace ipc
} // namespace mozilla
""", file=ipc_msgtype_name)

ipdl.writeifmodified(ipcmsgstart.getvalue(), ipcmessagestartpath)
ipdl.writeifmodified(ipc_msgtype_name.getvalue(), ipc_msgtype_name_path)
