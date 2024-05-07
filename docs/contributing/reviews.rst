Getting reviews
===============


Thorough code reviews are one of Mozilla's ways of ensuring code quality.
Every patch must be reviewed by the module owner of the code, or one of their designated peers.

Commit message syntax
---------------------

When submitting your commit(s), use the following syntax to request review for your patch:

.. list-table::
   :header-rows: 1

   * - Request
     - Syntax
     - Description
   * - Single reviewer
     - ``r=reviewer``
     - Request a review from a single reviewer (``reviewer``). Historically, the syntax ``r?reviewer`` requested a review, and ``r=reviewer`` marked the reviewer who accepted the patch before landing. Both can now be used interchangeably when requesting a review.
   * - Multiple reviewers
     - ``r=reviewer1,reviewer2``
     - Request reviews from *both* ``reviewer1`` and ``reviewer2``.
   * - Blocking review
     - ``r=reviewer!``
     - Request a *blocking* review from ``reviewer``. This means that ``reviewer`` *must* review the patch before it can be landed.
   * - Both optional and blocking reviews
     - ``r=reviewer1,reviewer2!``
     - Request a *blocking* review from ``reviewer2``, and an *optional* review from ``reviewer1``.
   * - Review groups
     - ``r=#review-group``
     - Request a review from members of ``#review-group``. A full list can be found below.
   * - Blocking review groups
     - ``r=#review-group!``
     - Request a blocking review from a review group. This will require *at least one member* of the group to approve before landing.

For example, the commit syntax to request review from group ``group-name`` or ``developer-nickname`` would be:

.. code-block::

     Bug xxxx - explain what you are doing and why r?#group-name

     or

     Bug xxxx - explain what you are doing and why r?developer-nickname

Reviews, and review groups, can be selected or adjusted after submission in the phabricator UI.

Sometimes when publishing a patch, groups will automatically be added as blocking reviewers due to the code being touched. In this case, you may want to check that the reviewers you requested review from are set to blocking as well.

Choosing reviewers
------------------

* If you have a mentor assigned on the bug you are fixing, the mentor can usually either also review or find a suitable reviewer on your behalf.
* If you do not have a mentor, see if your code fits into one of the review groups below, and request review from that group.
* Otherwise, try looking at the history of the file to see who has modified it recently. (For example, `hg log <modified-file>` or `git log <modified-file>`)
* Finally if you are still unable to identify someone, try asking in the `#introduction channel on Matrix <https://chat.mozilla.org/#/room/#introduction:mozilla.org>`_.


Getting attention
-----------------

Generally most reviews will happen within roughly a week. If, however, a reviewer doesn't respond within a week or so of the review request:

  * Contact the reviewer directly (either via e-mail or on Matrix).
  * Join developers on `Mozilla's Matrix server <https://chat.mozilla.org>`_, and ask if anyone knows why a review may be delayed. Please link to the bug too.
  * If the review is still not addressed, mail the reviewer directly, asking if/when they'll have time to review the patch, or might otherwise be able to review it.

Remember that reviewers are human too, and may have complex reasons that prevent them from reviewing your patch in a timely manner. Be confident in reaching out to your reviewer, but be mindful of the `Mozilla Community Participation Guidelines <https://www.mozilla.org/en-US/about/governance/policies/participation/>`_ while doing so.

For simple documentation changes, reviews are not required.

For more information about the review process, see the :ref:`Code Review FAQ`.

Review groups
-------------


.. list-table::
   :header-rows: 1

   * - Name
     - Owns
     - Members
   * - #anti-tracking
     - `Core: Anti-Tracking </mots/index.html#core-anti-tracking>`__
     - `Member list <https://phabricator.services.mozilla.com/project/members/157/>`__
   * - #build or #firefox-build-system-reviewers
     - The configure & build system
     - `Member list <https://phabricator.services.mozilla.com/project/members/20/>`__
   * - #cookies
     - `Core: Cookies </mots/index.html#core-cookies>`__
     - `Member list <https://phabricator.services.mozilla.com/project/members/177/>`__
   * - #desktop-theme-reviewers
     - User interface CSS
     - `Member list <https://phabricator.services.mozilla.com/project/members/141/>`__
   * - #devtools-reviewers
     - Firefox DevTools
     - `Member list <https://phabricator.services.mozilla.com/project/members/153/>`__
   * - #dom-workers-and-storage-reviewers
     - DOM Workers & Storage
     - `Member list <https://phabricator.services.mozilla.com/project/members/115/>`__
   * - #fluent-reviewers
     - Changes to Fluent (FTL) files (translation).
     - `Member list <https://phabricator.services.mozilla.com/project/members/105/>`__
   * - #firefox-source-docs-reviewers
     - Documentation files and its build
     - `Member list <https://phabricator.services.mozilla.com/project/members/118/>`__
   * - #firefox-ux-team
     - User experience (UX)
     - `Member list <https://phabricator.services.mozilla.com/project/members/91/>`__
   * - #firefox-svg-reviewers
     - SVG-related changes
     - `Member list <https://phabricator.services.mozilla.com/project/members/97/>`__
   * - #frontend-codestyle-reviewers
     - Changes to ESLint, Prettier or Stylelint configurations.
     - `Member list <https://phabricator.services.mozilla.com/project/members/208/>`__
   * - #android-reviewers
     - Changes to Fenix, Focus and Android Components.
     - `Member list <https://phabricator.services.mozilla.com/project/members/200/>`__
   * - #geckoview-reviewers
     - Changes to GeckoView
     - `Member list <https://phabricator.services.mozilla.com/project/members/92/>`__
   * - #gfx-reviewers
     - Changes to Graphics code
     - `Member list <https://phabricator.services.mozilla.com/project/members/122/>`__
   * - #webgpu-reviewers
     - Changes to WebGPU code
     - `Member list <https://phabricator.services.mozilla.com/project/members/170/>`__
   * - #intermittent-reviewers
     - Test manifest changes
     - `Member list <https://phabricator.services.mozilla.com/project/members/110/>`__
   * - #layout-reviewers
     - Layout changes.
     - `Member list <https://phabricator.services.mozilla.com/project/members/126/>`__
   * - #linter-reviewers
     - tools/lint/*
     - `Member list <https://phabricator.services.mozilla.com/project/members/119/>`__
   * - #mac-reviewers
     - Changes to Mac-specific code
     - `Member list <https://phabricator.services.mozilla.com/project/members/149/>`__
   * - #mozbase
     - Changes to Mozbase
     - `Member list <https://phabricator.services.mozilla.com/project/members/113/>`__
   * - #mozbase-rust
     - Changes to Mozbase in Rust
     - `Member list <https://phabricator.services.mozilla.com/project/members/114/>`__
   * - #necko-reviewers
     - Changes to network code (aka necko, aka netwerk)
     - `Member list <https://phabricator.services.mozilla.com/project/members/127/>`__
   * - #nss-reviewers
     - Changes to Network Security Services (NSS)
     - `Member list <https://phabricator.services.mozilla.com/project/members/156/>`__
   * - #perftest-reviewers
     - Perf Tests
     - `Member list <https://phabricator.services.mozilla.com/project/members/102/>`__
   * - #permissions or #permissions-reviewers
     - `Permissions </mots/index.html#core-permissions>`__
     - `Member list <https://phabricator.services.mozilla.com/project/members/158/>`__
   * - #places-reviewers
     - `Bookmarks & History (Places) </mots/index.html#bookmarks-history>`__
     - `Member list <https://phabricator.services.mozilla.com/project/members/186/>`__
   * - #platform-i18n-reviewers
     - Platform Internationalization
     - `Member list <https://phabricator.services.mozilla.com/project/members/150/>`__
   * - #preferences-reviewers
     - Firefox for Desktop Preferences (Options) user interface
     - `Member list <https://phabricator.services.mozilla.com/project/members/132/>`__
   * - #remote-debugging-reviewers
     - Remote Debugging UI & tools
     - `Member list <https://phabricator.services.mozilla.com/project/members/108/>`__
   * - #search-reviewers
     - Search Reviewers (search parts of `Search and Address Bar </mots/index.html#search-and-address-bar>`__)
     - `Member list <https://phabricator.services.mozilla.com/project/members/169/>`__
   * - #spidermonkey-reviewers
     - SpiderMonkey JS/Wasm Engine
     - `Member list <https://phabricator.services.mozilla.com/project/members/173/>`__
   * - #static-analysis-reviewers
     - Changes related to Static Analysis
     - `Member list <https://phabricator.services.mozilla.com/project/members/120/>`__
   * - #style or #firefox-style-system-reviewers
     - Firefox style system (servo, layout/style).
     - `Member list <https://phabricator.services.mozilla.com/project/members/90/>`__
   * - #supply-chain-reviewers
     - Changes to third-party audits and vendoring (cargo-vet, supply_chain).
     - `Member list <https://phabricator.services.mozilla.com/project/members/164/>`__
   * - #webcompat-reviewers
     - System addons maintained by the Web Compatibility team
     - `Member list <https://phabricator.services.mozilla.com/project/members/124/>`__
   * - #webdriver-reviewers
     - Marionette and geckodriver (including MozBase Rust), and Remote Protocol with WebDriver BiDi, and CDP.
     - `Member list <https://phabricator.services.mozilla.com/project/members/103/>`__
   * - #webidl
     - Changes related to WebIDL
     - `Member list <https://phabricator.services.mozilla.com/project/members/112/>`__
   * - #xpcom-reviewers
     - Changes related to XPCOM
     - `Member list <https://phabricator.services.mozilla.com/project/members/125/>`__
   * - #media-playback-reviewers
     - `Media playback <https://wiki.mozilla.org/Modules/All#Media_Playback>`__
     - `Member list <https://phabricator.services.mozilla.com/project/profile/159/>`__
   * - #cubeb-reviewers
     - Changes related to cubeb, Gecko's audio input/output library and associated projects (audioipc, cubeb-rs, rust cubeb backends)
     - `Member list <https://phabricator.services.mozilla.com/project/profile/129/>`__

To create a new group, fill a `new bug in Conduit::Administration <https://bugzilla.mozilla.org/enter_bug.cgi?product=Conduit&component=Administration>`__.
See `bug 1613306 <https://bugzilla.mozilla.org/show_bug.cgi?id=1613306>`__ as example.
