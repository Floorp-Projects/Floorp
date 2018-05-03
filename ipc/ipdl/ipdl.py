# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import optparse, os, re, sys
from cStringIO import StringIO
import mozpack.path as mozpath
from ConfigParser import RawConfigParser

import ipdl

def log(minv, fmt, *args):
    if _verbosity >= minv:
        print fmt % args

# process command line

op = optparse.OptionParser(usage='ipdl.py [options] IPDLfiles...')
op.add_option('-I', '--include', dest='includedirs', default=[ ],
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
includedirs = [ os.path.abspath(incdir) for incdir in options.includedirs ]

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
        print >>sys.stderr, 'Specification could not be parsed.'
        sys.exit(1)

    log(2, 'checking types')
    if not ipdl.typecheck(ast):
        print >>sys.stderr, 'Specification is not well typed.'
        sys.exit(1)

    if not ipdl.checkSyncMessage(ast, syncMsgList):
        print >>sys.stderr, 'Error: New sync IPC messages must be reviewed by an IPC peer and recorded in %s' % options.syncMsgList
        sys.exit(1)

    if _verbosity > 2:
        log(3, '  pretty printed code:')
        ipdl.genipdl(ast, codedir)

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
        allprotocols.append('%sMsgStart' % ast.protocol.name)
        # e.g. PContent::RequestMemoryReport (not prefixed or suffixed.)
        for md in ast.protocol.messageDecls:
            allmessageprognames.append('%s::%s' % (md.namespace, md.decl.progname))

allprotocols.sort()

# Check if we have undefined message names in segmentCapacityDict.
# This is a fool-proof of the 'message-metadata.ini' file.
undefinedMessages = set(segmentCapacityDict.keys()) - set(allmessageprognames)
if len(undefinedMessages) > 0:
    print >>sys.stderr, 'Error: Undefined message names in message-metadata.ini:'
    print >>sys.stderr, undefinedMessages
    sys.exit(1)

ipcmsgstart = StringIO()

print >>ipcmsgstart, """
// CODE GENERATED by ipdl.py. Do not edit.

#ifndef IPCMessageStart_h
#define IPCMessageStart_h

enum IPCMessageStart {
"""

for name in allprotocols:
    print >>ipcmsgstart, "  %s," % name

print >>ipcmsgstart, """
  LastMsgIndex
};

static_assert(LastMsgIndex <= 65536, "need to update IPC_MESSAGE_MACRO");

#endif // ifndef IPCMessageStart_h
"""

ipc_msgtype_name = StringIO()
print >>ipc_msgtype_name, """
// CODE GENERATED by ipdl.py. Do not edit.
#include <cstdint>

#include "mozilla/ipc/ProtocolUtils.h"
#include "IPCMessageStart.h"

using std::uint32_t;

namespace {

enum IPCMessages {
"""

for protocol in sorted(allmessages.keys()):
    for (msg, num) in allmessages[protocol].idnums:
        if num:
            print >>ipc_msgtype_name, "  %s = %s," % (msg, num)
        elif not msg.endswith('End'):
            print >>ipc_msgtype_name,  "  %s__%s," % (protocol, msg)

print >>ipc_msgtype_name, """
};

} // anonymous namespace

namespace IPC {

const char* StringFromIPCMessageType(uint32_t aMessageType)
{
  switch (aMessageType) {
"""

for protocol in sorted(allmessages.keys()):
    for (msg, num) in allmessages[protocol].idnums:
        if num or msg.endswith('End'):
            continue
        print >>ipc_msgtype_name, """
  case %s__%s:
    return "%s::%s";""" % (protocol, msg, protocol, msg)

print >>ipc_msgtype_name, """
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
"""

ipdl.writeifmodified(ipcmsgstart.getvalue(), ipcmessagestartpath)
ipdl.writeifmodified(ipc_msgtype_name.getvalue(), ipc_msgtype_name_path)
