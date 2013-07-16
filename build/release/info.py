from datetime import datetime
import os
from os import path
import re
import shutil
import sys
from urllib2 import urlopen

from release.paths import makeCandidatesDir

import logging
log = logging.getLogger(__name__)

# If version has two parts with no trailing specifiers like "rc", we
# consider it a "final" release for which we only create a _RELEASE tag.
FINAL_RELEASE_REGEX = "^\d+\.\d+$"


class ConfigError(Exception):
    pass


def getBuildID(platform, product, version, buildNumber, nightlyDir='nightly',
               server='stage.mozilla.org'):
    infoTxt = makeCandidatesDir(product, version, buildNumber, nightlyDir,
                                protocol='http', server=server) + \
        '%s_info.txt' % platform
    try:
        buildInfo = urlopen(infoTxt).read()
    except:
        log.error("Failed to retrieve %s" % infoTxt)
        raise

    for line in buildInfo.splitlines():
        key, value = line.rstrip().split('=', 1)
        if key == 'buildID':
            return value


def findOldBuildIDs(product, version, buildNumber, platforms,
                    nightlyDir='nightly', server='stage.mozilla.org'):
    ids = {}
    if buildNumber <= 1:
        return ids
    for n in range(1, buildNumber):
        for platform in platforms:
            if platform not in ids:
                ids[platform] = []
            try:
                id = getBuildID(platform, product, version, n, nightlyDir,
                                server)
                ids[platform].append(id)
            except Exception, e:
                log.error("Hit exception: %s" % e)
    return ids


def getReleaseConfigName(product, branch, version=None, staging=False):
    # XXX: Horrible hack for bug 842741. Because Thunderbird release
    # and esr both build out of esr17 repositories we'll bump the wrong
    # config for release without this.
    if product == 'thunderbird' and 'esr17' in branch and version and 'esr' not in version:
        cfg = 'release-thunderbird-comm-release.py'
    else:
        cfg = 'release-%s-%s.py' % (product, branch)
    if staging:
        cfg = 'staging_%s' % cfg
    return cfg


def readReleaseConfig(configfile, required=[]):
    return readConfig(configfile, keys=['releaseConfig'], required=required)


def readBranchConfig(dir, localconfig, branch, required=[]):
    shutil.copy(localconfig, path.join(dir, "localconfig.py"))
    oldcwd = os.getcwd()
    os.chdir(dir)
    sys.path.append(".")
    try:
        return readConfig("config.py", keys=['BRANCHES', branch],
                          required=required)
    finally:
        os.chdir(oldcwd)
        sys.path.remove(".")


def readConfig(configfile, keys=[], required=[]):
    c = {}
    execfile(configfile, c)
    for k in keys:
        c = c[k]
    items = c.keys()
    err = False
    for key in required:
        if key not in items:
            err = True
            log.error("Required item `%s' missing from %s" % (key, c))
    if err:
        raise ConfigError("Missing at least one item in config, see above")
    return c


def isFinalRelease(version):
    return bool(re.match(FINAL_RELEASE_REGEX, version))


def getBaseTag(product, version):
    product = product.upper()
    version = version.replace('.', '_')
    return '%s_%s' % (product, version)


def getTags(baseTag, buildNumber, buildTag=True):
    t = ['%s_RELEASE' % baseTag]
    if buildTag:
        t.append('%s_BUILD%d' % (baseTag, int(buildNumber)))
    return t


def getRuntimeTag(tag):
    return "%s_RUNTIME" % tag


def getReleaseTag(tag):
    return "%s_RELEASE" % tag


def generateRelbranchName(version, prefix='GECKO'):
    return '%s%s_%s_RELBRANCH' % (
        prefix, version.replace('.', ''),
        datetime.now().strftime('%Y%m%d%H'))


def getReleaseName(product, version, buildNumber):
    return '%s-%s-build%s' % (product.title(), version, str(buildNumber))


def getRepoMatchingBranch(branch, sourceRepositories):
    for sr in sourceRepositories.values():
        if branch in sr['path']:
            return sr
    return None


def fileInfo(filepath, product):
    """Extract information about a release file.  Returns a dictionary with the
    following keys set:
    'product', 'version', 'locale', 'platform', 'contents', 'format',
    'pathstyle'

    'contents' is one of 'complete', 'installer'
    'format' is one of 'mar' or 'exe'
    'pathstyle' is either 'short' or 'long', and refers to if files are all in
        one directory, with the locale as part of the filename ('short' paths,
        firefox 3.0 style filenames), or if the locale names are part of the
        directory structure, but not the file name itself ('long' paths,
        firefox 3.5+ style filenames)
    """
    try:
        # Mozilla 1.9.0 style (aka 'short') paths
        # e.g. firefox-3.0.12.en-US.win32.complete.mar
        filename = os.path.basename(filepath)
        m = re.match("^(%s)-([0-9.]+)\.([-a-zA-Z]+)\.(win32)\.(complete|installer)\.(mar|exe)$" % product, filename)
        if not m:
            raise ValueError("Could not parse: %s" % filename)
        return {'product': m.group(1),
                'version': m.group(2),
                'locale': m.group(3),
                'platform': m.group(4),
                'contents': m.group(5),
                'format': m.group(6),
                'pathstyle': 'short',
                'leading_path': '',
                }
    except:
        # Mozilla 1.9.1 and on style (aka 'long') paths
        # e.g. update/win32/en-US/firefox-3.5.1.complete.mar
        #      win32/en-US/Firefox Setup 3.5.1.exe
        ret = {'pathstyle': 'long'}
        if filepath.endswith('.mar'):
            ret['format'] = 'mar'
            m = re.search("update/(win32|linux-i686|linux-x86_64|mac|mac64)/([-a-zA-Z]+)/(%s)-(\d+\.\d+(?:\.\d+)?(?:\w+(?:\d+)?)?)\.(complete)\.mar" % product, filepath)
            if not m:
                raise ValueError("Could not parse: %s" % filepath)
            ret['platform'] = m.group(1)
            ret['locale'] = m.group(2)
            ret['product'] = m.group(3)
            ret['version'] = m.group(4)
            ret['contents'] = m.group(5)
            ret['leading_path'] = ''
        elif filepath.endswith('.exe'):
            ret['format'] = 'exe'
            ret['contents'] = 'installer'
            # EUballot builds use a different enough style of path than others
            # that we can't catch them in the same regexp
            if filepath.find('win32-EUballot') != -1:
                ret['platform'] = 'win32'
                m = re.search("(win32-EUballot/)([-a-zA-Z]+)/((?i)%s) Setup (\d+\.\d+(?:\.\d+)?(?:\w+\d+)?(?:\ \w+\ \d+)?)\.exe" % product, filepath)
                if not m:
                    raise ValueError("Could not parse: %s" % filepath)
                ret['leading_path'] = m.group(1)
                ret['locale'] = m.group(2)
                ret['product'] = m.group(3).lower()
                ret['version'] = m.group(4)
            else:
                m = re.search("(partner-repacks/[-a-zA-Z0-9_]+/|)(win32|mac|linux-i686)/([-a-zA-Z]+)/((?i)%s) Setup (\d+\.\d+(?:\.\d+)?(?:\w+(?:\d+)?)?(?:\ \w+\ \d+)?)\.exe" % product, filepath)
                if not m:
                    raise ValueError("Could not parse: %s" % filepath)
                ret['leading_path'] = m.group(1)
                ret['platform'] = m.group(2)
                ret['locale'] = m.group(3)
                ret['product'] = m.group(4).lower()
                ret['version'] = m.group(5)
        else:
            raise ValueError("Unknown filetype for %s" % filepath)

        return ret
