#!/usr/bin/python

import sys
if not sys.platform == "win32":
    raise Exception("This script was only meant for Windows.")

import os

def dos2unix(path):
    print "dos2unix: %s" % path

    inf = open(path, "r")
    data = inf.read()
    inf.close()


    outf = open(path, "wb")
    outf.write(data)
    outf.close()

adminfiles = [
    "Root",
    "Repository",
    "Entries",
    "Entries.Log",
    "Entries.Static",
    "Tag",
    "Notify",
    "Template"
    ]

def walkdirectory(path):
    if not os.path.exists(os.path.join(path, "CVS")):
        return

    print "Directory: %s" % path

    for f in adminfiles:
        cvsf = os.path.join(path, "CVS", f)
        if os.path.exists(cvsf):
            dos2unix(cvsf)

    entries = open(os.path.join(path, "CVS", "Entries"), "r")
    for entry in entries:
        if entry == "D\n":
            continue
        
        (type, filename, rev, date, flags, extra) = entry.split('/')
        if type == "D" or flags == "-kb" or rev[0] == "-":
            continue

        dos2unix(os.path.join(path, filename))

    # Now walk subdirectories
    for entry in os.listdir(path):
        subdir = os.path.join(path, entry)
        if os.path.isdir(subdir):
            walkdirectory(subdir)

topsrcdir = os.sep.join(os.path.abspath(__file__).split(os.sep)[:-3])

print """This command will convert the source tree at
%s 
to an MSYS-compatible (unix mode) source tree. You can run this
command multiple times safely. Are you sure you want to continue (Y/N)? """ % topsrcdir,
sys.stdout.flush()
print

ask = raw_input()
if len(ask) == 0 or (ask[0] != "y" and ask[0] != "Y"):
    raise Exception("User aborted action.")

walkdirectory(topsrcdir)
