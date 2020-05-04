Mercurial Overview
==================

Mercurial is a source-code management tool which allows users to keep track of changes to the source code locally and share their changes with others.
We use it for the development of Firefox.

Installation
------------

See `Mercurial Page <https://www.mercurial-scm.org/downloads>`__ for installation.

`More information <https://developer.mozilla.org/docs/Mozilla/Mercurial/Installing_Mercurial>`__


Using `hg clone`
----------------

If you are not worried about network interruptions, then you can simply
use Mercurial to directly clone the repository you're interested in
using its URL, as given above. For example, to use the command line to
clone ``mozilla-central`` into a directory called ``firefox-source``,
you would use the following:

.. code-block:: shell

   hg clone https://hg.mozilla.org/mozilla-central/ firefox-source
   cd firefox-source

Using Mercurial bundles
-----------------------

If you are worried that your Internet connection is not fast or robust
enough to download such a large amount of data all in one go without
being interrupted, then you are recommended to download one of the
If you have any network connection issues and cannot clone with command, try :ref:`Mercurial bundles <Mercurial bundles>`. If interrupted, they can be resumed (continued without downloading 
from the beginning) if the app you're using to download supports it. For
example, in Firefox you would right click on the download and select
`Resume` once your connection to the Internet was reestablished.

Basic configuration
-------------------

You should configure Mercurial before submitting patches to Mozilla.

If you will be pulling the Firefox source code or one of the derived repositories, the easiest way to configure Mercurial is to run the vcs-setup mach command:

.. code-block:: shell

    $ ./mach vcs-setup

This command starts an interactive wizard that will help ensure your Mercurial is configured with the latest recommended settings. This command will not change any files on your machine without your consent.


Other configuration tips
------------------------

If you don't have the Firefox source code available, you should edit your Mercurial configuration file to look like the following:

.. code-block:: shell

    [ui]
    username = Your Real Name <user@example.com>
    merge = your-merge-program (or internal:merge)

    [diff]
    git = 1
    showfunc = 1
    unified = 8

    [defaults]
    commit = -v

On Windows, these settings can be added to `$HOME\.hgrc` or `$HOME\Mercurial.ini`, or, if you'd like global settings, `C:\mozilla-build\hg\Mercurial.ini`
or `C:\Program Files\Mercurial\Mercurial.ini.` On UNIX-like systems, they should be in your `$HOME/.hgrc` file.

You can configure the editor to use for commit messages using the `editor` option in the `[ui]` section or by setting the `EDITOR` environment variable.

If you are trying to access the repository through a proxy server, see `these
instructions <http://www.selenic.com/mercurial/hgrc.5.html#http-proxy>`__


Selecting a repository (tree)
-----------------------------

There are multiple hg repositories hosted at mozilla.org to choose from.
A summary of the main trees is given below, but see
https://hg.mozilla.org/ for the full list.

mozilla-central
---------------

This is the main development tree for Firefox. Most developers write
patches against mozilla-central.

URL: https://hg.mozilla.org/mozilla-central/


mozilla-beta
------------

The source for the current beta version of Firefox (and the next and all
previous betas). This code represents the expected next release of the
Firefox browser, and should be pretty stable.

URL: https://hg.mozilla.org/releases/mozilla-beta/

mozilla-release
---------------

The source for the current release of Firefox (and the next and all
previous releases).

URL: https://hg.mozilla.org/releases/mozilla-release/


L10n repos
----------

Mainly useful for localizers working on localizing Firefox. Code for all
l10n projects lives here and is organized into separate repos that (in
most cases) have the locale's two character ISO code. To get the repo
that you need look for the repo you're interested in on the following
page.

URL: https://hg.mozilla.org/l10n-central/

Unified Repositories
--------------------

It is common for advanced users to want to interact with more than one
firefox repository. If you get to the point where having individual
copies of repositories is annoying you, then see
https://mozilla-version-control-tools.readthedocs.org/en/latest/hgmozilla/unifiedrepo.html
for instructions on doing this efficiently.

Selecting a revision to build
-----------------------------

Most of the time the `tip` revision of most repositories will build
without issue. If you are worried about it not, then you may want to
`get the latest revision that has passed the automatic
tests <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Source_Code/LatestPassingSource>`__.

Building
--------

By default with no configuration a similar-to-release build is done. If
you wish you can
`configure <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Configuring_Build_Options>`__
the build using a ``.mozconfig`` file and ``mach build``.
Different OSs have different prerequisites for a successful build,
please refer to the `build
documentation <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions>`__
to verify they are available on your build machine.

Extensions
----------

There's a number of extensions you can enable. See http://mercurial.selenic.com/wiki/UsingExtensions. Almost everyone should probably enable the following, most of them are enabled by ``mach boostrap``:

#. color - Colorize terminal output
#. histedit - Provides git rebase --interactive behavior.
#. progress - Draw progress bars on long-running operations.
#. rebase - Ability to easily rebase patches on top of other heads.
#. evolve - Enable and enhance the inprogress ChangesetEvolution work.
#. firefoxtree - Enhances the interaction with Firefox repositories.
#. transplant - Easily move patches between repositories, branches, etc.

These can all be turned on by just adding this to your `.hgrc` file:

.. code-block:: shell

    [extensions]
    color =
    rebase =
    histedit =
    progress =
    firefoxtree =
    evolve =
    transplant =

In addition, there are some 3rd party extensions that are incredibly
useful for basic development:

`mozext <https://hg.mozilla.org/hgcustom/version-control-tools/file/default/hgext/mozext>`__
   Mozilla-specific functionality to aid in developing Firefox/Gecko.

`trychooser <https://github.com/pbiggar/trychooser>`__
   Automatically creates a try commit message and then pushes changes to
   Mozilla's Try infrastructure. Just run:

.. code-block:: shell

    hg trychooser

Configuring the try repository
------------------------------

About `Try Server <Try Server>`__.

Further reading
---------------

The `Mercurial
tag <https://developer.mozilla.org/docs/tag/Mercurial>`__ lists
the Mercurial-related articles on MDN.

And on wiki.mozilla.org, these helpful pages:

-  `Creating Mercurial User
   Repositories <https://developer.mozilla.org/docs/Creating_Mercurial_User_Repositories>`__ , If you have a LDAP account that allows you to push to hg.mozilla.org
   you can also create your own user repositories on the server to share
   work.

Learning to use Mercurial
-------------------------

If you are new to Mercurial, you should start with the `official guide <https://www.mercurial-scm.org/guide>`__.

Then, move on to `Mercurial basics <https://developer.mozilla.org/docs/Mercurial_basics>`__ and the `version control tool docs <https://mozilla-version-control-tools.readthedocs.io/en/latest/hgmozilla/>`__ for Mozilla-centric Mercurial information.

More documentation about mercurial
----------------------------------
https://developer.mozilla.org/docs/Mozilla/Developer_guide/Source_Code/Mercurial

https://developer.mozilla.org/docs/Mozilla/Mercurial

https://developer.mozilla.org/docs/Mozilla/Mercurial/Basics
