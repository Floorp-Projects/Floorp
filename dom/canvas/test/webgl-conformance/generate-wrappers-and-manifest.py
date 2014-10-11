#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Write a Mochitest manifest for WebGL conformance test files.

import os
import re

WRAPPER_TEMPLATE_FILEPATH = 'mochi-wrapper.html.template'
WRAPPERS_DIR = '_wrappers'
MANIFEST_TEMPLATE_FILEPATH = 'mochitest.ini.template'
MANIFEST_OUTPUT_FILEPATH = '../_webgl-conformance.ini'
ERRATA_FILEPATH = 'mochitest-errata.ini'
BASE_TEST_LIST_FILENAME = '00_test_list.txt'
FILE_PATH_PREFIX = os.path.basename(os.getcwd()) # 'webgl-conformance'

SUPPORT_DIRS = [
    'conformance',
    'resources',
]

EXTRA_SUPPORT_FILES = [
    'always-fail.html',
    'iframe-autoresize.js',
    'mochi-single.html',
    '../webgl-mochitest/driver-info.js',
]

ACCEPTABLE_ERRATA_KEYS = set([
  'skip-if',
])

GENERATED_HEADER = '''
# This is a GENERATED FILE. Do not edit it directly.
# Regenerated it by using `python generate-wrapper-and-manifest.py`.
# Mark skipped tests in mochitest-errata.ini.
# Mark failing tests in mochi-single.html.
'''.strip()

########################################################################
# GetTestList

def GetTestList():
    testList = []
    AccumTests('', BASE_TEST_LIST_FILENAME, testList)
    return testList

##############################
# Internals

def AccumTests(path, listFile, out_testList):
    listFilePath = os.path.join(path, listFile)
    assert os.path.exists(listFilePath), 'Bad `listFilePath`: ' + listFilePath

    with open(listFilePath, 'rb') as fIn:
        for line in fIn:
            line = line.rstrip()
            if not line:
                continue

            strippedLine = line.lstrip()
            if strippedLine.startswith('//'):
                continue
            if strippedLine.startswith('#'):
                continue
            if strippedLine.startswith('--'):
                continue

            split = line.rsplit('.', 1)
            assert len(split) == 2, 'Bad split for `line`: ' + line
            (name, ext) = split

            if ext == 'html':
                newTestFilePath = os.path.join(path, line)
                newTestFilePath = newTestFilePath.replace(os.sep, '/')
                out_testList.append(newTestFilePath)
                continue

            assert ext == 'txt', 'Bad `ext` on `line`: ' + line

            split = line.rsplit('/', 1)
            nextListFile = split[-1]
            nextPath = ''
            if len(split) != 1:
                nextPath = split[0]

            nextPath = os.path.join(path, nextPath)
            AccumTests(nextPath, nextListFile, out_testList)
            continue

    return

########################################################################
# Templates

def FillTemplate(inFilePath, templateDict, outFilePath):
    templateShell = ImportTemplate(inFilePath)
    OutputFilledTemplate(templateShell, templateDict, outFilePath)
    return


def ImportTemplate(inFilePath):
    with open(inFilePath, 'rb') as f:
        return TemplateShell(f)


def OutputFilledTemplate(templateShell, templateDict, outFilePath):
    spanStrList = templateShell.Fill(templateDict)

    with open(outFilePath, 'wb') as f:
        f.writelines(spanStrList)
    return

##############################
# Internals

def WrapWithIndent(lines, indentLen):
  split = lines.split('\n')
  if len(split) == 1:
      return lines

  ret = [split[0]]
  indentSpaces = ' ' * indentLen
  for line in split[1:]:
      ret.append(indentSpaces + line)

  return '\n'.join(ret)


templateRE = re.compile('(%%.*?%%)')
assert templateRE.split('  foo = %%BAR%%;') == ['  foo = ', '%%BAR%%', ';']


class TemplateShellSpan:
    def __init__(self, span):
        self.span = span

        self.isLiteralSpan = True
        if self.span.startswith('%%') and self.span.endswith('%%'):
            self.isLiteralSpan = False
            self.span = self.span[2:-2]

        return


    def Fill(self, templateDict, indentLen):
        if self.isLiteralSpan:
            return self.span

        assert (self.span in templateDict,
                '\'' + self.span + '\' not in dict!')

        filling = templateDict[self.span]

        return WrapWithIndent(filling, indentLen)


class TemplateShell:
    def __init__(self, iterableLines):
        spanList = []
        curLiteralSpan = []
        for line in iterableLines:
            split = templateRE.split(line)

            for cur in split:
                isTemplateSpan = cur.startswith('%%') and cur.endswith('%%')
                if not isTemplateSpan:
                    curLiteralSpan.append(cur)
                    continue

                if curLiteralSpan:
                    span = ''.join(curLiteralSpan)
                    span = TemplateShellSpan(span)
                    spanList.append(span)
                    curLiteralSpan = []

                assert len(cur) >= 4

                span = TemplateShellSpan(cur)
                spanList.append(span)
                continue
            continue

        if curLiteralSpan:
            span = ''.join(curLiteralSpan)
            span = TemplateShellSpan(span)
            spanList.append(span)

        self.spanList = spanList
        return


    # Returns spanStrList.
    def Fill(self, templateDict):
        indentLen = 0
        ret = []
        for span in self.spanList:
            span = span.Fill(templateDict, indentLen)
            ret.append(span)

            # Get next `indentLen`.
            try:
                lineStartPos = span.rindex('\n') + 1

                # let span = 'foo\nbar'
                # len(span) is 7
                # lineStartPos is 4
                indentLen = len(span) - lineStartPos
            except ValueError:
                indentLen += len(span)
            continue

        return ret

########################################################################
# Output

def WriteWrappers(testWebPathList):
    templateShell = ImportTemplate(WRAPPER_TEMPLATE_FILEPATH)

    if not os.path.exists(WRAPPERS_DIR):
        os.mkdir(WRAPPERS_DIR)
    assert os.path.isdir(WRAPPERS_DIR)

    wrapperManifestPathList = []
    for testWebPath in testWebPathList:
        # Mochitests must start with 'test_' or similar, or the test
        # runner will ignore our tests.
        # The error text is "is not a valid test".
        wrapperFilePath = 'test_' + testWebPath.replace('/', '__')
        wrapperFilePath = os.path.join(WRAPPERS_DIR, wrapperFilePath)

        templateDict = {
            'TEST_PATH': testWebPath,
        }

        print('Writing \'' + wrapperFilePath + '\'')
        OutputFilledTemplate(templateShell, templateDict,
                             wrapperFilePath)

        wrapperManifestPath = wrapperFilePath.replace(os.sep, '/')
        wrapperManifestPathList.append(wrapperManifestPath)
        continue

    return wrapperManifestPathList


def PathFromManifestDir(path):
    print('path: ' + path)
    return os.path.join(FILE_PATH_PREFIX, path)


def WriteManifest(wrapperManifestPathList, supportFilePathList):
    errataMap = LoadErrata()

    # DEFAULT_ERRATA
    defaultHeader = '[DEFAULT]'
    defaultErrataStr = ''
    if defaultHeader in errataMap:
        defaultErrataStr = '\n'.join(errataMap[defaultHeader])
        del errataMap[defaultHeader]

    # SUPPORT_FILES
    supportFilePathList = sorted(supportFilePathList)
    supportFilePathList = [PathFromManifestDir(x) for x in supportFilePathList]
    supportFilesStr = '\n'.join(supportFilePathList)

    # MANIFEST_TESTS
    manifestTestLineList = []
    for wrapperManifestPath in wrapperManifestPathList:
        header = '[' + wrapperManifestPath + ']'
        transformedHeader = '[' + PathFromManifestDir(wrapperManifestPath) + ']'
        # header: '[foo.html]'
        # transformedHeader: '[webgl-conformance/foo.html]'

        manifestTestLineList.append(transformedHeader)

        if not header in errataMap:
            continue

        errataLineList = errataMap[header]
        del errataMap[header]
        manifestTestLineList += errataLineList
        continue

    assert not errataMap, 'Errata left in map: {}'.format(str(errataMap))

    manifestTestsStr = '\n'.join(manifestTestLineList)

    # Fill the template.
    templateDict = {
        'HEADER': GENERATED_HEADER,
        'DEFAULT_ERRATA': defaultErrataStr,
        'SUPPORT_FILES': supportFilesStr,
        'MANIFEST_TESTS': manifestTestsStr,
    }

    FillTemplate(MANIFEST_TEMPLATE_FILEPATH, templateDict,
                 MANIFEST_OUTPUT_FILEPATH)
    return

##############################
# Internals

kManifestHeaderRegex = re.compile(r'\[[^\]]*?\]')


def LoadErrata():
    nodeMap = {}

    nodeHeader = None
    nodeLineList = []
    lineNum = 0
    with open(ERRATA_FILEPATH, 'rb') as f:
        for line in f:
            lineNum += 1
            line = line.rstrip()
            cur = line.lstrip()
            if cur.startswith('#'):
                continue

            if not cur:
                continue

            if not cur.startswith('['):
                split = cur.split('=')
                key = split[0].strip()
                if not key in ACCEPTABLE_ERRATA_KEYS:
                    text = 'Unacceptable errata key on line {}: {}'
                    text = text.format(str(lineNum), key)
                    raise Exception(text)
                nodeLineList.append(line)
                continue

            match = kManifestHeaderRegex.search(cur)
            assert match, line

            nodeHeader = match.group()
            assert not nodeHeader in nodeMap, 'Duplicate header: ' + nodeHeader
            nodeLineList = []
            nodeMap[nodeHeader] = nodeLineList
            continue

    return nodeMap

########################################################################

def GetSupportFileList():
    ret = []
    for supportDir in SUPPORT_DIRS:
        ret += GetFilePathListForDir(supportDir)

    ret += EXTRA_SUPPORT_FILES

    return ret


def GetFilePathListForDir(baseDir):
    ret = []
    for root, folders, files in os.walk(baseDir):
        for f in files:
            filePath = os.path.join(root, f)
            filePath = filePath.replace(os.sep, '/')
            ret.append(filePath)

    return ret


if __name__ == '__main__':
    fileDir = os.path.dirname(__file__)
    assert not fileDir, 'Run this file from its directory, not ' + fileDir

    testWebPathList = GetTestList()

    wrapperFilePathList = WriteWrappers(testWebPathList)

    supportFilePathList = GetSupportFileList()
    WriteManifest(wrapperFilePathList, supportFilePathList)

    print('Done!')
