# Copyright (C) 2011-2013 Ms2ger
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.


def parseManifest(fd):
    def parseReftestLine(chunks):
        assert len(chunks) % 2 == 0
        reftests = []
        for i in range(2, len(chunks), 2):
            if not chunks[i] in ["==", "!="]:
                raise Exception("Misformatted reftest line " + line)
            reftests.append([chunks[i], chunks[1], chunks[i + 1]])
        return reftests

    dirs = []
    autotests = []
    reftests = []
    othertests = []
    supportfiles = []
    for fullline in fd:
        line = fullline.strip()
        if not line:
            continue

        chunks = line.split(" ")

        if chunks[0] == "MANIFEST":
            raise Exception("MANIFEST listed on line " + line)

        if chunks[0] == "dir":
            dirs.append(chunks[1])
        elif chunks[0] == "support" and chunks[1] == "dir":
            dirs.append(chunks[1])
        elif chunks[0] == "ref":
            if len(chunks) % 2:
                raise Exception("Missing chunk in line " + line)
            reftests.extend(parseReftestLine(chunks))
        elif chunks[0] == "support":
            supportfiles.append(chunks[1])
        elif chunks[0] in ["manual", "parser", "http"]:
            othertests.append(chunks[1])
        else:
            # automated
            autotests.append(chunks[0])
    return dirs, autotests, reftests, othertests, supportfiles


def parseManifestFile(path):
    fp = open(path)
    dirs, autotests, reftests, othertests, supportfiles = parseManifest(fp)
    fp.close()
    return dirs, autotests, reftests, othertests, supportfiles
