Enterprise Policies
===================

Introduction
------------

Firefox provides policies to manage various aspects of Firefox. The best documentation is in `GitHub <https://github.com/mozilla/policy-templates/>`_.

Kiosk Mode
----------

The kiosk mode provided by Firefox on the command line (--kiosk) is a very basic mode intended for kiosks where the content loaded in the browser is strictly limited by the owner of the kiosk and either there is no keyboard or keyboard access is limited (particularly Ctrl and Alt). It is their responsibility to ensure the content does not surprise/confuse users or break browser UI in this setup.

It does three main things:

1. Switch all main browser windows (not popup windows) into full screen mode.
2. Don't show the context menu.
3. Don't show status for URLs or pageloading.
