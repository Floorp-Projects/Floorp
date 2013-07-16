"""Functions for interacting with hg"""
import os
import re
import subprocess
from urlparse import urlsplit
from ConfigParser import RawConfigParser

from util.commands import run_cmd, get_output, remove_path
from util.retry import retry

import logging
log = logging.getLogger(__name__)


class DefaultShareBase:
    pass
DefaultShareBase = DefaultShareBase()


class HgUtilError(Exception):
    pass


def _make_absolute(repo):
    if repo.startswith("file://"):
        path = repo[len("file://"):]
        repo = "file://%s" % os.path.abspath(path)
    elif "://" not in repo:
        repo = os.path.abspath(repo)
    return repo


def make_hg_url(hgHost, repoPath, protocol='https', revision=None,
                filename=None):
    """construct a valid hg url from a base hg url (hg.mozilla.org),
    repoPath, revision and possible filename"""
    base = '%s://%s' % (protocol, hgHost)
    repo = '/'.join(p.strip('/') for p in [base, repoPath])
    if not filename:
        if not revision:
            return repo
        else:
            return '/'.join([p.strip('/') for p in [repo, 'rev', revision]])
    else:
        assert revision
        return '/'.join([p.strip('/') for p in [repo, 'raw-file', revision, filename]])


def get_repo_name(repo):
    return repo.rstrip('/').split('/')[-1]


def get_repo_path(repo):
    repo = _make_absolute(repo)
    if repo.startswith("/"):
        return repo.lstrip("/")
    else:
        return urlsplit(repo).path.lstrip("/")


def get_revision(path):
    """Returns which revision directory `path` currently has checked out."""
    return get_output(['hg', 'parent', '--template', '{node|short}'], cwd=path)


def get_branch(path):
    return get_output(['hg', 'branch'], cwd=path).strip()


def get_branches(path):
    branches = []
    for line in get_output(['hg', 'branches', '-c'], cwd=path).splitlines():
        branches.append(line.split()[0])
    return branches


def is_hg_cset(rev):
    """Retruns True if passed revision represents a valid HG revision
    (long or short(er) 40 bit hex)"""
    try:
        int(rev, 16)
        return True
    except (TypeError, ValueError):
        return False


def hg_ver():
    """Returns the current version of hg, as a tuple of
    (major, minor, build)"""
    ver_string = get_output(['hg', '-q', 'version'])
    match = re.search("\(version ([0-9.]+)\)", ver_string)
    if match:
        bits = match.group(1).split(".")
        if len(bits) < 3:
            bits += (0,)
        ver = tuple(int(b) for b in bits)
    else:
        ver = (0, 0, 0)
    log.debug("Running hg version %s", ver)
    return ver


def purge(dest):
    """Purge the repository of all untracked and ignored files."""
    try:
        run_cmd(['hg', '--config', 'extensions.purge=', 'purge',
                 '-a', '--all', dest], cwd=dest)
    except subprocess.CalledProcessError, e:
        log.debug('purge failed: %s' % e)
        raise


def update(dest, branch=None, revision=None):
    """Updates working copy `dest` to `branch` or `revision`.  If neither is
    set then the working copy will be updated to the latest revision on the
    current branch.  Local changes will be discarded."""
    # If we have a revision, switch to that
    if revision is not None:
        cmd = ['hg', 'update', '-C', '-r', revision]
        run_cmd(cmd, cwd=dest)
    else:
        # Check & switch branch
        local_branch = get_output(['hg', 'branch'], cwd=dest).strip()

        cmd = ['hg', 'update', '-C']

        # If this is different, checkout the other branch
        if branch and branch != local_branch:
            cmd.append(branch)

        run_cmd(cmd, cwd=dest)
    return get_revision(dest)


def clone(repo, dest, branch=None, revision=None, update_dest=True,
          clone_by_rev=False, mirrors=None, bundles=None):
    """Clones hg repo and places it at `dest`, replacing whatever else is
    there.  The working copy will be empty.

    If `revision` is set, only the specified revision and its ancestors will
    be cloned.

    If `update_dest` is set, then `dest` will be updated to `revision` if
    set, otherwise to `branch`, otherwise to the head of default.

    If `mirrors` is set, will try and clone from the mirrors before
    cloning from `repo`.

    If `bundles` is set, will try and download the bundle first and
    unbundle it. If successful, will pull in new revisions from mirrors or
    the master repo. If unbundling fails, will fall back to doing a regular
    clone from mirrors or the master repo.

    Regardless of how the repository ends up being cloned, the 'default' path
    will point to `repo`.
    """
    if os.path.exists(dest):
        remove_path(dest)

    if bundles:
        log.info("Attempting to initialize clone with bundles")
        for bundle in bundles:
            if os.path.exists(dest):
                remove_path(dest)
            init(dest)
            log.info("Trying to use bundle %s", bundle)
            try:
                if not unbundle(bundle, dest):
                    remove_path(dest)
                    continue
                adjust_paths(dest, default=repo)
                # Now pull / update
                return pull(repo, dest, update_dest=update_dest,
                            mirrors=mirrors, revision=revision, branch=branch)
            except Exception:
                remove_path(dest)
                log.exception("Problem unbundling/pulling from %s", bundle)
                continue
        else:
            log.info("Using bundles failed; falling back to clone")

    if mirrors:
        log.info("Attempting to clone from mirrors")
        for mirror in mirrors:
            log.info("Cloning from %s", mirror)
            try:
                retval = clone(mirror, dest, branch, revision,
                               update_dest=update_dest, clone_by_rev=clone_by_rev)
                adjust_paths(dest, default=repo)
                return retval
            except:
                log.exception("Problem cloning from mirror %s", mirror)
                continue
        else:
            log.info("Pulling from mirrors failed; falling back to %s", repo)
            # We may have a partial repo here; mercurial() copes with that
            # We need to make sure our paths are correct though
            if os.path.exists(os.path.join(dest, '.hg')):
                adjust_paths(dest, default=repo)
            return mercurial(repo, dest, branch, revision, autoPurge=True,
                             update_dest=update_dest, clone_by_rev=clone_by_rev)

    cmd = ['hg', 'clone']
    if not update_dest:
        cmd.append('-U')

    if clone_by_rev:
        if revision:
            cmd.extend(['-r', revision])
        elif branch:
            # hg >= 1.6 supports -b branch for cloning
            ver = hg_ver()
            if ver >= (1, 6, 0):
                cmd.extend(['-b', branch])

    cmd.extend([repo, dest])
    run_cmd(cmd)

    if update_dest:
        return update(dest, branch, revision)


def common_args(revision=None, branch=None, ssh_username=None, ssh_key=None):
    """Fill in common hg arguments, encapsulating logic checks that depend on
       mercurial versions and provided arguments"""
    args = []
    if ssh_username or ssh_key:
        opt = ['-e', 'ssh']
        if ssh_username:
            opt[1] += ' -l %s' % ssh_username
        if ssh_key:
            opt[1] += ' -i %s' % ssh_key
        args.extend(opt)
    if revision:
        args.extend(['-r', revision])
    elif branch:
        if hg_ver() >= (1, 6, 0):
            args.extend(['-b', branch])
    return args


def pull(repo, dest, update_dest=True, mirrors=None, **kwargs):
    """Pulls changes from hg repo and places it in `dest`.

    If `update_dest` is set, then `dest` will be updated to `revision` if
    set, otherwise to `branch`, otherwise to the head of default.

    If `mirrors` is set, will try and pull from the mirrors first before
    `repo`."""

    if mirrors:
        for mirror in mirrors:
            try:
                return pull(mirror, dest, update_dest=update_dest, **kwargs)
            except:
                log.exception("Problem pulling from mirror %s", mirror)
                continue
        else:
            log.info("Pulling from mirrors failed; falling back to %s", repo)

    # Convert repo to an absolute path if it's a local repository
    repo = _make_absolute(repo)
    cmd = ['hg', 'pull']
    # Don't pass -r to "hg pull", except when it's a valid HG revision.
    # Pulling using tag names is dangerous: it uses the local .hgtags, so if
    # the tag has moved on the remote side you won't pull the new revision the
    # remote tag refers to.
    pull_kwargs = kwargs.copy()
    if 'revision' in pull_kwargs and \
       not is_hg_cset(pull_kwargs['revision']):
        del pull_kwargs['revision']

    cmd.extend(common_args(**pull_kwargs))

    cmd.append(repo)
    run_cmd(cmd, cwd=dest)

    if update_dest:
        branch = None
        if 'branch' in kwargs and kwargs['branch']:
            branch = kwargs['branch']
        revision = None
        if 'revision' in kwargs and kwargs['revision']:
            revision = kwargs['revision']
        return update(dest, branch=branch, revision=revision)

# Defines the places of attributes in the tuples returned by `out'
REVISION, BRANCH = 0, 1


def out(src, remote, **kwargs):
    """Check for outgoing changesets present in a repo"""
    cmd = ['hg', '-q', 'out', '--template', '{node} {branches}\n']
    cmd.extend(common_args(**kwargs))
    cmd.append(remote)
    if os.path.exists(src):
        try:
            revs = []
            for line in get_output(cmd, cwd=src).rstrip().split("\n"):
                try:
                    rev, branch = line.split()
                # Mercurial displays no branch at all if the revision is on
                # "default"
                except ValueError:
                    rev = line.rstrip()
                    branch = "default"
                revs.append((rev, branch))
            return revs
        except subprocess.CalledProcessError, inst:
            # In some situations, some versions of Mercurial return "1"
            # if no changes are found, so we need to ignore this return code
            if inst.returncode == 1:
                return []
            raise


def push(src, remote, push_new_branches=True, force=False, **kwargs):
    cmd = ['hg', 'push']
    cmd.extend(common_args(**kwargs))
    if force:
        cmd.append('-f')
    if push_new_branches:
        cmd.append('--new-branch')
    cmd.append(remote)
    run_cmd(cmd, cwd=src)


def mercurial(repo, dest, branch=None, revision=None, update_dest=True,
              shareBase=DefaultShareBase, allowUnsharedLocalClones=False,
              clone_by_rev=False, mirrors=None, bundles=None, autoPurge=False):
    """Makes sure that `dest` is has `revision` or `branch` checked out from
    `repo`.

    Do what it takes to make that happen, including possibly clobbering
    dest.

    If allowUnsharedLocalClones is True and we're trying to use the share
    extension but fail, then we will be able to clone from the shared repo to
    our destination.  If this is False, the default, then if we don't have the
    share extension we will just clone from the remote repository.

    If `clone_by_rev` is True, use 'hg clone -r <rev>' instead of 'hg clone'.
    This is slower, but useful when cloning repos with lots of heads.

    If `mirrors` is set, will try and use the mirrors before `repo`.

    If `bundles` is set, will try and download the bundle first and
    unbundle it instead of doing a full clone. If successful, will pull in
    new revisions from mirrors or the master repo. If unbundling fails, will
    fall back to doing a regular clone from mirrors or the master repo.
    """
    dest = os.path.abspath(dest)
    if shareBase is DefaultShareBase:
        shareBase = os.environ.get("HG_SHARE_BASE_DIR", None)

    log.info("Reporting hg version in use")
    cmd = ['hg', '-q', 'version']
    run_cmd(cmd, cwd='.')

    if shareBase:
        # Check that 'hg share' works
        try:
            log.info("Checking if share extension works")
            output = get_output(['hg', 'help', 'share'], dont_log=True)
            if 'no commands defined' in output:
                # Share extension is enabled, but not functional
                log.info("Disabling sharing since share extension doesn't seem to work (1)")
                shareBase = None
            elif 'unknown command' in output:
                # Share extension is disabled
                log.info("Disabling sharing since share extension doesn't seem to work (2)")
                shareBase = None
        except subprocess.CalledProcessError:
            # The command failed, so disable sharing
            log.info("Disabling sharing since share extension doesn't seem to work (3)")
            shareBase = None

    # Check that our default path is correct
    if os.path.exists(os.path.join(dest, '.hg')):
        hgpath = path(dest, "default")

        # Make sure that our default path is correct
        if hgpath != _make_absolute(repo):
            log.info("hg path isn't correct (%s should be %s); clobbering",
                     hgpath, _make_absolute(repo))
            remove_path(dest)

    # If the working directory already exists and isn't using share we update
    # the working directory directly from the repo, ignoring the sharing
    # settings
    if os.path.exists(dest):
        if not os.path.exists(os.path.join(dest, ".hg")):
            log.warning("%s doesn't appear to be a valid hg directory; clobbering", dest)
            remove_path(dest)
        elif not os.path.exists(os.path.join(dest, ".hg", "sharedpath")):
            try:
                if autoPurge:
                    purge(dest)
                return pull(repo, dest, update_dest=update_dest, branch=branch,
                            revision=revision,
                            mirrors=mirrors)
            except subprocess.CalledProcessError:
                log.warning("Error pulling changes into %s from %s; clobbering", dest, repo)
                log.debug("Exception:", exc_info=True)
                remove_path(dest)

    # If that fails for any reason, and sharing is requested, we'll try to
    # update the shared repository, and then update the working directory from
    # that.
    if shareBase:
        sharedRepo = os.path.join(shareBase, get_repo_path(repo))
        dest_sharedPath = os.path.join(dest, '.hg', 'sharedpath')

        if os.path.exists(sharedRepo):
            hgpath = path(sharedRepo, "default")

            # Make sure that our default path is correct
            if hgpath != _make_absolute(repo):
                log.info("hg path isn't correct (%s should be %s); clobbering",
                         hgpath, _make_absolute(repo))
                # we need to clobber both the shared checkout and the dest,
                # since hgrc needs to be in both places
                remove_path(sharedRepo)
                remove_path(dest)

        if os.path.exists(dest_sharedPath):
            # Make sure that the sharedpath points to sharedRepo
            dest_sharedPath_data = os.path.normpath(
                open(dest_sharedPath).read())
            norm_sharedRepo = os.path.normpath(os.path.join(sharedRepo, '.hg'))
            if dest_sharedPath_data != norm_sharedRepo:
                # Clobber!
                log.info("We're currently shared from %s, but are being requested to pull from %s (%s); clobbering",
                         dest_sharedPath_data, repo, norm_sharedRepo)
                remove_path(dest)

        try:
            log.info("Updating shared repo")
            mercurial(repo, sharedRepo, branch=branch, revision=revision,
                      update_dest=False, shareBase=None, clone_by_rev=clone_by_rev,
                      mirrors=mirrors, bundles=bundles, autoPurge=False)
            if os.path.exists(dest):
                if autoPurge:
                    purge(dest)
                return update(dest, branch=branch, revision=revision)

            try:
                log.info("Trying to share %s to %s", sharedRepo, dest)
                return share(sharedRepo, dest, branch=branch, revision=revision)
            except subprocess.CalledProcessError:
                if not allowUnsharedLocalClones:
                    # Re-raise the exception so it gets caught below.
                    # We'll then clobber dest, and clone from original repo
                    raise

                log.warning("Error calling hg share from %s to %s;"
                            "falling back to normal clone from shared repo",
                            sharedRepo, dest)
                # Do a full local clone first, and then update to the
                # revision we want
                # This lets us use hardlinks for the local clone if the OS
                # supports it
                clone(sharedRepo, dest, update_dest=False,
                      mirrors=mirrors, bundles=bundles)
                return update(dest, branch=branch, revision=revision)
        except subprocess.CalledProcessError:
            log.warning(
                "Error updating %s from sharedRepo (%s): ", dest, sharedRepo)
            log.debug("Exception:", exc_info=True)
            remove_path(dest)
    # end if shareBase

    if not os.path.exists(os.path.dirname(dest)):
        os.makedirs(os.path.dirname(dest))

    # Share isn't available or has failed, clone directly from the source
    return clone(repo, dest, branch, revision,
                 update_dest=update_dest, mirrors=mirrors,
                 bundles=bundles, clone_by_rev=clone_by_rev)


def apply_and_push(localrepo, remote, changer, max_attempts=10,
                   ssh_username=None, ssh_key=None, force=False):
    """This function calls `changer' to make changes to the repo, and tries
       its hardest to get them to the origin repo. `changer' must be a
       callable object that receives two arguments: the directory of the local
       repository, and the attempt number. This function will push ALL
       changesets missing from remote."""
    assert callable(changer)
    branch = get_branch(localrepo)
    changer(localrepo, 1)
    for n in range(1, max_attempts + 1):
        new_revs = []
        try:
            new_revs = out(src=localrepo, remote=remote,
                           ssh_username=ssh_username,
                           ssh_key=ssh_key)
            if len(new_revs) < 1:
                raise HgUtilError("No revs to push")
            push(src=localrepo, remote=remote, ssh_username=ssh_username,
                 ssh_key=ssh_key, force=force)
            return
        except subprocess.CalledProcessError, e:
            log.debug("Hit error when trying to push: %s" % str(e))
            if n == max_attempts:
                log.debug("Tried %d times, giving up" % max_attempts)
                for r in reversed(new_revs):
                    run_cmd(['hg', '--config', 'extensions.mq=', 'strip', '-n',
                             r[REVISION]], cwd=localrepo)
                raise HgUtilError("Failed to push")
            pull(remote, localrepo, update_dest=False,
                 ssh_username=ssh_username, ssh_key=ssh_key)
            # After we successfully rebase or strip away heads the push is
            # is attempted again at the start of the loop
            try:
                run_cmd(['hg', '--config', 'ui.merge=internal:merge',
                         'rebase'], cwd=localrepo)
            except subprocess.CalledProcessError, e:
                log.debug("Failed to rebase: %s" % str(e))
                update(localrepo, branch=branch)
                for r in reversed(new_revs):
                    run_cmd(['hg', '--config', 'extensions.mq=', 'strip', '-n',
                             r[REVISION]], cwd=localrepo)
                changer(localrepo, n + 1)


def share(source, dest, branch=None, revision=None):
    """Creates a new working directory in "dest" that shares history with
       "source" using Mercurial's share extension"""
    run_cmd(['hg', 'share', '-U', source, dest])
    return update(dest, branch=branch, revision=revision)


def cleanOutgoingRevs(reponame, remote, username, sshKey):
    outgoingRevs = retry(out, kwargs=dict(src=reponame, remote=remote,
                                          ssh_username=username,
                                          ssh_key=sshKey))
    for r in reversed(outgoingRevs):
        run_cmd(['hg', '--config', 'extensions.mq=', 'strip', '-n',
                 r[REVISION]], cwd=reponame)


def path(src, name='default'):
    """Returns the remote path associated with "name" """
    try:
        return get_output(['hg', 'path', name], cwd=src).strip()
    except subprocess.CalledProcessError:
        return None


def init(dest):
    """Initializes an empty repo in `dest`"""
    run_cmd(['hg', 'init', dest])


def unbundle(bundle, dest):
    """Unbundles the bundle located at `bundle` into `dest`.

    `bundle` can be a local file or remote url."""
    try:
        get_output(['hg', 'unbundle', bundle], cwd=dest, include_stderr=True)
        return True
    except subprocess.CalledProcessError:
        return False


def adjust_paths(dest, **paths):
    """Adjusts paths in `dest`/.hg/hgrc so that names in `paths` are set to
    paths[name].

    Note that any comments in the hgrc will be lost if changes are made to the
    file."""
    hgrc = os.path.join(dest, '.hg', 'hgrc')
    config = RawConfigParser()
    config.read(hgrc)

    if not config.has_section('paths'):
        config.add_section('paths')

    changed = False
    for path_name, path_value in paths.items():
        if (not config.has_option('paths', path_name) or
                config.get('paths', path_name) != path_value):
            changed = True
            config.set('paths', path_name, path_value)

    if changed:
        config.write(open(hgrc, 'w'))


def commit(dest, msg, user=None):
    cmd = ['hg', 'commit', '-m', msg]
    if user:
        cmd.extend(['-u', user])
    run_cmd(cmd, cwd=dest)
    return get_revision(dest)


def tag(dest, tags, user=None, msg=None, rev=None, force=None):
    cmd = ['hg', 'tag']
    if user:
        cmd.extend(['-u', user])
    if msg:
        cmd.extend(['-m', msg])
    if rev:
        cmd.extend(['-r', rev])
    if force:
        cmd.append('-f')
    cmd.extend(tags)
    run_cmd(cmd, cwd=dest)
    return get_revision(dest)
