# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Debugging JIT Compilations can be obscure without large context. This python
script provide commands to let GDB open an image viewer to display the graph of
any compilation, as they are executed within GDB.

This python script should be sourced within GDB after loading the python scripts
provided with SpiderMonkey.
"""

import gdb
import os
import subprocess
import tempfile
import time
import mozilla.prettyprinters
from mozilla.prettyprinters import pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)

# Cache information about the JSString type for this objfile.


class jsvmPrinterCache(object):
    def __init__(self):
        self.d = None

    def __getattr__(self, name):
        if self.d is None:
            self.initialize()
        return self.d[name]

    def initialize(self):
        self.d = {}
        self.d["char"] = gdb.lookup_type("char")


# Dummy class used to store the content of the type cache in the context of the
# iongraph command, which uses the jsvmLSprinter.


class ModuleCache(object):
    def __init__(self):
        self.mod_IonGraph = None


@pretty_printer("js::vm::LSprinter")
class jsvmLSprinter(object):
    def __init__(self, value, cache):
        self.value = value
        if not cache.mod_IonGraph:
            cache.mod_IonGraph = jsvmPrinterCache()
        self.cache = cache.mod_IonGraph

    def to_string(self):
        next = self.value["head_"]
        tail = self.value["tail_"]
        if next == 0:
            return ""
        res = ""
        while next != tail:
            chars = (next + 1).cast(self.cache.char.pointer())
            res = res + chars.string("ascii", "ignore", next["length"])
            next = next["next"]
        length = next["length"] - self.value["unused_"]
        chars = (next + 1).cast(self.cache.char.pointer())
        res = res + chars.string("ascii", "ignore", length)
        return res


def search_in_path(bin):
    paths = os.getenv("PATH", "")
    if paths == "":
        return ""
    for d in paths.split(":"):
        f = os.path.join(d, bin)
        if os.access(f, os.X_OK):
            return f
    return ""


class IonGraphBinParameter(gdb.Parameter):
    set_doc = "Set the path to iongraph binary, used by iongraph command."
    show_doc = "Show the path to iongraph binary, used by iongraph command."

    def get_set_string(self):
        return "Path to iongraph binary changed to: %s" % self.value

    def get_show_string(self, value):
        return "Path to iongraph binary set to: %s" % value

    def __init__(self):
        super(IonGraphBinParameter, self).__init__(
            "iongraph-bin", gdb.COMMAND_SUPPORT, gdb.PARAM_FILENAME
        )
        self.value = os.getenv("GDB_IONGRAPH", "")
        if self.value == "":
            self.value = search_in_path("iongraph")


class DotBinParameter(gdb.Parameter):
    set_doc = "Set the path to dot binary, used by iongraph command."
    show_doc = "Show the path to dot binary, used by iongraph command."

    def get_set_string(self):
        return "Path to dot binary changed to: %s" % self.value

    def get_show_string(self, value):
        return "Path to dot binary set to: %s" % value

    def __init__(self):
        super(DotBinParameter, self).__init__(
            "dot-bin", gdb.COMMAND_SUPPORT, gdb.PARAM_FILENAME
        )
        self.value = os.getenv("GDB_DOT", "")
        if self.value == "":
            self.value = search_in_path("dot")


class PngViewerBinParameter(gdb.Parameter):
    set_doc = "Set the path to a png viewer binary, used by iongraph command."
    show_doc = "Show the path to a png viewer binary, used by iongraph command."

    def get_set_string(self):
        return "Path to a png viewer binary changed to: %s" % self.value

    def get_show_string(self):
        return "Path to a png viewer binary set to: %s" % self.value

    def __init__(self):
        super(PngViewerBinParameter, self).__init__(
            "pngviewer-bin", gdb.COMMAND_SUPPORT, gdb.PARAM_FILENAME
        )
        self.value = os.getenv("GDB_PNGVIEWER", "")
        if self.value == "":
            self.value = search_in_path("xdg-open")


iongraph = IonGraphBinParameter()
dot = DotBinParameter()
pngviewer = PngViewerBinParameter()


class IonGraphCommand(gdb.Command):
    """Command used to display the current state of the MIR graph in a png
    viewer by providing an expression to access the MIRGenerator.
    """

    def __init__(self):
        super(IonGraphCommand, self).__init__(
            "iongraph", gdb.COMMAND_DATA, gdb.COMPLETE_EXPRESSION
        )
        self.typeCache = ModuleCache()

    def invoke(self, mirGenExpr, from_tty):
        """Call function from the graph spewer to populate the json printer with
        the content generated by the jsonSpewer. Then we read the json content
        from the jsonPrinter internal data, and gives that as input of iongraph
        command."""

        # From the MIRGenerator, find the graph spewer which contains both the
        # jsonPrinter (containing the result of the output), and the jsonSpewer
        # (continaining methods for spewing the graph).
        mirGen = gdb.parse_and_eval(mirGenExpr)
        jsonPrinter = mirGen["gs_"]["jsonPrinter_"]
        jsonSpewer = mirGen["gs_"]["jsonSpewer_"]
        graph = mirGen["graph_"]

        # These commands are doing side-effects which are saving the state of
        # the compiled code on the LSprinter dedicated for logging. Fortunately,
        # if you are using these gdb command, this probably means that other
        # ways of getting this content failed you already, so making a mess in
        # these logging strings should not cause much issues.
        gdb.parse_and_eval(
            "(*(%s*)(%s)).clear()"
            % (
                jsonPrinter.type,
                jsonPrinter.address,
            )
        )
        gdb.parse_and_eval(
            "(*(%s*)(%s)).beginFunction((JSScript*)0)"
            % (
                jsonSpewer.type,
                jsonSpewer.address,
            )
        )
        gdb.parse_and_eval(
            '(*(%s*)(%s)).beginPass("gdb")'
            % (
                jsonSpewer.type,
                jsonSpewer.address,
            )
        )
        gdb.parse_and_eval(
            "(*(%s*)(%s)).spewMIR((%s)%s)"
            % (
                jsonSpewer.type,
                jsonSpewer.address,
                graph.type,
                graph,
            )
        )
        gdb.parse_and_eval(
            "(*(%s*)(%s)).spewLIR((%s)%s)"
            % (
                jsonSpewer.type,
                jsonSpewer.address,
                graph.type,
                graph,
            )
        )
        gdb.parse_and_eval(
            "(*(%s*)(%s)).endPass()"
            % (
                jsonSpewer.type,
                jsonSpewer.address,
            )
        )
        gdb.parse_and_eval(
            "(*(%s*)(%s)).endFunction()"
            % (
                jsonSpewer.type,
                jsonSpewer.address,
            )
        )

        # Dump the content of the LSprinter containing the JSON view of the
        # graph into a python string.
        json = jsvmLSprinter(jsonPrinter, self.typeCache).to_string()

        # We are in the middle of the program execution and are messing up with
        # the state of the logging data. As this might not be the first time we
        # call beginFunction, we might have an extra comma at the beginning of
        # the output, just strip it.
        if json[0] == ",":
            json = json[1:]

        # Usually this is added by the IonSpewer.
        json = '{ "functions": [%s] }' % json

        # Display the content of the json with iongraph and other tools.
        self.displayMIRGraph(json)

    def displayMIRGraph(self, jsonStr):
        png = tempfile.NamedTemporaryFile()

        # start all processes in a shell-like equivalent of:
        #   iongraph < json | dot > tmp.png; xdg-open tmp.png
        i = subprocess.Popen(
            [iongraph.value, "--funcnum", "0", "--passnum", "0", "--out-mir", "-", "-"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
        )
        d = subprocess.Popen([dot.value, "-Tpng"], stdin=i.stdout, stdout=png)

        # Write the json file as the input of the iongraph command.
        i.stdin.write(jsonStr.encode("utf8"))
        i.stdin.close()
        i.stdout.close()

        # Wait for iongraph and dot, such that the png file contains all the
        # bits needed to by the png viewer.
        i.wait()
        d.communicate()[0]

        # Spawn & detach the png viewer, to which we give the name of the
        # temporary file.  Note, as we do not want to wait on the image viewer,
        # there is a minor race between the removal of the temporary file, which
        # would happen at the next garbage collection cycle, and the start of
        # the png viewer.  We could use a pipe, but unfortunately, this does not
        # seems to be supported by xdg-open.
        subprocess.Popen([pngviewer.value, png.name], stdin=None, stdout=None)
        time.sleep(1)


iongraph_cmd = IonGraphCommand()
