import difflib
import logging
import re
import urllib2
from util.commands import run_cmd, get_output
from util.hg import get_repo_name, make_hg_url
from subprocess import CalledProcessError

log = logging.getLogger(__name__)


def check_buildbot():
    """check if buildbot command works"""
    try:
        run_cmd(['buildbot', '--version'])
    except CalledProcessError:
        log.error("FAIL: buildbot command doesn't work", exc_info=True)
        raise


def find_version(contents, versionNumber):
    """Given an open readable file-handle look for the occurrence
       of the version # in the file"""
    ret = re.search(re.compile(re.escape(versionNumber), re.DOTALL), contents)
    return ret


def locale_diff(locales1, locales2):
    """ accepts two lists and diffs them both ways, returns any differences
    found """
    diff_list = [locale for locale in locales1 if not locale in locales2]
    diff_list.extend(locale for locale in locales2 if not locale in locales1)
    return diff_list


def get_buildbot_username_param():
    cmd = ['buildbot', 'sendchange', '--help']
    output = get_output(cmd)
    if "-W, --who=" in output:
        return "--who"
    else:
        return "--username"


def sendchange(branch, revision, username, master, products):
    """Send the change to buildbot to kick off the release automation"""
    if isinstance(products, basestring):
        products = [products]
    cmd = [
        'buildbot',
        'sendchange',
        get_buildbot_username_param(),
        username,
        '--master',
        master,
        '--branch',
        branch,
        '-p',
        'products:%s' % ','.join(products),
        '-p',
        'script_repo_revision:%s' % revision,
        'release_build'
    ]
    logging.info("Executing: %s" % cmd)
    run_cmd(cmd)


def verify_mozconfigs(mozconfig_pair, nightly_mozconfig_pair, platform,
                      mozconfigWhitelist={}):
    """Compares mozconfig to nightly_mozconfig and compare to an optional
    whitelist of known differences. mozconfig_pair and nightly_mozconfig_pair
    are pairs containing the mozconfig's identifier and the list of lines in
    the mozconfig."""

    # unpack the pairs to get the names, the names are just for
    # identifying the mozconfigs when logging the error messages
    mozconfig_name, mozconfig_lines = mozconfig_pair
    nightly_mozconfig_name, nightly_mozconfig_lines = nightly_mozconfig_pair

    missing_args = mozconfig_lines == [] or nightly_mozconfig_lines == []
    if missing_args:
        log.info("Missing mozconfigs to compare for %s" % platform)
        return False

    success = True

    diffInstance = difflib.Differ()
    diff_result = diffInstance.compare(mozconfig_lines, nightly_mozconfig_lines)
    diffList = list(diff_result)

    for line in diffList:
        clean_line = line[1:].strip()
        if (line[0] == '-' or line[0] == '+') and len(clean_line) > 1:
            # skip comment lines
            if clean_line.startswith('#'):
                continue
            # compare to whitelist
            message = ""
            if line[0] == '-':
                if platform in mozconfigWhitelist.get('release', {}):
                    if clean_line in \
                            mozconfigWhitelist['release'][platform]:
                        continue
            elif line[0] == '+':
                if platform in mozconfigWhitelist.get('nightly', {}):
                    if clean_line in \
                            mozconfigWhitelist['nightly'][platform]:
                        continue
                    else:
                        log.warning("%s not in %s %s!" % (
                            clean_line, platform,
                            mozconfigWhitelist['nightly'][platform]))
            else:
                log.error("Skipping line %s!" % line)
                continue
            message = "found in %s but not in %s: %s"
            if line[0] == '-':
                log.error(message % (mozconfig_name,
                                     nightly_mozconfig_name, clean_line))
            else:
                log.error(message % (nightly_mozconfig_name,
                                     mozconfig_name, clean_line))
            success = False
    return success
