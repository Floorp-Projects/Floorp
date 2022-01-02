Mercurial Bundles
=================

If you have a poor network connection that is preventing ``hg clone`` from completing, you may want to try downloading a bundle of the repository you're interested in. This is useful since a file download, unlike ``hg clone``, can be resumed if the connection is interrupted. Once you have the bundle, staying up-to-date shouldn't take much time at all, if you keep up with it regularly.

This document explains the steps to setup the `mozilla-unified <https://hg.mozilla.org/mozilla-unified/>`__ repository using a bundle file. Be sure to replace "``mozilla-unified``" with the project you're working with as appropriate.

Download the bundle
-------------------

1. Open https://hg.cdn.mozilla.net/ :

It lists up-to-date bundles for some of the repositories listed at https://hg.mozilla.net/ .
Each row corresponds to each repository, and each column corresponds to each compression format.

2. Download the bundle file for the ``mozilla-unified`` repository:

Click the link in the "mozilla-unified" row, the "zstd (max)" column.

Setting up the repository
-------------------------

Once you have downloaded the repository bundle, follow the steps below to recreate the repository locally based upon that bundle.

1. Initialize a new repository (in a directory called ``mozilla-unified`` here):

.. code-block:: shell

               mkdir mozilla-unified
               hg init mozilla-unified

2. Un-bundle the bundle file to that repository:

Move the bundle file next to ``mozilla-unified`` directory, and rename it to ``bundle.hg``.

.. code-block:: shell

               cd mozilla-unified
               hg unbundle ../bundle.hg

Get comfortable. Grab a coffee (or your favorite tasty beverage). Maybe a nap. This unbundling process is going to take quite a lot of time.

3. Create the repository's config file ``.hg/hgrc``, and add the following lines, so that Mercurial will automatically know where to pull changes from future updates. You can open the template config file in your editor by running ``hg config --local --edit`` or ``EDITOR=<editor-of-your-choice> hg config --local --edit``

.. code-block:: shell

               [paths]
               default = https://hg.mozilla.org/mozilla-unified/

4. Update the repository to get all the changes since the bundle was created (this step also doubles as a check of the bundle integrity since if its contents are not exactly the same as what's in the official repository then the ``hg pull`` will fail):

.. code-block:: shell

               hg pull

5. Check out a working copy from your new up to date repository:

.. code-block:: shell

               hg update

You now have a clone of ``mozilla-unified`` that is identical to one made via ``hg clone``. You can adjust your build settings, or you can go straight ahead and build Firefox!

If at any point you are stuck, feel free to ask on Riot/Matrix at `https://chat.mozilla.org <https://chat.mozilla.org>`__
in `#introduction <https://chat.mozilla.org/#/room/#introduction:mozilla.org>`__ channel.
