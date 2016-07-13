#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Write a Mochitest manifest for WebGL conformance test files.

import os
import re

CURRENT_VERSION = '1.0.3'

# All paths in this file are based where this file is run.
WRAPPER_TEMPLATE_FILE = 'mochi-wrapper.html.template'
MANIFEST_TEMPLATE_FILE = 'mochitest.ini.template'
ERRATA_FILE = 'mochitest-errata.ini'
DEST_MANIFEST_PATHSTR = 'generated-mochitest.ini'

BASE_TEST_LIST_PATHSTR = 'checkout/00_test_list.txt'
GENERATED_PATHSTR = 'generated'

SUPPORT_DIRS = [
    'checkout/conformance',
    'checkout/resources',
]

EXTRA_SUPPORT_FILES = [
    'always-fail.html',
    'iframe-passthrough.css',
    'mochi-single.html',
]

ACCEPTABLE_ERRATA_KEYS = set([
  'fail-if',
  'skip-if',
  'subsuite',
])

########################################################################
# GetTestList

def GetTestList():
    split = BASE_TEST_LIST_PATHSTR.rsplit('/', 1)
    basePath = '.'
    testListFile = split[-1]
    if len(split) == 2:
        basePath = split[0]

    curVersion = CURRENT_VERSION
    testList = ['always-fail.html']
    AccumTests(basePath, testListFile, curVersion, testList)

    testList = [os.path.relpath(x, basePath).replace(os.sep, '/') for x in testList]
    return testList

##############################
# Internals

def IsVersionLess(a, b):
    aSplit = [int(x) for x in a.split('.')]
    bSplit = [int(x) for x in b.split('.')]

    while len(aSplit) < len(bSplit):
        aSplit.append(0)

    while len(aSplit) > len(bSplit):
        bSplit.append(0)

    for i in range(len(aSplit)):
        aVal = aSplit[i]
        bVal = bSplit[i]

        if aVal == bVal:
            continue

        return aVal < bVal

    return False


def AccumTests(pathStr, listFile, curVersion, out_testList):
    listPathStr = pathStr + '/' + listFile

    listPath = listPathStr.replace('/', os.sep)
    assert os.path.exists(listPath), 'Bad `listPath`: ' + listPath

    with open(listPath, 'rb') as fIn:
        lineNum = 0
        for line in fIn:
            lineNum += 1

            line = line.rstrip()
            if not line:
                continue

            curLine = line.lstrip()
            if curLine.startswith('//'):
                continue
            if curLine.startswith('#'):
                continue

            shouldSkip = False
            while curLine.startswith('--'): # '--min-version 1.0.2 foo.html'
                (flag, curLine) = curLine.split(' ', 1)
                if flag == '--min-version':
                    (refVersion, curLine) = curLine.split(' ', 1)
                    if IsVersionLess(curVersion, refVersion):
                        shouldSkip = True
                        break
                elif flag == '--max-version':
                    (refVersion, curLine) = curLine.split(' ', 1)
                    if IsVersionLess(refVersion, curVersion):
                        shouldSkip = True
                        break
                elif flag == '--slow':
                    continue # TODO
                else:
                    text = 'Unknown flag \'{}\': {}:{}: {}'.format(flag, listPath,
                                                                   lineNum, line)
                    assert False, text
                continue

            if shouldSkip:
                continue

            split = curLine.rsplit('.', 1)
            assert len(split) == 2, 'Bad split for `line`: ' + line
            (name, ext) = split

            if ext == 'html':
                newTestFilePathStr = pathStr + '/' + curLine
                out_testList.append(newTestFilePathStr)
                continue

            assert ext == 'txt', 'Bad `ext` on `line`: ' + line

            split = curLine.rsplit('/', 1)
            nextListFile = split[-1]
            nextPathStr = ''
            if len(split) != 1:
                nextPathStr = split[0]

            nextPathStr = pathStr + '/' + nextPathStr
            AccumTests(nextPathStr, nextListFile, curVersion, out_testList)
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

        assert self.span in templateDict, '\'' + self.span + '\' not in dict!'

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

def WriteWrappers(testWebPathStrList):
    templateShell = ImportTemplate(WRAPPER_TEMPLATE_FILE)

    generatedDirPath = GENERATED_PATHSTR.replace('/', os.sep)
    if not os.path.exists(generatedDirPath):
        os.mkdir(generatedDirPath)
    assert os.path.isdir(generatedDirPath)

    wrapperManifestPathStrList = []
    for testWebPathStr in testWebPathStrList:
        # Mochitests must start with 'test_' or similar, or the test
        # runner will ignore our tests.
        # The error text is "is not a valid test".
        wrapperFilePathStr = 'test_' + testWebPathStr.replace('/', '__')
        wrapperFilePathStr = GENERATED_PATHSTR + '/' + wrapperFilePathStr
        wrapperManifestPathStrList.append(wrapperFilePathStr)

        templateDict = {
            'HEADER': '<!-- GENERATED FILE, DO NOT EDIT -->',
            'TEST_PATH': testWebPathStr,
        }

        print 'Adding wrapper: ' + wrapperFilePathStr
        OutputFilledTemplate(templateShell, templateDict, wrapperFilePathStr)
        continue

    return wrapperManifestPathStrList


kManifestRelPathStr = os.path.relpath('.', os.path.dirname(DEST_MANIFEST_PATHSTR))
kManifestRelPathStr = kManifestRelPathStr.replace(os.sep, '/')

def ManifestPathStr(pathStr):
    pathStr = kManifestRelPathStr + '/' + pathStr
    return os.path.normpath(pathStr).replace(os.sep, '/')


def WriteManifest(wrapperPathStrList, supportPathStrList):
    destPathStr = DEST_MANIFEST_PATHSTR
    print 'Generating manifest: ' + destPathStr

    errataMap = LoadErrata()

    # DEFAULT_ERRATA
    defaultSectionName = 'DEFAULT'

    defaultSectionLines = []
    if defaultSectionName in errataMap:
        defaultSectionLines = errataMap[defaultSectionName]
        del errataMap[defaultSectionName]

    defaultSectionStr = '\n'.join(defaultSectionLines)

    # SUPPORT_FILES
    supportPathStrList = [ManifestPathStr(x) for x in supportPathStrList]
    supportPathStrList = sorted(supportPathStrList)
    supportFilesStr = '\n'.join(supportPathStrList)

    # MANIFEST_TESTS
    manifestTestLineList = []
    wrapperPathStrList = sorted(wrapperPathStrList)
    for wrapperPathStr in wrapperPathStrList:
        #print 'wrapperPathStr: ' + wrapperPathStr

        wrapperManifestPathStr = ManifestPathStr(wrapperPathStr)
        sectionName = '[' + wrapperManifestPathStr + ']'
        manifestTestLineList.append(sectionName)

        if wrapperPathStr in errataMap:
            manifestTestLineList += errataMap[wrapperPathStr]
            del errataMap[wrapperPathStr]

        continue

    if errataMap:
        print 'Errata left in map:'
        for x in errataMap.keys():
            print ' '*4 + x
        assert False

    manifestTestsStr = '\n'.join(manifestTestLineList)

    # Fill the template.
    templateDict = {
        'DEFAULT_ERRATA': defaultSectionStr,
        'SUPPORT_FILES': supportFilesStr,
        'MANIFEST_TESTS': manifestTestsStr,
    }

    destPath = destPathStr.replace('/', os.sep)
    FillTemplate(MANIFEST_TEMPLATE_FILE, templateDict, destPath)
    return

##############################
# Internals

kManifestHeaderRegex = re.compile(r'[[]([^]]*)[]]')

def LoadINI(path):
    curSectionName = None
    curSectionMap = {}

    lineNum = 0

    ret = {}
    ret[curSectionName] = (lineNum, curSectionMap)

    with open(path, 'rb') as f:
        for line in f:
            lineNum += 1

            line = line.strip()
            if not line:
                continue

            if line[0] in [';', '#']:
                continue

            if line[0] == '[':
                assert line[-1] == ']', '{}:{}'.format(path, lineNum)

                curSectionName = line[1:-1]
                assert curSectionName not in ret, 'Line {}: Duplicate section: {}'.format(lineNum, line)

                curSectionMap = {}
                ret[curSectionName] = (lineNum, curSectionMap)
                continue

            split = line.split('=', 1)
            key = split[0].strip()
            val = ''
            if len(split) == 2:
                val = split[1].strip()

            curSectionMap[key] = (lineNum, val)
            continue

    return ret


def LoadErrata():
    iniMap = LoadINI(ERRATA_FILE)

    ret = {}

    for (sectionName, (sectionLineNum, sectionMap)) in iniMap.iteritems():
        curLines = []

        if sectionName == None:
            continue
        elif sectionName != 'DEFAULT':
            path = sectionName.replace('/', os.sep)
            assert os.path.exists(path), 'Line {}: {}'.format(sectionLineNum, sectionName)

        for (key, (lineNum, val)) in sectionMap.iteritems():
            assert key in ACCEPTABLE_ERRATA_KEYS, 'Line {}: {}'.format(lineNum, key)

            curLine = '{} = {}'.format(key, val)
            curLines.append(curLine)
            continue

        ret[sectionName] = curLines
        continue

    return ret

########################################################################

def GetSupportFileList():
    ret = EXTRA_SUPPORT_FILES[:]

    for pathStr in SUPPORT_DIRS:
        ret += GetFilePathListForDir(pathStr)
        continue

    for pathStr in ret:
        path = pathStr.replace('/', os.sep)
        assert os.path.exists(path), path + '\n\n\n' + 'pathStr: ' + str(pathStr)
        continue

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

    testPathStrList = GetTestList()
    wrapperPathStrList = WriteWrappers(testPathStrList)

    supportPathStrList = GetSupportFileList()
    WriteManifest(wrapperPathStrList, supportPathStrList)

    print('Done!')
