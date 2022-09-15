Firefox Branding
================

Firefox has multiple different sets of "branding" that are used to hold channel-specific things such as:
* Logos and other iconography
* Product names (eg: "Mozilla Firefox", "Firefox Developer Edition")
* Channel-specific preferences (eg: ``app.update.interval``)

Brandings are stored in the `branding subdirectory <https://searchfox.org/mozilla-central/source/browser/branding>`_ and map to builds as follows:

- ``official`` is used for Release and Beta builds
- ``aurora`` is used for Developer Edition builds
- ``nightly`` is used for Nightly and Try builds
- ``unofficial`` is used when no other branding is specified (eg: local developer builds)


Additional reading
------------------

.. toctree::

   UpdatingMacIcons
