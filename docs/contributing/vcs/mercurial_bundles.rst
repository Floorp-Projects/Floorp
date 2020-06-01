Mercurial Bundles
=================

If you have a poor network connection that is preventing ``hg clone`` from completing, you may want to try downloading a bundle of the repository you're interested in. This is useful since a file download, unlike ``hg clone``, can be resumed if the connection is interrupted. Once you have the bundle, staying up-to-date shouldn't take much time at all, if you keep up with it regularly.


Clone the sources
-----------------

Up-to-date bundles of some of the repositories listed at https://hg.mozilla.org/ are available on a CDN at https://hg.cdn.mozilla.net/.

If you have Mercurial 4.1 (released March 2017) or later, download the zstd bundle for the repository you're interested in. The zstd bundles are faster to download (smaller) and faster to decompress.

Setting up the repository
-------------------------

Once you have downloaded the repository bundle, follow the steps below to recreate the repository locally based upon that bundle. Be sure to replace "``mozilla-central``" with the project you're working with as appropriate.

1. Initialize a new repository (in a directory called ``mozilla-central`` here):

.. code-block:: shell

               mkdir mozilla-central
               hg init mozilla-central

2. Un-bundle the bundle file to that repository:

To use the below command in Windows, export the ``\path\to\hg`` and invoke the command from command prompt.

On Linux/Mac click on the properties of the file and you can find the path. In case the name of the file is not bundle.hg rename it.

.. code-block:: shell

               cd mozilla-central
               hg unbundle /path/to/your/bundle.hg

Get comfortable. Grab a coffee (or your favorite tasty beverage). Maybe a nap. This unbundling process is going to take quite a lot of time.

3. Add the following lines to the repository's config file (``.hg/hgrc``) so that Mercurial will automatically know where to pull changes from future updates. You can open the template config file in your editor by running ``hg config --edit`` or ``EDITOR=<editor-of-your-choice> hg config --edit``

.. code-block:: shell

               [paths]
               default = https://hg.mozilla.org/mozilla-central/

4. Update the repository to get all the changes since the bundle was created (this step also doubles as a check of the bundle integrity since if its contents are not exactly the same as what's in the official repository then the ``hg pull`` will fail):

.. code-block:: shell

               hg pull

5. Check out a working copy from your new up to date repository:

.. code-block:: shell

               hg update

You now have a clone of ``mozilla-central`` that is identical to one made via ``hg clone``. You can adjust your build settings, or you can go straight ahead and build Firefox!

If at any point you are stuck, feel free to ask on Riot/Matrix at `https://chat.mozilla.org <https://chat.mozilla.org>`__
in `#introduction <https://chat.mozilla.org/#/room/#introduction:mozilla.org>`__ channel.
